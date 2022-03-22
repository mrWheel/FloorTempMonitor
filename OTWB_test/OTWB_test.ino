
#define _FW_VERSION "v1.0.0 (16-05-2020)"


#define _HOSTNAME   "OTWB"
#include "ESP8266_Framework.h"

// WiFi Server object and parameters
WiFiServer server(80);


uint32_t feedTime = 1000;
int ledMode;


//=====================================================================
void handleWDTfeed(bool force)
{
  if (force || (millis() > WDTfeedTimer))
  {
    WDTfeedTimer = millis() + feedTime;
    digitalWrite(KEEP_ALIVE_PIN, !digitalRead(KEEP_ALIVE_PIN));
    digitalWrite(LED_BLUE,        digitalRead(KEEP_ALIVE_PIN));
    if (force)
    {
      feedTime = 2000;
    }
    else
    {
      feedTime += 500;
      DebugTf("feedTime is now [%d] MSec\r\n", feedTime);
    }
  }
  
} // handleWDTfeed();


//=====================================================================
void setup()
{
  Serial.begin(115200);
  while(!Serial) { /* wait a bit */ }

  lastReset     = ESP.getResetReason();

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(KEEP_ALIVE_PIN,  OUTPUT);
  pinMode(LED_BLUE,    OUTPUT);
  pinMode(LED_RED_B,    OUTPUT);
  pinMode(LED_RED_C,    OUTPUT);
  
  startTelnet();
  
  DebugTln("\r\n[ESP8266_Framework]\r\n");
  DebugTf("Booting....[%s]\r\n\r\n", String(_FW_VERSION).c_str());
  
//================ SPIFFS ===========================================
  if (SPIFFS.begin()) 
  {
    DebugTln(F("SPIFFS Mount succesfull\r"));
    SPIFFSmounted = true;
  } else { 
    DebugTln(F("SPIFFS Mount failed\r"));   // Serious problem with SPIFFS 
    SPIFFSmounted = false;
  }

  readSettings(true);

  // attempt to connect to Wifi network:
  DebugTln("Attempting to connect to WiFi network\r");
  int t = 0;
  while ((WiFi.status() != WL_CONNECTED) && (t < 25))
  {
    delay(100);
    Serial.print(".");
    t++;
  }
  Debugln();
  if ( WiFi.status() != WL_CONNECTED) 
  {
    sprintf(cMsg, "Connect to AP '%s' and configure WiFi on  192.168.4.1   ", _HOSTNAME);
    DebugTln(cMsg);
  }
  // Connect to and initialise WiFi network
  digitalWrite(LED_BUILTIN, HIGH);
  startWiFi(_HOSTNAME, 240);  // timeout 4 minuten
  digitalWrite(LED_BUILTIN, LOW);

  startMDNS(settingHostname);
  
  //--- ezTime initialisation
  setDebug(INFO);  
  waitForSync(); 
  CET.setLocation(F("Europe/Amsterdam"));
  CET.setDefault(); 
  
  Debugln("UTC time: "+ UTC.dateTime());
  Debugln("CET time: "+ CET.dateTime());

  snprintf(cMsg, sizeof(cMsg), "Last reset reason: [%s]\r", ESP.getResetReason().c_str());
  DebugTln(cMsg);

  Serial.print("\nGebruik 'telnet ");
  Serial.print (WiFi.localIP());
  Serial.println("' voor verdere debugging\r\n");

//================ Start HTTP Server ================================
  setupFSexplorer();
  httpServer.serveStatic("/FSexplorer.png",   SPIFFS, "/FSexplorer.png");
  httpServer.on("/",          sendIndexPage);
  httpServer.on("/index",     sendIndexPage);
  httpServer.on("/index.html",sendIndexPage);
  httpServer.serveStatic("/index.css", SPIFFS, "/index.css");
  httpServer.serveStatic("/index.js",  SPIFFS, "/index.js");
  // all other api calls are catched in FSexplorer onNotFounD!
  httpServer.on("/api", HTTP_GET, processAPI);


  httpServer.begin();
  DebugTln("\nServer started\r");
  
  // Set up first message as the IP address
  sprintf(cMsg, "%03d.%03d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  DebugTf("\nAssigned IP[%s]\r\n", cMsg);

  for (int b=0; b<20; b++)
  {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(200);
  }
  
} // setup()


//=====================================================================
void loop()
{
  httpServer.handleClient();
  MDNS.update();
  events(); // trigger ezTime update etc.
  handleWDTfeed(false);

  //--- Eat your hart out!
  if (millis() > blinkyTimer)
  {
    blinkyTimer = millis() + 2000;
    //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    switch(ledMode)
    {
      case 0: digitalWrite(LED_RED_B, !digitalRead(LED_RED_B));
              DebugTf("Switch LED_RED_B %s\r\n", digitalRead(LED_RED_B) ? "ON" : "OFF");
              ledMode++;
              break;
      case 1: digitalWrite(LED_RED_C, !digitalRead(LED_RED_C));
              DebugTf("Switch LED_RED_C %s\r\n", digitalRead(LED_RED_C) ? "ON" : "OFF");
              ledMode++;
              break;
      case 2: digitalWrite(LED_RED_B, LOW);
              digitalWrite(LED_RED_C, LOW);
              delay(300);
              digitalWrite(LED_RED_B, HIGH);
              delay(300);
              digitalWrite(LED_RED_C, HIGH);
              delay(300);
              digitalWrite(LED_RED_B, LOW);
              delay(300);
              digitalWrite(LED_RED_C, LOW);
              delay(300);
              ledMode = 0;
              break;
    }
  }
  
} // loop()
