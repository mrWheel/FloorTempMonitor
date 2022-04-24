
/*
**  Program   : FloorTempMonitor
*/


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

/******************** don't change anything below this comment **********************/

#include "FloorTempMonitor.h"

// platform 

#include <time.h>
#include "time_zones.h"

#include <WiFi.h>             // v2.0.0 (part of Arduino ESP32 Core @2.0.2)   
#include <WiFiManager.h>      // v2.0.10-beta - https://github.com/tzapu/WiFiManager

#include <ESPmDNS.h>          // v2.0.0 (part of Arduino ESP32 Core @2.0.2)
#include <WebServer.h>        // v2.0.0 (part of Arduino ESP32 Core @2.0.2)

#include <FS.h>               // v2.0.0 (part of Arduino Core ESP32 @2.0.2)                          
#include <LittleFS.h>         // v2.0.0 (part of Arduino Core ESP32 @2.0.2)
                              //-- you need to install this plugin: https://github.com/lorol/arduino-esp32littlefs-plugin
                      
// my platform

#include "Debug.h"
#include "timers.h"
#include "profiling.h"
#include "network.h"

// application

#include "servos.h"
#include "sensors.h"
#include "rooms.h"
#include "restapi.h"

#include "LedPin.h"

DECLARE_TIMER(heartBeat, 2)  //-- fire every 2 seconds
__TIMER(UptimeDisplay,   3)  //-- fire every 3 minutes/seconds

const char  *ntpServer  = "pool.ntp.org";
const char  *TzLocation = "Europe/Amsterdam";
tm          timeInfo;

char        cMsg[150];
uint32_t    startTimeNow;

bool        LittleFSmounted = false;

#include <rom/rtc.h>      // low-level 'C'???

#ifdef USE_UPDATE_SERVER

  #include "UpdateServerHtml.h"
  #include "ESP32ModUpdateServer.h" 
  
  ESP32HTTPUpdateServer httpUpdater(true);
  
#endif

WebServer        httpServer (80);

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
  uint32_t  upTimeNow = (millis() / 1000) - startTimeNow; // upTimeNow = seconds

  sprintf(calcUptime, "%d[d] %02d:%02d"
          , (int)((upTimeNow / (60 * 60 * 24)) % 365)
          , (int)((upTimeNow / (60 * 60)) % 24) // hours
          , (int)((upTimeNow / 60) % 60));       // minuten
  return calcUptime;

} // upTime()


//===========================================================================================
void handleHeartBeat()
{
  // toggle HB pin
  address_t GA;
  
  knx.loop();
  
  if ( DUE (heartBeat) )
  {
    boolean nextStatus = !digitalRead(PIN_HEARTBEAT);
    
    digitalWrite(PIN_HEARTBEAT, nextStatus);
    
    GA = knx.GA_to_address(1,1,99);
    knx.write_1bit(GA, (uint8_t) (nextStatus ? 1 : 0));  // HeartBeat on KNXbus
    
    DebugTf("Flipping heartbeat on pin %d\n", PIN_HEARTBEAT);
  }

  if ( DUE (UptimeDisplay) )
  {
    DebugTf("Running %s\n", upTime());
  }

} //  handleHeartBeat()


//===========================================================================================
void setup()
{

  // platform 
  Serial.begin(115200);
  
  Debugln("\nBooting ... \n");
  Debugf("[%s] %s  compiled [%s %s]\n", _HOSTNAME, _FW_VERSION, __DATE__, __TIME__);

  startTimeNow = millis() / 1000;

  DebugTln("Freeze Hardware Watchdog ..");
  pinMode(PIN_WDT_RST,  OUTPUT);
  digitalWrite(PIN_WDT_RST, LOW);

  pinMode(PIN_HEARTBEAT, OUTPUT);
  // handleHeartBeat();
  
  pinMode(LED_BUILTIN,  OUTPUT);

  pinMode(LED_RED,      OUTPUT);
  digitalWrite(LED_RED, LED_ON);

  pinMode(LED_GREEN,    OUTPUT);
  digitalWrite(LED_GREEN, LED_OFF);

  pinMode(LED_WHITE,    OUTPUT);
  digitalWrite(LED_WHITE, LED_OFF);

  startWiFi();
  // handleHeartBeat();
  
  DebugTln("Free Hardware Watchdog ..!");
  digitalWrite(PIN_WDT_RST, HIGH);

  digitalWrite(LED_RED, LED_OFF);
  digitalWrite(LED_WHITE, LED_ON);

  configTime(0, 0, ntpServer);
  //-- Set environment variable with your time zone
  setenv("TZ", getTzByLocation(TzLocation).c_str(), 1);
  tzset();
  // handleHeartBeat();

  startTelnet();
  delay(3000); // time to start telnet

  printLocalTime();

  DebugT("Use 'telnet ");
  Debug(WiFi.localIP());
  Debugln("' (port 23) for debugging");

  startMDNS(_HOSTNAME);
  // handleHeartBeat();

  digitalWrite(LED_WHITE, LED_OFF);
  
  getLastResetReason(rtc_get_reset_reason(0), cMsg, sizeof(cMsg));
  DebugTf("last Reset Reason CPU[0] [%s]\r\n", cMsg);
  getLastResetReason(rtc_get_reset_reason(1), cMsg, sizeof(cMsg));
  DebugTf("last Reset Reason CPU[1] [%s]\r\n", cMsg);

  for (int I = 0; I < 11; I++) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
  }

  LittleFSmounted = false;      
  if (LittleFS.begin())
    LittleFSmounted = true;

  httpServer.serveStatic("/",                 LittleFS, "/index.html");
  httpServer.serveStatic("/index.html",       LittleFS, "/index.html");
  httpServer.serveStatic("/ftm.js",           LittleFS, "/ftm.js");
  httpServer.serveStatic("/ftm.css",          LittleFS, "/ftm.css");
  httpServer.serveStatic("/floortempmon.js",  LittleFS, "/floortempmon.js");
  httpServer.serveStatic("/favicon.ico",      LittleFS, "/favicon.ico");

  setupFSmanager();
  // handleHeartBeat();

  digitalWrite(LED_BLUE, LED_OFF);

  httpServer.begin();
  DebugTln( "HTTP server gestart\r" );
 
  digitalWrite(LED_BLUE, LED_OFF);
  digitalWrite(LED_GREEN, LED_ON);

 //// FTM Application Specific stuff ////
 
  // handleHeartBeat();

  AllSensors.Init();
  AllSensors.Print();
  // handleHeartBeat();

  AllServos.Init();
  // handleHeartBeat();

  Rooms.Init();

  API_Init(&httpServer);

  myKNX_init(&httpServer);  // knxInit has to be called AFTER roomsInit as it uses noRooms counter!
  handleHeartBeat();

#ifdef USE_NTP_TIME
  if (getLocalTime(&timeInfo))
  {
    DebugTf("Startup complete! %02d-%02d-20%02d %02d:%02d:%02d\r\n", timeInfo.tm_mday, timeInfo.tm_mon,
            timeInfo.tm_year, hour(), minute(), second());
  }
#endif

  DebugTf("Startup complete! (took[%d]seconds)\r\n\n", millis() / 1000);

} // setup()


//===========================================================================================
void loop()
{

  
  timeThis( handleHeartBeat() );       // toggle PIN_HEARTBEAT
  timeThis( httpServer.handleClient() );

  timeThis( handleNTP() );
  timeThis( httpServer.handleClient() );

  timeThis( AllSensors.Loop() );      // update return water temperature information
  timeThis( httpServer.handleClient() );

  timeThis( Rooms.Loop() );           // check room temperatures and operate servos when necessary
  timeThis( httpServer.handleClient() );

  timeThis( AllServos.Loop() );       // update servo state
  timeThis( httpServer.handleClient() );

  timeThis( myKNX_loop() );           // allign KNX bus and respond to queries over KNX
  timeThis( httpServer.handleClient() );

  timeThis( updateBoiler() );
  timeThis( httpServer.handleClient() );

}
