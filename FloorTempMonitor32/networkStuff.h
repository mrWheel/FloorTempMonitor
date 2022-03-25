/*
***************************************************************************  
**  Program  : networkStuff.h, part of FloorTempMonitor
**  Version  : v0.5.0
**
**  Copyright (c) 2019 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
#if defined(ESP8266)
  #include <ESP8266WiFi.h>        // version 1.0.0 - part of ESP8266 Core https://github.com/esp8266/Arduino
  #include <DNSServer.h>
//#include <ESP8266WebServer.h>   // Version 1.0.0 - part of ESP8266 Core https://github.com/esp8266/Arduino
  #include <ESPAsyncTCP.h>     // new AaW
  #include <WiFiUdp.h>            // part of ESP8266 Core https://github.com/esp8266/Arduino
  #include <ESP8266mDNS.h>        // part of ESP8266 Core https://github.com/esp8266/Arduino
  #ifdef USE_UPDATE_SERVER
    #include <ESP8266HTTPUpdateServer.h>
  #endif

#else
  #include <WiFi.h>         //
  #include <DNSServer.h>    // https://github.com/zhouhan0126/DNSServer---esp32
//#include <WebServer.h>    // https://github.com/zhouhan0126/WebServer-esp32
  #include <AsyncTCP.h>     // new AaW
  #include <WiFiUdp.h>      // 
  #include <ESPmDNS.h>      // 
  #ifdef USE_UPDATE_SERVER
    #include <Update.h>
  #endif

#endif

#include <ESPAsyncWebServer.h>

#ifdef USE_WIFIMANAGER
  #include <WiFiManager.h>        // ESP8266: version 0.14.0 - https://github.com/tzapu/WiFiManager
                                  // ESP32:   version        - https://github.com/zhouhan0126/WIFIMANAGER-ESP32
                                  // EDIT: WiFiManager.h and .cpp in WIFIMANAGER-ESP32-master
                                  // Replace alloccurences of
                                  //      HTTP_HEAD[]   ----> HTTP_HEADM[]
                                  //      HTTP_HEAD_END[] --> HTTP_HEADM_END[]
#endif
                                
#include <TelnetStream.h>       // Version 0.0.1 - https://github.com/jandrassy/TelnetStream
                                // EDIT: TelnetStream.h in TelnetStream-master
                                // 19    #ifndef _TELNETSTREAM_H_
                                // 20    #define _TELNETSTREAM_H_
                                // 21
                                // 22    #if defined(ESP8266)
                                // 23      #include <ESP8266WiFi.h>
                                // 24    #else
                                // 25      #include <WiFi.h>
                                // 26    #endif
                                // 27    class TelnetStreamClass : public Stream ....
                                
                                // EDIT: TelnetStream.cpp in TelnetStream-master
                                // 17   boolean TelnetStreamClass::disconnected() {
                                // 18   //if (server.status() == CLOSED)
                                // 19   //  return true;
                                // 20   if (!client) {



#if defined(ESP8266)
//ESP8266WebServer        httpServer (80);
  #ifdef USE_UPDATE_SERVER
    ESP8266HTTPUpdateServer httpUpdater(true);
  #endif

#else
//WebServer        httpServer (80);
//#ifdef USE_UPDATE_SERVER
//  HTTPUpdateServer httpUpdater(true);
//#endif

#endif

AsyncWebServer        httpServer (80);

#ifdef USE_WIFIMANAGER

//gets called when WiFiManager enters configuration mode
//===========================================================================================
void configModeCallback (WiFiManager *myWiFiManager) 
{
  DebugTln("Entered config mode\r");
  DebugTln(WiFi.softAPIP().toString());
  //if you used auto generated SSID, print it
  DebugTln(myWiFiManager->getConfigPortalSSID());

} // configModeCallback()


//===========================================================================================
void startWiFi() 
{
  WiFiManager manageWiFi;

  String thisAP = String(_HOSTNAME) + "-" + WiFi.macAddress();
  
  manageWiFi.setDebugOutput(true);
  
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  manageWiFi.setAPCallback(configModeCallback);

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep in seconds
  manageWiFi.setTimeout(240);  // 4 minuten
  
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "DSMR-WS-<MAC>"
  //and goes into a blocking loop awaiting configuration
  if (!manageWiFi.autoConnect(thisAP.c_str())) {
    DebugTln("failed to connect and hit timeout");

    //reset and try again, or maybe put it to deep sleep
    delay(3000);
  //ESP.reset();
    ESP.restart();
    delay(2000);
  }

  DebugTf("Connected with IP-address [%s]\r\n\r\n", WiFi.localIP().toString().c_str());
#ifdef HAS_OLED_SSD1306
    oled_Clear();
#endif


#ifdef USE_UPDATE_SERVER
  httpUpdater.setup(&httpServer);
#endif
  
} // startWiFi()

#endif

//===========================================================================================
void startTelnet() 
{
  TelnetStream.begin();
  DebugTln("\nTelnet server started ..");
  TelnetStream.flush();

} // startTelnet()


//=======================================================================
void startMDNS(const char *Hostname) 
{
  DebugTf("[1] mDNS setup as [%s.local]\r\n", Hostname);
  if (MDNS.begin(Hostname)) {              // Start the mDNS responder for Hostname.local
    DebugTf("[2] mDNS responder started as [%s.local]\r\n", Hostname);
  } else {
    DebugTln("[3] Error setting up MDNS responder!\r\n");
  }
  MDNS.addService("http", "tcp", 80);
  
} // startMDNS()

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
***************************************************************************/
