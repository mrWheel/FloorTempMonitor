
// ****************************************************************
// Sketch Esp8266 Filesystem Manager spezifisch sortiert Modular(Tab)
// created: Jens Fleischer, 2020-06-08
// last mod: Jens Fleischer, 2020-09-02
// For more information visit: https://fipsok.de/Esp8266-Webserver/littlefs-esp8266-270.tab
// ****************************************************************
// Hardware: Esp8266, ESP32
// Software: Esp8266 Arduino Core 2.7.0 - 2.7.4
// Software: Esp32   Arduino Core 2.0.2
// Geprüft: von 1MB bis 2MB Flash
// Getestet auf: Nodemcu
/******************************************************************
  Copyright (c) 2020 Jens Fleischer. All rights reserved.

  This file is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This file is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
*******************************************************************/
// Diese Version von LittleFS sollte als Tab eingebunden werden.
// #include <LittleFS.h> #include <ESP8266WebServer.h> müssen im Haupttab aufgerufen werden
// Die Funktionalität des ESP8266 Webservers ist erforderlich.
// "httpServer.onNotFound()" darf nicht im Setup des ESP8266 Webserver stehen.
// Die Funktion "setupFS();" muss im Setup aufgerufen werden.
/**************************************************************************************/

//#include <list>
//#include <tuple>

struct _catStruct {
    char fDir[35];
    char fName[35];
    int fSize;
} catStruct;

const char WARNING[] PROGMEM = R"(<h2>Check! Sketch is compiled with "FS:none"!)";
const char HELPER[]  PROGMEM = R"(
  <br>You first need to upload at least these two files:
  <ul>
    <li>FSmanager.html</li>
    <li>FSmanager.css</li>
  </ul>
  <hr>
  <form method="POST" action="/upload" enctype="multipart/form-data">
    <input type="file" name="upload">
    <input type="submit" value="Upload">
  </form>
  <hr>
  <!--
  <br/><b>or</b> you can use the <i>Flash Utility</i> to flash firmware or LittleFS!
  <form action='/update' method='GET'>
    <input type='submit' name='SUBMIT' value='Flash Utility'/>
  </form>
  -->
)";

//===========================================================================================
//-- Funktionsaufruf "setupFS();" muss im Setup eingebunden werden
void setupFSmanager() 
{
  httpServer.serveStatic("/FSmanager", LittleFS, "/FSmanager.html");
  httpServer.on("/format", formatFS);
  httpServer.on("/listFS", listFS);
  httpServer.on("/ReBoot", reBootESP);
  httpServer.on("/upload", HTTP_POST, sendResponce, handleUpload); 

#ifdef USE_UPDATE_SERVER
  //const char *update_path = "/update";
  //const char *update_username = "admin";
  //const char *update_password = "admin";
  httpUpdater.setup(&httpServer); // , update_path, update_username, update_password);
  httpUpdater.setIndexPage(UpdateServerIndex);
  httpUpdater.setSuccessPage(UpdateServerSuccess);
#endif

  httpServer.onNotFound([]() 
  {
    DebugTf("cannot find [%s]\r\n", httpServer.uri());
    if (!handleFile(httpServer.urlDecode(httpServer.uri())))
        httpServer.send(404, "text/plain", "FileNotFound");
  });

} //  setupFSmanager() 


//===========================================================================================
//-- Senden aller Daten an den Client
bool handleList() 
{
  _catStruct catalog[50];
  char thisDir[35];
  int catPos = 0;

  memset(catalog, 0, sizeof(catalog));
  
  File root = LittleFS.open("/");

  File dir  = root.openNextFile();
  while (dir && (catPos < 50) )
  { 
    yield();  
    if (dir.isDirectory()) // Ordner und Dateien zur Liste hinzufügen
    {
      //DebugTf("Found Directory [%s]\r\n", dir.name());
      uint8_t ran {0};
      snprintf(thisDir, sizeof(thisDir), "/%s/", dir.name());
      File fold = LittleFS.open(thisDir);
      fold.openNextFile();  
      while (fold && (catPos < 49))  
      {
        yield();
        //DebugTf("[%s] Found file [%s]\r\n", thisDir, fold.name());
        ran++;
        //dirList.emplace_back(String(dir.name()), String(fold.name()), fold.size());
        snprintf(catalog[catPos].fDir, sizeof(catalog[catPos].fDir), "%s", dir.name());
        snprintf(catalog[catPos].fName, sizeof(catalog[catPos].fName), "%s", fold.name());
        catalog[catPos].fSize = fold.size();
        catPos++;
        fold = dir.openNextFile();  
      }
      if (!ran)
      {
        snprintf(catalog[catPos].fDir, sizeof(catalog[catPos].fDir), "%s", dir.name());
        snprintf(catalog[catPos].fName, sizeof(catalog[catPos].fName), "");
        catalog[catPos].fSize = 0;
        catPos++;
      }
    }
    else 
    {
      //DebugTf("Found file [%s]\r\n", dir.name());
      snprintf(catalog[catPos].fDir, sizeof(catalog[catPos].fDir), "");
      snprintf(catalog[catPos].fName, sizeof(catalog[catPos].fName), "%s", dir.name());
      catalog[catPos].fSize = dir.size();
      catPos++;
    }
    dir = root.openNextFile();
  }

  qsort(catalog, catPos, sizeof(catalog[0]), sort_desc);

  String temp = "[";
  for (int i=0; i<catPos; i++) 
  {
    if (temp != "[") temp += "\n,";
    temp += "{\"folder\":\"" + String(catalog[i].fDir)
             + "\",\"name\":\"" + String(catalog[i].fName)
             + "\",\"size\":\"" + formatBytes(catalog[i].fSize) + "\"}";
  }
  temp += ",{\"usedBytes\":\"" + formatBytes(LittleFS.usedBytes()) +                      // Berechnet den verwendeten Speicherplatz
          "\",\"totalBytes\":\"" + formatBytes(LittleFS.totalBytes()) +                   // Zeigt die Größe des Speichers
          "\",\"freeBytes\":\"" + (LittleFS.totalBytes() - LittleFS.usedBytes()) + "\"}]";   // Berechnet den freien Speicherplatz

  httpServer.send(200, "application/json", temp);
  return true;
  
} //  handleList() 


//===========================================================================================
void deleteRecursive(const String &path) 
{
  if (LittleFS.remove(path)) 
  {
    LittleFS.open(path.substring(0, path.lastIndexOf('/')) + "/", "w");
    return;
  }
  File dir = LittleFS.open(path);
  while (dir.openNextFile()) 
  {
    yield();
    deleteRecursive(path + '/' + dir.name());
  }
  LittleFS.rmdir(path);
  
} //  deleteRecursive()


//===========================================================================================
bool handleFile(String &&path) 
{

  if (httpServer.hasArg("new")) 
  {
    String folderName {httpServer.arg("new")};
    for (auto& c : {34, 37, 38, 47, 58, 59, 92}) for (auto& e : folderName) if (e == c) e = 95;    // Ersetzen der nicht erlaubten Zeichen
    LittleFS.mkdir(folderName);
  }
  if (httpServer.hasArg("sort")) return handleList();
  if (httpServer.hasArg("delete")) 
  {
    deleteRecursive(httpServer.arg("delete"));
    sendResponce();
    return true;
  }
  if (!LittleFS.exists("/FSmanager.html")) 
  {
    httpServer.send(200, "text/html", LittleFS.begin() ? HELPER : WARNING);     // ermöglicht das hochladen der FSmanager.html
  }
  if (path.endsWith("/")) path += "/index.html";
  if (path == "/LittleFS.html") sendResponce(); // Vorrübergehend für den Admin Tab
  //return LittleFS.exists(path) ? ({File f = LittleFS.open(path, "r"); httpServer.streamFile(f, mime::getContentType(path)); f.close(); true;}) : false;
  return LittleFS.exists(path) ? ({File f = LittleFS.open(path, "r"); httpServer.streamFile(f, getContentType(path)); f.close(); true;}) : false;

} //  handleFile()


//===========================================================================================
void handleUpload() 
{                            // Dateien ins Filesystem schreiben
  static File fsUploadFile;
  
  HTTPUpload& upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) 
  {
    if (upload.filename.length() > 31) 
    {  // Dateinamen kürzen
      upload.filename = upload.filename.substring(upload.filename.length() - 31, upload.filename.length());
    }
    printf(PSTR("handleFileUpload Name: /%s\r\n"), upload.filename.c_str());
    fsUploadFile = LittleFS.open(httpServer.arg(0) + "/" + httpServer.urlDecode(upload.filename), "w");
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) 
  {
    printf(PSTR("handleFileUpload Data: %u\r\n"), upload.currentSize);
    fsUploadFile.write(upload.buf, upload.currentSize);
  } 
  else if (upload.status == UPLOAD_FILE_END) 
  {
    printf(PSTR("handleFileUpload Size: %u\r\n"), upload.totalSize);
    fsUploadFile.close();
  }
  
} //  handleUpload()


//===========================================================================================
void formatFS()      // Formatiert das Filesystem
{
  DebugTln("formatting littleFS ..");
  LittleFS.format();
  sendResponce();
  
} //  formatFS()


//===========================================================================================
void listFS() 
{
  DebugTln("send littleFS data ..");
  sendResponce();
  
} //  listFS()


//===========================================================================================
void sendResponce() 
{
  httpServer.sendHeader("Location", "/FSmanager.html");
  httpServer.send(303, "message/http");
  
} //  sendResponse()


//===========================================================================================
const String formatBytes(size_t const& bytes) 
{                                        // lesbare Anzeige der Speichergrößen
  return bytes < 1024 ? static_cast<String>(bytes) + " Byte" : bytes < 1048576 ? static_cast<String>(bytes / 1024.0) + " KB" : static_cast<String>(bytes / 1048576.0) + " MB";

} //  formatBytes()

/*
//=====================================================================================
void updateFirmware()
{
#ifdef USE_UPDATE_SERVER
  DebugTln(F("Redirect to updateIndex ???"));
  //doRedirect("wait ... ", 5, "/updateIndex ", false);
  httpServer.send(200, "text/html", UpdaterBootstrap);
#else
  doRedirect("UpdateServer not available", 10, "/", false);
#endif
      
} // updateFirmware()
*/

//=====================================================================================
void reBootESP()
{
  DebugTln(F("Redirect and ReBoot .."));
  doRedirect("Reboot ESP32 ..", 30, "/", true);
      
} // reBootESP()

//=====================================================================================
void doRedirect(String msg, int wait, const char* URL, bool reboot)
{
  String redirectHTML = 
  "<!DOCTYPE HTML><html lang='en-US'>"
  "<head>"
  "<meta charset='UTF-8'>"
  "<style type='text/css'>"
  "body {background-color: lightblue;}"
  "</style>"
  "<title>Redirect to Main Program</title>"
  "</head>"
  "<body><h1>"+String(_HOSTNAME)+"</h1>"
  "<h3>"+msg+"</h3>";
  if (String(URL).indexOf("/updateIndex") == 0)
  {
    redirectHTML += "<br>If this does not work just type \"<b>http://"+String(_HOSTNAME)+".local/update\"";
    redirectHTML += "</b>as URL!<br>";
  }
  redirectHTML += "<br><div style='width: 500px; position: relative; font-size: 25px;'>"
  "  <div style='float: left;'>Redirect over &nbsp;</div>"
  "  <div style='float: left;' id='counter'>"+String(wait)+"</div>"
  "  <div style='float: left;'>&nbsp; seconden ...</div>"
  "  <div style='float: right;'>&nbsp;</div>"
  "</div>"
  "<!-- Note: don't tell people to `click` the link, just tell them that it is a link. -->"
  "<br><br><hr>If you are not redirected automatically, click this <a href='/'>Main Program</a>."
  "  <script>"
  "      setInterval(function() {"
  "          var div = document.querySelector('#counter');"
  "          var count = div.textContent * 1 - 1;"
  "          div.textContent = count;"
  "          if (count <= 0) {"
  "              window.location.replace('"+String(URL)+"'); "
  "          } "
  "      }, 1000); "
  "  </script> "
  "</body></html>\r\n";
  
  DebugTln(msg);
  httpServer.send(200, "text/html", redirectHTML);
  if (reboot) 
  {
    delay(5000);
    ESP.restart();
    delay(5000);
  }
  
} // doRedirect()


//===========================================================================================
String getContentType(String filename)
{
  if (httpServer.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";

} // getContentType()


//---------------------------------------------------------
// qsort requires you to create a sort function
int sort_desc(const void *cmp1, const void *cmp2)
{
  // Need to cast the void * to int *
  int a = *((int *)cmp1);
  int b = *((int *)cmp2);
  // The comparison
  //return a > b ? -1 : (a < b ? 1 : 0);
  // A simpler, probably faster way:
  return b - a;

} //  sort_desc()
