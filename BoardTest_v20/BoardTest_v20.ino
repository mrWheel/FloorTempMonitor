/*
***************************************************************************  
 *  Program  : FTM_BoardTest_v20 - template- 
 *  Copyright (c) 2021 - 2022 Willem Aandewiel
 */
#define _FW_VERSION "v2.0.0 (02-04-2022)"
/* 
*  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************  
 * Board:         "Generic ESP32 Module"  // always! even if you have a Wemos or NodeMCU!
 * Builtin Led:   "2"
 * CPU Frequency: "80 MHz" or "160 MHz"
 * Flash Size:    "4MB (FS:2MB OTA:~???KB)"
***************************************************************************/  

#define USE_UPDATE_SERVER

//---------- no need to change enything after this ------

#define _HOSTNAME   "FTMTESTER"

#define _MAX_LITTLEFS_FILES   30

#include "WiFicredentials.h"
const char ssid[] = WIFI_SSID;    
const char pass[] = WIFI_PASSWORD;  


#include <WiFi.h>
#include <TelnetStream.h>
#include <time.h>
#include "timing.h"
#include "time_zones.h"
#include "Debug.h"
#include <LittleFS.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "I2C_MuxLib.h"

const char *ntpServer  = "pool.ntp.org";
const char *TzLocation = "Europe/Amsterdam";

#ifdef USE_UPDATE_SERVER
  #include "updateServerHtml.h"
  #include "ESP32ModUpdateServer.h"
#endif

#ifndef LED_BUILTIN
  #define LED_BUILTIN          2  //-- blue LED
#endif
#define PIN_HARTBEAT           4  //-- pulse @ 0.5-1.0 Hz
#define LED_WHITE             16
#define LED_GREEN             17
#define LED_RED               18
#define LED_ON               LOW
#define LED_OFF             HIGH
#define _SDA                  21
#define _SCL                  22
#define I2C_MUX_ADDRESS     0x48    // the 7-bit address 

DECLARE_TIMER(heartBeat,     3) //-- fire every 3 seconds 
DECLARE_TIMERm(UptimeDisplay,1) //-- fire every minute
DECLARE_TIMERms(whiteLed, 1000) //-- toggle every 0.5 seconds 
DECLARE_TIMER(greenLed,      5) //-- toggle every 5 seconds 
DECLARE_TIMER(redLed,       30) //-- toggle every 20 seconds 

WebServer  httpServer(80);
#ifdef USE_UPDATE_SERVER
  ESP32HTTPUpdateServer httpUpdater(true);
#endif

I2CMUX    I2cExpander;   //Create instance of the I2CMUX object

struct tm timeinfo;

uint32_t startTimeNow = 0, printTimeTimer = 0, pulseTimer = 0;
uint32_t stopHartbeatTimer = 0;
bool     I2cMuxFound = false;

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
    DebugTf("Running %s\r\n",upTime());
  }

} //  handleHeartBeat()


//===========================================================================================
char *upTime()
{
  static char calcUptime[20];
  uint32_t  upTimeNow = ((millis() / 1000) - startTimeNow); // upTimeNow = seconds

  sprintf(calcUptime, "%d[d] %02d:%02d" 
          , (int)((upTimeNow / (60 * 60 * 24)) % 365)
          , (int)((upTimeNow / (60 * 60)) % 24) // hours
          , (int)((upTimeNow / 60) % 60));       // minuten
  return calcUptime;

} // upTime()


//===========================================================================================
void checkI2C_Mux()
{
  static int16_t errorCountUp   = 0;
  static int16_t errorCountDown = 0;
  static uint32_t clickTimer    = 0;
  byte whoAmI /*, majorRelease, minorRelease */ ;
  
  //DebugTln("getWhoAmI() ..");
  if( I2cExpander.connectedToMux())
  {
    if ( (whoAmI = I2cExpander.getWhoAmI()) == I2C_MUX_ADDRESS)
    {
      for(int i=0;i<16;i++)
      {
        I2cExpander.digitalWrite(i, !I2cExpander.digitalRead(i));
      }
      return;
    } 
    else
    {
      DebugTf("Connected to different I2cExpander %x\r\n",whoAmI );
    }
  } 
  else 
  {
    DebugTf("Connection lost to I2cExpander\r\n");
  }
  digitalWrite(LED_RED, LED_ON);
  
} // checkI2C_MUX();


//===========================================================================================
bool setupI2C_Mux()
{
  byte whoAmI;
  
  DebugT("Setup Wire ..");
//Wire.begin(_SDA, _SCL); // join i2c bus (address optional for master)
  Wire.begin();
  Wire.setClock(100000L); // <-- don't make this 400000. It won't work
  Debugln(".. done");
  
  if (I2cExpander.begin()) {
    DebugTln("Connected to the I2C multiplexer!");
  } else {
    DebugTln("Not Connected to the I2C multiplexer !ERROR!");
    delay(100);
    return false;
  }
  whoAmI       = I2cExpander.getWhoAmI();
  if (whoAmI != I2C_MUX_ADDRESS) {
    digitalWrite(LED_RED, LED_ON);
    return false;
  }
  digitalWrite(LED_RED, LED_OFF);

  DebugT("Close all servo's ..16 ");
  I2cExpander.digitalWrite(0, LOW);
  delay(300);
  for (int s=15; s>0; s--) {
    I2cExpander.digitalWrite(s, LOW);
    Debugf("%d ",s);
    delay(150);
  }
  Debugln();
  DebugT("Open all servo's ...");
  for (int s=15; s>0; s--) {
    I2cExpander.digitalWrite(s, HIGH);
    Debugf("%d ",s);
    delay(150);
  }
  delay(250);
  I2cExpander.digitalWrite(0, HIGH);
  Debugln(16);
  
  return true;
  
} // setupI2C_Mux()


//------------------------------------------------------
void setup() 
{
  Serial.begin(115200);
  while(!Serial) { delay(10); }
  Serial.println();
  Serial.printf("\r\n[%s]\r\n", _HOSTNAME);
  Serial.flush();

  startTimeNow = millis() / 1000; //-- in seconds!

  pinMode(LED_BUILTIN,  OUTPUT);
  pinMode(LED_WHITE,    OUTPUT);
  pinMode(LED_GREEN,    OUTPUT);
  pinMode(LED_RED,      OUTPUT);
  pinMode(PIN_HARTBEAT, OUTPUT);
  
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) 
  {
    Serial.println("WiFi failed! ..Retry to connect.");
    delay(1000);
    WiFi.begin(ssid, pass);
  }

  IPAddress ip = WiFi.localIP();
  Debugln();
  DebugTln("Connected to WiFi network.");
  DebugT("Connect with Telnet client to ");
  Debugln(ip);
  
  //DebugTf("getTzByLocation(%s)\r\n", getTzByLocation(TzLocation).c_str());
  //DebugTf("NTP server [%s]\r\n", ntpServer);

  //-- 0, 0 because we will use TZ in the next line
  configTime(0, 0, ntpServer);
  //-- Set environment variable with your time zone
  setenv("TZ", getTzByLocation(TzLocation).c_str(), 1);
  tzset();
  printLocalTime();

  DebugTf("[1] mDNS setup as [%s.local]\r\n", _HOSTNAME);
  if (MDNS.begin(_HOSTNAME))               // Start the mDNS responder for Hostname.local
  {
    DebugTf("[2] mDNS responder started as [%s.local]\r\n", _HOSTNAME);
  } 
  else 
  {
    DebugTln(F("[3] Error setting up MDNS responder!\r\n"));
  }
  MDNS.addService("http", "tcp", 80);

  LittleFS.begin();
  
  TelnetStream.begin();

  httpServer.serveStatic("/",               LittleFS, "/index.html");
  httpServer.serveStatic("/index",          LittleFS, "/index.html");
  httpServer.serveStatic("/index.html",     LittleFS, "/index.html");
  httpServer.serveStatic("/index.css",      LittleFS, "/index.css");
  httpServer.serveStatic("/index.js",       LittleFS, "/index.js");

  setupFSmanager();
  
  httpServer.begin();
  
  DebugTln("HTTP httpServer started");

  I2cMuxFound = setupI2C_Mux();

}   // setup()


//------------------------------------------------------
void loop() 
{
  httpServer.handleClient();

  //if ( (millis() - stopHartbeatTimer) < 30000)
  {
    handleHeartBeat();       // toggle PIN_HARTBEAT
  }

  if (DUE(whiteLed)) 
  {
    digitalWrite(LED_WHITE, !digitalRead(LED_WHITE));
  }
  if (DUE(greenLed)) 
  {
    digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
    //DebugTf("Green LED %s\r\n", digitalRead(LED_GREEN) ? "Off":"On ");
  }
  if (DUE(redLed))    
  {
    digitalWrite(LED_RED,   !digitalRead(LED_RED));
    DebugTf("Red LED %s\r\n", digitalRead(LED_RED) ? "Off":"On ");
  }

  //--- read key's from monitor and telnet
  if (Serial.available()) 
  {
    char cIn = Serial.read();
    if ( (cIn >= ' ' && cIn <= 'z') || (cIn == '\n') ) 
    {
      TelnetStream.print(cIn);
    }
  }
  if (TelnetStream.available()) 
  {
    char cIn = TelnetStream.read();
    if ( (cIn >= ' ' && cIn <= 'z') || (cIn == '\n') ) 
    {
      Serial.print(cIn);
    }
  }

  if ((millis() - pulseTimer) > 2000)
  {
    pulseTimer = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    if (I2cMuxFound)  checkI2C_Mux();
  }

  if ((millis() - printTimeTimer) > 120000)
  {
    printTimeTimer = millis();
    printLocalTime();
  }

}   // loop()

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
****************************************************************************
*/
