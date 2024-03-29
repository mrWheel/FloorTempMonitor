/*
***************************************************************************  
**  Program  : network.ino, part of FloorTempMonitor
**  Version  : v0.5.0
**
**  Copyright 2020, 2021, 2022 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/

#include "FloorTempMonitor.h"
#include "Debug.h"


//gets called when WiFiManager enters configuration mode
//===========================================================================================
void configModeCallback (WiFiManager *myWiFiManager);
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
  
  WiFi.mode(WIFI_STA);
  manageWiFi.setDebugOutput(true);
  
  //--set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  //manageWiFi.setAPCallback(configModeCallback);

  //--sets timeout until configuration portal gets turned off
  //--useful to make it all retry or go to sleep in seconds
  manageWiFi.setTimeout(240);  // 4 minuten
  
  //--fetches ssid and pass and tries to connect
  //--if it does not connect it starts an access point with the specified name
  //--here  "FloorTempMonitor-<MAC>"
  //--and goes into a blocking loop awaiting configuration
  if (!manageWiFi.autoConnect(thisAP.c_str())) {
    DebugTln("failed to connect and hit timeout");

    //reset and try again, or maybe put it to deep sleep
    DebugFlush();
    delay(2000);
    TelnetStream.stop();
    ESP.restart();
    delay(2000);
  }

  DebugTf("Connected with IP-address [%s]\r\n\r\n", WiFi.localIP().toString().c_str());

#ifdef USE_UPDATE_SERVER
  httpUpdater.setup(&httpServer);
  httpUpdater.setIndexPage(UpdateServerIndex);
  httpUpdater.setSuccessPage(UpdateServerSuccess);
#endif
  
} // startWiFi()


//===========================================================================================
void startTelnet() 
{
  TelnetStream.begin();
  DebugTln("Telnet server started ..");
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
