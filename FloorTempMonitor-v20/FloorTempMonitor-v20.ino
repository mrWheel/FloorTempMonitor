/*
**  Program   : FloorTempMonitor
*/
#define _FW_VERSION "v2.0.0 (25-03-2022)"

/*
**  Copyright 2020, 2021, 2022 Willem Aandewiel / Erik Meinders
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
  Use Boards Manager to install Arduino ESP8266 core 2.6.3  (https://github.com/esp8266/Arduino/releases)
  
  Arduino-IDE settings for FloorTempMonitor:

    - Board: "ESP32 Wrover Module"
    - Upload Speed: "115200"  -  "921600"                                                                                                                                                                                                                                               
    - Flash Frequency: "80 MHz"
    - Flash mode: "QIO" | "DOUT" | "DIO"    // if you change from one to the other OTA may fail!
    - Partition Scheme: "Default 4MB with spiffs (1.2MB APP/1.5MB FS)"
    - Core Debug Level: "None"
    - Port: <select correct port>
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

#include <Timezone.h>           // v 1.2.4 https://github.com/JChristensen/Timezone
#include <TimeLib.h>            // https://github.com/PaulStoffregen/Time
#include "Debug.h"
#include "timing.h"
#include "networkStuff.h"

//-- https://github.com/c-o-m-m-a-n-d-e-r/esp-knx-ip
//-- modified version for ESP32
//-- AaW changed line 267
//-- from "cemi_addi_t additional_info[];"
//-- to   "cemi_addi_t *additional_info;"
#include <esp-knx-ip.h>

#include "FloorTempMonitor.h"
#include "FTMConfig.h"
#include <FS.h>
//-- you need to install this plugin: https://github.com/lorol/arduino-esp32littlefs-plugin
#include <LittleFS.h>
#include <OneWire.h>            // Versie 2.3.6
#include <DallasTemperature.h>  // https://github.com/milesburton/Arduino-Temperature-Control-Library

#define _SA                   sensorArray
#define _PULSE_TIME           (uint32_t)sensorArray[0].deltaTemp

#define PIN_HARTBEAT           4  //-- pulse @ 0.5-1.0 Hz
#define LED_BUILTIN            2  //-- blue LED
#define LED_WHITE             16
#define LED_RED               17
#define LED_GREEN             18
#define LED_ON                LOW
#define LED_OFF               HIGH

DECLARE_TIMER(heartBeat,     2) //-- fire every 2 seconds 

DECLARE_TIMERm(sensorPoll,   1)  //-- fire every minute

DECLARE_TIMERm(UptimeDisplay,1)  //-- fire every minute

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
servoStruct   servoArray[_MAX_SERVOS];

char      cMsg[150];
String    pTimestamp;
int8_t    prevNtpHour         = 0;
uint32_t  startTimeNow;

int8_t    noSensors;
int8_t    cycleNr             = 0;
int8_t    lastSaveHour        = 0;
uint8_t   connectionMuxLostCount    = 0;
bool      LittleFSmounted       = false;
bool      cycleAllSensors     = false;


#include <rom/rtc.h>

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
  if ( DUE (heartBeat) ) 
  {
    digitalWrite(PIN_HARTBEAT, LOW); 
    digitalWrite(LED_GREEN, LED_ON); 
  } 
  
  if ( SINCE(heartBeat) > 500) //-- milliSeconds?
  {
    digitalWrite(PIN_HARTBEAT, HIGH);
    digitalWrite(LED_GREEN, LED_OFF); 
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

  pinMode(PIN_HARTBEAT, OUTPUT);
  pinMode(LED_BUILTIN,  OUTPUT);
  pinMode(LED_RED,      OUTPUT);
  digitalWrite(LED_RED, LED_ON);
  pinMode(LED_GREEN,    OUTPUT);
  digitalWrite(LED_GREEN, LED_ON);
  pinMode(LED_WHITE,    OUTPUT);
  digitalWrite(LED_WHITE, LED_ON);

  startWiFi();
  digitalWrite(LED_WHITE, LED_OFF);
  startTelnet();

  Debug("Gebruik 'telnet ");
  Debug(WiFi.localIP());
  Debugln("' (port 23) voor verdere debugging");
  digitalWrite(LED_RED, LED_OFF);

  startMDNS(_HOSTNAME);

  ntpInit();
  startTimeNow = now();

  for (int I = 0; I < 10; I++) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
  }

  LittleFS.begin();
  listDir("/", 1);

  httpServer.serveStatic("/",                 LittleFS, "/index.html");
  httpServer.serveStatic("/index.html",       LittleFS, "/index.html");
  httpServer.serveStatic("/ftm.js",           LittleFS, "/ftm.js");
  httpServer.serveStatic("/ftm.css",          LittleFS, "/ftm.css");
  httpServer.serveStatic("/floortempmon.js",  LittleFS, "/floortempmon.js");
  httpServer.serveStatic("/sensorEdit.html",  LittleFS, "/index.html");
  httpServer.serveStatic("/sensorEdit.js",    LittleFS, "/sensorEdit.js");
  httpServer.serveStatic("/favicon.ico",      LittleFS, "/favicon.ico");

  setupFSmanager();

  httpServer.begin();

  apiInit();
   
  DebugTln( "HTTP server gestart\r" );
  digitalWrite(LED_GREEN, LED_OFF);
  
  //-aaw32-sprintf(cMsg, "Last reset reason: [%s]\r", ESP.getResetReason().c_str());
  getLastResetReason(rtc_get_reset_reason(0), cMsg, sizeof(cMsg));
  DebugTf("last Reset Reason CPU[0] [%s]\r\n",cMsg);
  getLastResetReason(rtc_get_reset_reason(1), cMsg, sizeof(cMsg));
  DebugTf("last Reset Reason CPU[1] [%s]\r\n",cMsg);

  //--- Start up the library for the DS18B20 sensors
  sensors.begin();

  delay(5000); // time to start telnet
  sensorsInit();

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

  servosInit();
  setupI2C_Mux();
  roomsInit();
  myKNX_init(&httpServer);

#if defined (USE_NTP_TIME)
  String DT = buildDateTimeString();
  DebugTf("Startup complete! @[%s]\r\n\n", DT.c_str());
#endif 

} // setup()

//===========================================================================================
void loop()
{
  timeThis( handleHeartBeat() );       // toggle PIN_HARTBEAT
  
  //-aaw32-timeThis( MDNS.update() );
  
  timeThis( httpServer.handleClient() );
  timeThis( handleNTP() );

  timeThis( checkI2C_Mux() );         // check I2C_Mux communication with servo board

  timeThis( sensorsLoop() );          // update return water temperature information
  
  timeThis( servosLoop() );           // check for hotter than wanted return water temperatures
  
  timeThis( roomsLoop() );            // check room temperatures and operate servos when necessary

  timeThis( handleCycleServos() );    // ensure servos are cycled daily (and not all at once?)

  timeThis( servosAlign() );          // re-align servos after servo-board reset

  timeThis( myKNX_loop() );           // allign KNX bus and respond to queries over KNX

  timeThis( updateBoiler() );

} 
