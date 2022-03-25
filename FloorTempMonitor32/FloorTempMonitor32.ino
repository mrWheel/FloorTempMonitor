/*
**  Program   : FloorTempMonitor
*/
#define _FW_VERSION "v0.8.1 (12-11-2019)"

/*
**  Copyright (c) 2019 Willem Aandewiel / Erik Meinders
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
  Arduino-IDE settings for this program:

    - Board: "ESP32 Dev Module"
    - Upload Speed: "921600"
    - CPU Frequency: "240MHz (WiFi/BT)"
    - Flash Frequency: "80MHz"
    - Flash mode: "QIO"    
    - Flash size: "4MB (32Mb)"  
    - Partition Scheme: "Default 4MB with spiffs (1.2MB APP/ 1.5MB SPIFFS)"
    - Core Debug Level: "None"
    - PSRAM: "Disabled"
    - DebugT port: "Disabled"
*/
#define _HOSTNAME "FloorTempMonitor"
/******************** compiler options  ********************************************/
#define USE_NTP_TIME
// #define USE_UPDATE_SERVER
// #define USE_WIFIMANAGER
// #define HAS_FSEXPLORER
#define SHOW_PASSWRDS

#define PROFILING             // comment this line out if you want not profiling 
#define PROFILING_THRESHOLD 45 // defaults to 3ms - don't show any output when duration below TH

// BENCHMARK:==(TESTDATA)========================================
//  ESP8266 :   did 3630 iterations of loop() in 300 seconds
//    ESP32 :   did 3670 iterations of loop() in 300 seconds
// BENCHMARK:====================================================


#define TESTDATA

#ifndef USE_WIFIMANAGER
  // Replace with your network credentials
  const char* ssid      = "********";   //  "REPLACE_WITH_YOUR_SSID";
  const char* password  = "*********";  //  "REPLACE_WITH_YOUR_PASSWORD";
#endif
/******************** don't change anything below this comment **********************/

#include <Timezone.h>           // https://github.com/JChristensen/Timezone
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include "Debug.h"
#include "timing.h"
#include "networkStuff.h"
#include "FloorTempMonitor32.h"
#if defined(ESP8266)
  #include <FS.h>                 // part of ESP8266 Core https://github.com/esp8266/Arduino
#else
  #include <SPIFFS.h>
#endif
#include <OneWire.h>            // https://github.com/PaulStoffregen/OneWire
#include <DallasTemperature.h>  // https://github.com/milesburton/Arduino-Temperature-Control-Library

#define _SA                   sensorArray
#define _PULSE_TIME           (uint32_t)sensorArray[0].deltaTemp
#define HEATER_ON_TEMP        47.0
#if defined(ESP8266)
  #define LED_BUILTIN_ON      LOW
  #define LED_BUILTIN_OFF     HIGH
#else
  #define LED_BUILTIN         2     // not defined for ESP32???
  #define LED_BUILTIN_ON      HIGH
  #define LED_BUILTIN_OFF     LOW
#endif
#define LED_WHITE             14      // D5
#define LED_RED               12      // D6
#define LED_GREEN             13      // D7 - ONLY USE FOR HEARTBEAT!!
#define LED_ON                HIGH
#define LED_OFF               LOW

#define _PLOT_INTERVAL        120    // in seconds

DECLARE_TIMER(graphUpdate, _PLOT_INTERVAL)

DECLARE_TIMER(heartBeat,     3) // flash LED_GREEN 

DECLARE_TIMERm(sensorPoll,   1) // update sensors every 20s 

DECLARE_TIMERm(UptimeDisplay,1)

/*********************************************************************************
* Uitgangspunten:
* 
* S0 - Sensor op position '0' is verwarmingsketel water UIT (Flux In)
*      deze sensor moet een servoNr '-1' hebben -> want geen klep.
*      De 'deltaTemp' van deze sensor wordt gebruikt om een eenmaal
*      gesloten Servo/Klep dicht te houden (als tijd dus!).
* S1 - Sensor op position '1' is (Flux) retour naar verwarmingsketel
*      servoNr '-1' -> want geen klep
* 
* Cyclus voor bijv S3:
*                                                start _REFLOW_TIME
*   (S0.tempC - S3.tempC) < S3.deltaTemp         ^        einde _REFLOW_TIME
*                                      v         |        v
*  - servo/Klep open            -------+         +--------+------> wacht tot (S0.tempC - S3.tempC) < S3.deltaTemp 
*                                      |         |
*  - Servo/Klep closed                 +---------+
*                                      ^         ^
*                                      |         |
*   start S0.deltaTemp (_PULSE_TIME) --/         |                       
*       S0.deltaTemp (_PULSE_TIME) verstreken ---/
* 
*********************************************************************************/

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device addresses
DeviceAddress DS18B20;

// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};   // Central European Summer Time
TimeChangeRule CET  = {"CET ", Last, Sun, Oct, 3, 60};    // Central European Standard Time
Timezone CE(CEST, CET);
TimeChangeRule *tcr;         // pointer to the time change rule, use to get TZ abbrev

const char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

sensorStruct  sensorArray[_MAX_SENSORS];
dataStruct    dataStore[_MAX_DATAPOINTS+1];
servoStruct   servoArray[_MAX_SERVOS];

char      cMsg[150];
String    pTimestamp;
int8_t    prevNtpHour         = 0;
uint32_t  startTimeNow, startLoopTimer;
uint16_t  loopCounter = 0;

int8_t    noSensors;
int8_t    cycleNr             = 0;
int8_t    lastSaveHour        = 0;
uint8_t   connectionMuxLostCount    = 0;
bool      SPIFFSmounted       = false;
bool      cycleAllSensors     = false;



//===========================================================================================
String upTime()
{
  char    calcUptime[20];
  uint32_t  upTimeNow = now() - startTimeNow; // upTimeNow = seconds

  sprintf(calcUptime, "%d[d] %02d:%02d" 
          , (int)((upTimeNow / (60 * 60 * 24)) % 365)
          , (int)((upTimeNow / (60 * 60)) % 24) // hours
          , (int)((upTimeNow / 60) % 60));       // minuten
  return calcUptime;

} // upTime()


//===========================================================================================
void handleHeartBeat()
{
  if ( DUE (heartBeat) ) {
    digitalWrite(LED_GREEN, LED_ON); 
  } 
  
  if ( SINCE(heartBeat) > 1000) {
    digitalWrite(LED_GREEN, LED_OFF);
  }

  if ( DUE (UptimeDisplay))
    DebugTf("Running %s\n",upTime().c_str());
} 


//===========================================================================================
void handleReBoot(AsyncWebServerRequest *request)
{
  String redirectHTML = "";

  redirectHTML += "<!DOCTYPE HTML><html lang='en-US'>";
  redirectHTML += "<head>";
  redirectHTML += "<meta charset='UTF-8'>";
  redirectHTML += "<style type='text/css'>";
  redirectHTML += "body {background-color: lightblue;}";
  redirectHTML += "</style>";
  redirectHTML += "<title>Redirect to "+String(_HOSTNAME)+"</title>";
  redirectHTML += "</head>";
  redirectHTML += "<body><h1>"+String(_HOSTNAME)+" - FSexplorer</h1>";
  redirectHTML += "<h3>Rebooting "+String(_HOSTNAME)+"</h3>";
  redirectHTML += "<br><div style='width: 500px; position: relative; font-size: 25px;'>";
  redirectHTML += "  <div style='float: left;'>Redirect over &nbsp;</div>";
  redirectHTML += "  <div style='float: left;' id='counter'>15</div>";
  redirectHTML += "  <div style='float: left;'>&nbsp; seconden ...</div>";
  redirectHTML += "  <div style='float: right;'>&nbsp;</div>";
  redirectHTML += "</div>";
  redirectHTML += "<!-- Note: don't tell people to `click` the link, just tell them that it is a link. -->";
  redirectHTML += "<br><br><hr>If you are not redirected automatically, click this <a href='/'>"+String(_HOSTNAME)+"</a>.";
  redirectHTML += "  <script>";
  redirectHTML += "      setInterval(function() {";
  redirectHTML += "          var div = document.querySelector('#counter');";
  redirectHTML += "          var count = div.textContent * 1 - 1;";
  redirectHTML += "          div.textContent = count;";
  redirectHTML += "          if (count <= 0) {";
  redirectHTML += "              window.location.replace('/'); ";
  redirectHTML += "          } ";
  redirectHTML += "      }, 1000); ";
  redirectHTML += "  </script> ";
  redirectHTML += "</body></html>\r\n";

  request->send(200, "text/html", redirectHTML);

  DebugTf("ReBoot %s ..\r\n", _HOSTNAME);
  writeDataPoints();
  TelnetStream.flush();
  delay(1000);
  ESP.restart();

} // handleReBoot()


//===========================================================================================
void notFound(AsyncWebServerRequest *request) 
{
    request->send(404, "text/plain", "Not found");
}

//===========================================================================================
void setup()
{
  Serial.begin(115200);
  Debugln("\nBooting ... \n");
  Debugf("[%s] %s  compiled [%s %s]\n", _HOSTNAME, _FW_VERSION, __DATE__, __TIME__);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LED_ON);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LED_ON);
  pinMode(LED_WHITE, OUTPUT);
  digitalWrite(LED_WHITE, LED_ON);

#ifdef IUSE_WIFIMANAGER
  startWiFi();
#else
// Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    DebugTln("Connecting to WiFi..");
  }
#endif

  digitalWrite(LED_WHITE, LED_OFF);
  startTelnet();

  Debug("Gebruik 'telnet ");
  Debug(WiFi.localIP());
  Debugln("' voor verdere debugging");
  digitalWrite(LED_RED, LED_OFF);

  startMDNS(_HOSTNAME);

  ntpInit();
  startTimeNow = now();

  for (int I = 0; I < 20; I++) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
  }

  SPIFFSmounted = SPIFFS.begin();
  DebugTf("SPIFFS Mount = %s \n\r",SPIFFSmounted ? "true" : "false");   // Serious problem with SPIFFS
  
//httpServer.serveStatic("/",                 SPIFFS, "/index.html");
 // Route for root / web page
//httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  httpServer.on("/", [](AsyncWebServerRequest *request){
    //request->send(SPIFFS, "/index.html", String(), false, processor);
    request->send(SPIFFS, "/index.html", "text/html");
  });
  httpServer.on("/index.html", [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
//httpServer.serveStatic("/api.js",            SPIFFS, "/api.js");
  httpServer.on("/api.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/api.js", "text/javascript");
  });
//httpServer.serveStatic("/room.js",           SPIFFS, "/room.js");
  httpServer.on("/room.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/room.js", "text/javascript");
  });
//httpServer.serveStatic("/gauge.css",         SPIFFS, "/gauge.css");
  httpServer.on("/gauge.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/gauge.css", "text/css");
  });
//httpServer.serveStatic("/index.html",       SPIFFS, "/index.html");
//httpServer.serveStatic("/FSexplorer.png",   SPIFFS, "/FSexplorer.png");
//httpServer.serveStatic("/favicon.ico",      SPIFFS, "/favicon.ico");
//httpServer.serveStatic("/favicon-32x32.png",SPIFFS, "/favicon-32x32.png");
//httpServer.serveStatic("/favicon-16x16.png",SPIFFS, "/favicon-16x16.png");
  
//httpServer.on("/ReBoot", handleReBoot);
  httpServer.on("/ReBoot", [](AsyncWebServerRequest *request){
    //request->send(200, "text/plain", "ReBoot");
    handleReBoot(request);
  });
    
  apiInit();
  
  httpServer.begin(); // before .ons

#if defined (HAS_FSEXPLORER)
  httpServer.on("/FSexplorer", HTTP_POST, handleFileDelete);
  httpServer.on("/FSexplorer", handleRoot);
  httpServer.on("/FSexplorer/upload", HTTP_POST, []() {
    httpServer.send(200, "text/plain", "");
  }, handleFileUpload);
#endif
  //httpServer.onNotFound([]() {
  httpServer.onNotFound(notFound);
    /******
    if (httpServer.uri() == "/update") {
      httpServer.send(200, "text/html", "/update" );
    } else {
      DebugTf("onNotFound(%s)\r\n", httpServer.uri().c_str());
    }
    if (!handleFileRead(httpServer.uri())) {
      //httpServer.send(404, "text/plain", "FileNotFound");
      request->send(404, "text/plain", "FileNotFound");
    //}
  });
  *****/
  DebugTln( "HTTP server gestart\r" );
  digitalWrite(LED_GREEN, LED_OFF);

#if defined(ESP8266)
  sprintf(cMsg, "Last reset reason: [%s]\r", ESP.getResetReason().c_str());
  DebugTln(cMsg);
#endif

  //--- Start up the library for the DS18B20 sensors
  sensors.begin();

  delay(5000); // time to start telnet
  setupDallasSensors();

#ifdef TESTDATA                                       // TESTDATA
  noSensors = 11;                                              // TESTDATA
  for (int s=0; s < noSensors; s++) {                          // TESTDATA
    sprintf(_SA[s].name, "Sensor %d", (s+2));                  // TESTDATA
    sprintf(_SA[s].sensorID, "0x2800ab0000%02x", (s*3) + 3);   // TESTDATA
    _SA[s].servoNr     =  (s+1);                               // TESTDATA
    _SA[s].position    = s+2;                                  // TESTDATA
  //_SA[s].closeCount  =  0;                                   // TESTDATA
    _SA[s].deltaTemp   = 15 + s;                               // TESTDATA
    _SA[s].tempFactor  = 1.0;                                  // TESTDATA
  //_SA[s].servoTimer  = millis();                             // TESTDATA
  }                                                            // TESTDATA
  sprintf(_SA[5].name, "*Flux IN");                            // TESTDATA
  _SA[5].position      =  0;                                   // TESTDATA
  _SA[5].deltaTemp     =  2; // is _PULSE_TIME                 // TESTDATA
  _SA[5].servoNr       = -1;                                   // TESTDATA
  sprintf(_SA[6].name, "*Flux RETOUR");                        // TESTDATA
  _SA[6].position      =  1;                                   // TESTDATA
  _SA[6].deltaTemp     =  0;                                   // TESTDATA
  _SA[6].servoNr       = -1;                                   // TESTDATA
  sprintf(_SA[7].name, "*Room Temp.");                         // TESTDATA
  _SA[7].servoNr       = -1;                                   // TESTDATA
#endif                                                // TESTDATA

  Debugln("========================================================================================");
  sortSensors();
  printSensorArray();
  Debugln("========================================================================================");

  //readDataPoints();

  servoInit();
  setupI2C_Mux();

  String DT = buildDateTimeString();
  DebugTf("Startup complete! @[%s]\r\n\n", DT.c_str());
  
  roomsInit();
  
  startLoopTimer  = millis();
  loopCounter     = 0;

} // setup()

//===========================================================================================
void erix()
{
  DebugTf("Aangeroepen!\n");

}
int8_t fnindex=0;

//===========================================================================================
void loop()
{
  loopCounter++;
  if ((millis() - startLoopTimer) > (60 * 5 * 1000)) {
    DebugTln("====================================================");
    DebugTf("did %d iterations of loop() in %d seconds\n", loopCounter, (60 * 5));
    DebugTln("====================================================");
    startLoopTimer  = millis();
    loopCounter     = 0;
  }
  timeThis( handleHeartBeat() );       // blink GREEN led

#if defined(ESP8266)
  timeThis( MDNS.update() );
#endif
  //timeThis( httpServer.handleClient() );
  timeThis( handleNTP() );

  timeThis( checkI2C_Mux() );         //  call setupI2C_Mux() 
  timeThis( handleSensors() );        // update return water temperature information
  
  timeThis( checkI2C_Mux() );         // extra call to handleMUX as handle Sensors may take a long time ..
  timeThis( checkDeltaTemps() );      // check for hotter than wanted return water temperatures
  
  timeThis( checkI2C_Mux() );         // extra call to handleMUX as handle Sensors may take a long time ..
  timeThis( handleRoomTemps() );      // check room temperatures and operate servos when necessary

  timeThis( checkI2C_Mux() );         // extra call to handleMUX as handle Sensors may take a long time ..
  timeThis( handleDatapoints() );     // update datapoint for trends

  timeThis( handleCycleServos() );    // ensure servos are cycled daily (and not all at once?)

  timeThis( servosAlign() );

} 
