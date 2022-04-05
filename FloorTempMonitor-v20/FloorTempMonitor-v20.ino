/*
**  Program   : FloorTempMonitor
*/
#define _FW_VERSION "v2.0.0 (05-04-2022)"

/*
**  Copyright 2020, 2021, 2022 Willem Aandewiel / Erik Meinders
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
  Use Boards Manager to install Arduino ESP32 core 2.0.2 
   
  Arduino-IDE settings for FloorTempMonitor:

    - Board: "ESP32 Wrover Module"
    - Upload Speed: "115200"  -  "921600"                                                                                                                                                                                                                                               
    - Flash Frequency: "80 MHz"
    - Flash mode: "QIO" | "DOUT" | "DIO"    // if you change from one to the other OTA may fail!
    - Partition Scheme: "Default 4MB with spiffs (1.2MB APP/1.5MB FS)"
    - Core Debug Level: "None"
    - Port: <select correct port>

Using library WiFi at version 2.0.0               (part of Arduino ESP32 Core @2.0.2)
Using library WebServer at version 2.0.0          (part of Arduino ESP32 Core @2.0.2)
Using library ESPmDNS at version 2.0.0            (part of Arduino ESP32 Core @2.0.2) 
Using library Update at version 2.0.0             (part of Arduino ESP32 Core @2.0.2) 
Using library DNSServer at version 2.0.0          (part of Arduino ESP32 Core @2.0.2)
Using library EEPROM at version 2.0.0             (part of Arduino ESP32 Core @2.0.2)
Using library FS at version 2.0.0                 (part of Arduino ESP32 Core @2.0.2)
Using library LittleFS at version 2.0.0           (part of Arduino ESP32 Core @2.0.2)
Using library HTTPClient at version 2.0.0         (part of Arduino ESP32 Core @2.0.2)
Using library WiFiClientSecure at version 2.0.0   (part of Arduino ESP32 Core @2.0.2)
Using library Wire at version 2.0.0               (part of Arduino ESP32 Core @2.0.2)
Using library TelnetStream at version 1.2.2      in folder: ~libraries/TelnetStream 
Using library WiFiManager at version 2.0.10-beta  in folder: ~libraries/WiFiManager 
Using library esp-knx-ip-master at version 0.4   in folder: ~libraries/esp-knx-ip-master 
Using library OneWire at version 2.3.6           in folder: ~libraries/OneWire 
Using library DallasTemperature at version 3.8.0 in folder: ~libraries/DallasTemperature 
Using library ArduinoJson at version 6.19.3      in folder: ~libraries/ArduinoJson 

*/


#define _HOSTNAME "FloorTempMonitor"
/******************** compiler options  ********************************************/
#define USE_NTP_TIME
#define USE_UPDATE_SERVER
#define HAS_FSMANAGER
#define SHOW_PASSWRDS

#define PROFILING             // comment this line out if you want not profiling 
#define PROFILING_THRESHOLD 45 // defaults to 3ms - don't show any output when duration below TH

//  #define TESTDATA
/******************** don't change anything below this comment **********************/

void handleHeartBeat();  //-- proto for Debug.h

#include <time.h>
#include "time_zones.h"
#include "Debug.h"
#include "timing.h"
#include "networkStuff.h"

//-- https://github.com/c-o-m-m-a-n-d-e-r/esp-knx-ip
//-- modified version for ESP32
//-- AaW changed line 267
//-- from "cemi_addi_t additional_info[];"
//-- to   "cemi_addi_t *additional_info;"
#include <esp-knx-ip.h>         // v0.4 - https://github.com/c-o-m-m-a-n-d-e-r/esp-knx-ip

#include "FloorTempMonitor.h"
#include "FTMConfig.h"
#include <FS.h>                 // v2.0.0 (part of Arduino Core ESP32 @2.0.2)
//-- you need to install this plugin: https://github.com/lorol/arduino-esp32littlefs-plugin
#include <LittleFS.h>           // v2.0.0 (part of Arduino Core ESP32 @2.0.2)
#include <OneWire.h>            // v2.3.6
#include <DallasTemperature.h>  // v3.8.0  https://github.com/milesburton/Arduino-Temperature-Control-Library

#define _SA                   sensorArray
#define _PULSE_TIME           (uint32_t)sensorArray[0].deltaTemp
#define HEARTBEAT_RELAY       16

#define PIN_HARTBEAT           4  //-- pulse @ 0.5-1.0 Hz
#ifndef LED_BUILTIN
  #define LED_BUILTIN         2  //-- blue LED
#endif
#define LED_WHITE             16
#define LED_GREEN             17
#define LED_RED               18
#define LED_ON                LOW
#define LED_OFF               HIGH
#define _SDA                  21
#define _SCL                  22

DECLARE_TIMER(heartBeat,        3)  //-- fire every 2 seconds 

DECLARE_TIMERm(heartBeatRelay,  1)  //-- fire every minute 

DECLARE_TIMERm(sensorPoll,      1)  //-- fire every minute

DECLARE_TIMERm(UptimeDisplay,   5)  //-- fire every five minutes

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

const char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

const char *ntpServer  = "pool.ntp.org";
const char *TzLocation = "Europe/Amsterdam";

sensorStruct  sensorArray[_MAX_SENSORS];
servoStruct   servoArray[_MAX_SERVOS];

char      cMsg[150];
tm        timeInfo;
String    pTimestamp;
int8_t    prevNtpHour         = 0;
uint32_t  startTimeNow;

int8_t    noSensors;
int8_t    cycleNr             = 0;
int8_t    lastSaveHour        = 0;
uint8_t   connectionMuxLostCount    = 0;
bool      LittleFSmounted       = false;
bool      cycleAllSensors     = false;


#include <rom/rtc.h>      // low-level 'C'???

//===========================================================================================
void getLastResetReason(RESET_REASON reason, char *txtReason, int txtReasonLen)
{
  switch ( reason)
  {
    case  1 : snprintf(txtReason, txtReasonLen, "[%d] POWERON_RESET", reason);    break; // < 1, Vbat power on reset
    case  3 : snprintf(txtReason, txtReasonLen, "[%d] SW_RESET", reason);         break; // < 3, Software reset digital core
    case  4 : snprintf(txtReason, txtReasonLen, "[%d] OWDT_RESET", reason);       break; // < 4, Legacy watch dog reset digital core
    case  5 : snprintf(txtReason, txtReasonLen, "[%d] DEEPSLEEP_RESET", reason);  break; // < 5, Deep Sleep reset digital core
    case  6 : snprintf(txtReason, txtReasonLen, "[%d] SDIO_RESET", reason);       break; // < 6, Reset by SLC module, reset digital core
    case  7 : snprintf(txtReason, txtReasonLen, "[%d] TG0WDT_SYS_RESET", reason); break; // < 7, Timer Group0 Watch dog reset digital core
    case  8 : snprintf(txtReason, txtReasonLen, "[%d] TG1WDT_SYS_RESET", reason); break; // < 8, Timer Group1 Watch dog reset digital core
    case  9 : snprintf(txtReason, txtReasonLen, "[%d] RTCWDT_SYS_RESET", reason); break; // < 9, RTC Watch dog Reset digital core
    case 10 : snprintf(txtReason, txtReasonLen, "[%d] INTRUSION_RESET", reason);  break; // <10, Instrusion tested to reset CPU
    case 11 : snprintf(txtReason, txtReasonLen, "[%d] TGWDT_CPU_RESET", reason);  break; // <11, Time Group reset CPU
    case 12 : snprintf(txtReason, txtReasonLen, "[%d] SW_CPU_RESET", reason);     break; // <12, Software reset CPU
    case 13 : snprintf(txtReason, txtReasonLen, "[%d] RTCWDT_CPU_RESET", reason); break; // <13, RTC Watch dog Reset CPU
    case 14 : snprintf(txtReason, txtReasonLen, "[%d] EXT_CPU_RESET", reason);    break; // <14, for APP CPU, reseted by PRO CPU
    case 15 : snprintf(txtReason, txtReasonLen, "[%d] RTCWDT_BROWN_OUT_RESET", reason); break;// <15, Reset when the vdd voltage is not stable
    case 16 : snprintf(txtReason, txtReasonLen, "[%d] RTCWDT_RTC_RESET", reason); break; // <16, RTC Watch dog reset digital core and rtc module
    default : snprintf(txtReason, txtReasonLen, "[%d] NO_MEAN", reason);
  }
  
} //  getLastResetReason()  


//===========================================================================================
char * upTime()
{
  static char calcUptime[20];
  uint32_t  upTimeNow = (millis()/1000) - startTimeNow; // upTimeNow = seconds

  sprintf(calcUptime, "%d[d] %02d:%02d" 
          , (int)((upTimeNow / (60 * 60 * 24)) % 365)
          , (int)((upTimeNow / (60 * 60)) % 24) // hours
          , (int)((upTimeNow / 60) % 60));       // minuten
  return calcUptime;

} // upTime()


//===========================================================================================
void handleHeartBeat()
{
  if ( DUE (heartBeat) ) 
  {
    digitalWrite(PIN_HARTBEAT, LOW); 
  } 
  
  if ( SINCE(heartBeat) > 500) //-- milliSeconds?
  {
    digitalWrite(PIN_HARTBEAT, HIGH);
  }

  if ( DUE (UptimeDisplay))
  {
    DebugTf("Running %s\n",upTime());
  }
  
} //  handleHeartBeat()


//===========================================================================================
void setup()
{
  Serial.begin(115200);
  Debugln("\nBooting ... \n");
  Debugf("[%s] %s  compiled [%s %s]\n", _HOSTNAME, _FW_VERSION, __DATE__, __TIME__);

  //-aaw32- startTimeNow = now();
  startTimeNow = millis() / 1000;

  pinMode(PIN_HARTBEAT, OUTPUT);
  handleHeartBeat();
  pinMode(LED_BUILTIN,  OUTPUT);
  pinMode(LED_RED,      OUTPUT);
  digitalWrite(LED_RED, LED_ON);
  pinMode(LED_GREEN,    OUTPUT);
  digitalWrite(LED_GREEN, LED_OFF);
  pinMode(LED_WHITE,    OUTPUT);
  digitalWrite(LED_WHITE, LED_OFF);

  startWiFi();
  handleHeartBeat();
  digitalWrite(LED_RED, LED_OFF);
  digitalWrite(LED_WHITE, LED_ON);

  configTime(0, 0, ntpServer);
  //-- Set environment variable with your time zone
  setenv("TZ", getTzByLocation(TzLocation).c_str(), 1);
  tzset();
  printLocalTime();
  handleHeartBeat();

  startTelnet();

  DebugT("Gebruik 'telnet ");
  Debug(WiFi.localIP());
  Debugln("' (port 23) voor verdere debugging");
  
  digitalWrite(LED_WHITE, LED_ON);
  startMDNS(_HOSTNAME);
  handleHeartBeat();
  digitalWrite(LED_WHITE, LED_OFF);

  //ntpInit();
  for (int I = 0; I < 10; I++) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
  }

  if (LittleFS.begin())
        LittleFSmounted = true;
  else  LittleFSmounted = false;

  httpServer.serveStatic("/",                 LittleFS, "/index.html");
  httpServer.serveStatic("/index.html",       LittleFS, "/index.html");
  httpServer.serveStatic("/ftm.js",           LittleFS, "/ftm.js");
  httpServer.serveStatic("/ftm.css",          LittleFS, "/ftm.css");
  httpServer.serveStatic("/floortempmon.js",  LittleFS, "/floortempmon.js");
  httpServer.serveStatic("/sensorEdit.html",  LittleFS, "/index.html");
  httpServer.serveStatic("/sensorEdit.js",    LittleFS, "/sensorEdit.js");
  httpServer.serveStatic("/favicon.ico",      LittleFS, "/favicon.ico");

  setupFSmanager();
  handleHeartBeat();

  httpServer.begin();

  apiInit();
  handleHeartBeat();
   
  DebugTln( "HTTP server gestart\r" );
  digitalWrite(LED_GREEN, LED_ON);
  
  //-aaw32-sprintf(cMsg, "Last reset reason: [%s]\r", ESP.getResetReason().c_str());
  getLastResetReason(rtc_get_reset_reason(0), cMsg, sizeof(cMsg));
  DebugTf("last Reset Reason CPU[0] [%s]\r\n",cMsg);
  getLastResetReason(rtc_get_reset_reason(1), cMsg, sizeof(cMsg));
  DebugTf("last Reset Reason CPU[1] [%s]\r\n",cMsg);

  //--- Start up the library for the DS18B20 sensors
  sensors.begin();
  handleHeartBeat();

  delay(5000); // time to start telnet
  sensorsInit();
  handleHeartBeat();

#ifdef TESTDATA                                       // TESTDATA
  noSensors = 11;                                              // TESTDATA
  for (int s=0; s < noSensors; s++) {                          // TESTDATA
    sprintf(_SA[s].name, "Sensor %d", (s+2));                  // TESTDATA
    sprintf(_SA[s].sensorID, "0x2800ab0000%02x", (s*3) + 3);   // TESTDATA
    _SA[s].servoNr     =  (s+1);                               // TESTDATA
    _SA[s].deltaTemp   = 15 + s;                               // TESTDATA
    _SA[s].tempFactor  = 1.0;                                  // TESTDATA
    _SA[s].tempOffset  = 0.01;                                 // TESTDATA
    _SA[s].tempC       = 18.01;                                // TESTDATA
    /**
  int8_t    sensorIndex;          // index on CB bus NOT index in _SA array!
  char      sensorID[20];
  char      name[_MAX_NAME_LEN];
  float     tempOffset;     // calibration
  float     tempFactor;     // calibration
  int8_t    servoNr;        // index in servoArray
  float     deltaTemp;      //-- in S0 -> closeTime
  float     tempC;          //-- not in sensors.ini
  **/

  }                                                            // TESTDATA
  sprintf(_SA[0].name, "*Flux IN");                            // TESTDATA
  _SA[0].deltaTemp     =  2; // is _PULSE_TIME                 // TESTDATA
  _SA[0].servoNr       = -1;                                   // TESTDATA
  sprintf(_SA[1].name, "*Flux RETOUR");                        // TESTDATA
  _SA[1].deltaTemp     =  0;                                   // TESTDATA
  _SA[1].servoNr       = -1;                                   // TESTDATA
  sprintf(_SA[7].name, "*Room Temp.");                         // TESTDATA
  _SA[7].servoNr       = -1;                                   // TESTDATA
#endif                                                // TESTDATA

  Debugln("========================================================================================");
  printSensorArray();
  Debugln("========================================================================================");
  handleHeartBeat();

  setupI2C_Mux();
  servosInit();
  roomsInit();
  myKNX_init(&httpServer);
  handleHeartBeat();

#if defined (USE_NTP_TIME)
  //-aaw32- String DT = buildDateTimeString();

  if (getLocalTime(&timeInfo))
  {
    DebugTf("Startup complete! %02d-%02d-20%02d %02d:%02d:%02d\r\n", timeInfo.tm_mday, timeInfo.tm_mon,
                                          timeInfo.tm_year, hour(), minute(), second());
  }
#endif 
  DebugTf("Startup complete! (took[%d]seconds)\r\n\n", millis()/1000);

} // setup()


//===========================================================================================
void loop()
{
  handleHeartBeat();       // toggle PIN_HARTBEAT
  
  timeThis( httpServer.handleClient() );
  timeThis( handleNTP() );

  checkI2C_Mux();         // check I2C_Mux communication with servo board

  timeThis( sensorsLoop() );          // update return water temperature information
  
  timeThis( servosLoop() );           // check for hotter than wanted return water temperatures
  
  timeThis( roomsLoop() );            // check room temperatures and operate servos when necessary

  timeThis( handleCycleServos() );    // ensure servos are cycled daily (and not all at once?)

  timeThis( servosAlign() );          // re-align servos after servo-board reset

  timeThis( myKNX_loop() );           // allign KNX bus and respond to queries over KNX

  timeThis( updateBoiler() );

} 
