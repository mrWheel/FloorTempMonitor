
//#include <ESP8266WiFi.h>

#include <ezTime.h>             // https://github.com/ropg/ezTime
#include <TelnetStream.h>       // https://github.com/jandrassy/TelnetStream/commit/1294a9ee5cc9b1f7e51005091e351d60c8cddecf
#include "Debug.h"
#include "networkStuff.h"

#define SETTINGS_FILE   "/settings.ini"
#define CMSG_SIZE        512
#define JSON_BUFF_MAX   1024

// Wemos D1 on 1of!-Wemos board
#define THERMOSTAT_IN     16  //--- GPIO-16 / PIN-4  / D0
#define THERMOSTAT_OUT     4  //--- GPIO-04 / PIN-5  / D2
#define BOILER_IN          5  //--- GPIO-05 / PIN-6  / D1
#define BOILER_OUT        14  //--- GPIO-14 / PIN-12 / D5

#define KEEP_ALIVE_PIN    13  //--- GPIO-13 / PIN-14 / D7
#define LED_BLUE           2  //--- GPIO-02 / PIN-3  / D4
#define LED_RED_B          0  //--- GPIO-00 / PIN-4  / D3
#define LED_RED_C         12  //--- GPIO-12 / PIN-13 / D6


bool      Verbose = false;
char      cMsg[CMSG_SIZE];
char      fChar[10];
String    lastReset   = "";
uint32_t  ntpTimer    = millis() + 30000;
uint32_t  WDTfeedTimer, blinkyTimer;
char      settingHostname[41];
Timezone  CET;

const char *weekDayName[]  {  "Unknown", "Zondag", "Maandag", "Dinsdag", "Woensdag"
                            , "Donderdag", "Vrijdag", "Zaterdag", "Unknown" };
const char *flashMode[]    { "QIO", "QOUT", "DIO", "DOUT", "Unknown" };

// eof
