/*
***************************************************************************
**  Program  : FSexplorer, part of FloorTempMonitor
**  Version  : v0.5.0
**
**  Mostly stolen from https://www.arduinoforum.de/User-Fips
**  See also https://www.arduinoforum.de/arduino-Thread-SPIFFS-DOWNLOAD-UPLOAD-DELETE-Esp8266-NodeMCU
**
***************************************************************************
*/

#ifdef HAS_FSEXPLORER

File fsUploadFile;                      // Stores de actuele upload

//===========================================================================================
void reloadPage(String goTo)
{
  String goToPageHTML;
  goToPageHTML += "<!DOCTYPE HTML><html lang='en-US'><head><meta charset='UTF-8'>";
  goToPageHTML += "<meta name= viewport content=width=device-width, initial-scale=1.0, user-scalable=yes>";
  goToPageHTML += "<style>body {background-color: powderblue;}</style>";
  goToPageHTML += "</head>\r\n<body><center>Wait ..</center>";
  goToPageHTML += "<br><br><hr>If you are not redirected automatically, click this <a href='/'>"+String(_HOSTNAME)+"</a>.";
  goToPageHTML += "  <script>";
  goToPageHTML += "    window.location.replace('" + goTo + "'); ";
  goToPageHTML += "  </script> ";
  httpServer.send(200, "text/html", goToPageHTML );

} // reloadPage()


//===========================================================================================
void handleRoot()                       // HTML FSexplorer
{
#if defined(ESP8266)
  FSInfo fs_info;
  SPIFFS.info(fs_info);
#endif
  String FSexplorerHTML;
  FSexplorerHTML += "<!DOCTYPE HTML><html lang='en-US'>";
  FSexplorerHTML += "<head>";
  FSexplorerHTML += "<meta charset='UTF-8'>";
  FSexplorerHTML += "<meta name= viewport content='width=device-width, initial-scale=1.0,' user-scalable=yes>";
  FSexplorerHTML += "<style type='text/css'>";
  FSexplorerHTML += "body {background-color: lightblue;}";
  FSexplorerHTML += "</style>";
  FSexplorerHTML += "</head>";
  FSexplorerHTML += "<body><h1>FSexplorer</h1><h2>Upload, Download of Verwijder</h2>";

  FSexplorerHTML += "<hr><h3>Selecteer bestand om te downloaden:</h3>\r\n";
  if (!SPIFFS.begin())  {
    DebugTln("SPIFFS failed to mount !\r\n\r");
  }

#if defined(ESP8266)
  Dir dir = SPIFFS.openDir("/");         // List files on SPIFFS
  while (dir.next())  {
    FSexplorerHTML += "<a href ='";
    FSexplorerHTML += dir.fileName();
    FSexplorerHTML += "?download='>";
    FSexplorerHTML += "SPIFFS";
    FSexplorerHTML += dir.fileName();
    FSexplorerHTML += "</a> ";
    FSexplorerHTML += formatBytes(dir.fileSize()).c_str();
    FSexplorerHTML += "<br>\r\n";
  }
#endif

  FSexplorerHTML += "<p><hr><big>Sleep bestand om te verwijderen:</big>";
  FSexplorerHTML += "<form action='/FSexplorer' method='POST'>Om te verwijderen ";
  FSexplorerHTML += "<input type='text' style='height:45px; font-size:15px;' name='Delete' placeholder=' Bestand hier in-slepen ' required />";
  FSexplorerHTML += "<input type='submit' class='button' name='SUBMIT' value='Verwijderen' />";
  FSexplorerHTML += "</form><p>";

  //FSexplorerHTML += "<hr>";
  FSexplorerHTML += "<form method='POST' action='/FSexplorer/upload' enctype='multipart/form-data' style='height:35px;'>";
  FSexplorerHTML += "<big>Bestand uploaden: &nbsp;</big>";
  FSexplorerHTML += "<input type='file' name='upload' style='height:35px; font-size:14px;' required />";
  FSexplorerHTML += "<input type='submit' value='Upload' class='button'>";
  FSexplorerHTML += "</form>";

#if defined(ESP8266)
  FSexplorerHTML += "<p>";
  FSexplorerHTML += "Omvang SPIFFS: ";
  FSexplorerHTML += formatBytes(fs_info.totalBytes).c_str();
  FSexplorerHTML += "<br>Waarvan in gebruik: ";
  FSexplorerHTML += formatBytes(fs_info.usedBytes).c_str();
  FSexplorerHTML += "<p><hr>\r\n";
#endif

  FSexplorerHTML += "<div style='width: 60%'>";
  FSexplorerHTML += "  <form style='float: left;' action='/update' method='GET'><big>Update Firmware </big>";
#ifdef USE_UPDATE_SERVER
  FSexplorerHTML += "    <input type='submit' class='button' name='SUBMIT' value='select Firmware' ENABLED/>";
#else
  FSexplorerHTML += "    <input type='submit' class='button' name='SUBMIT' value='select Firmware' DISABLED/>";
#endif
  FSexplorerHTML += "  </form>";
    
  FSexplorerHTML += "  <form style='float: right;' action='/sensorEdit.html'><big>Edit Sensoren </big>";
  FSexplorerHTML += "    <input type='submit' class='button' name='SUBMIT' value='Edit'/>";
  FSexplorerHTML += "  </form>";
  FSexplorerHTML += "</div>";

  FSexplorerHTML += "<br><hr>";
  FSexplorerHTML += "<div style='width: 60%'>";
  FSexplorerHTML += "  <form style='float: left;' action='/ReBoot'>ReBoot " + String(_HOSTNAME);
  FSexplorerHTML += "    <input type='submit' class='button' name='SUBMIT' value='ReBoot'>";
  FSexplorerHTML += "  </form>";

  FSexplorerHTML += "  <form style='float: right;' action='/'> &nbsp; Exit FSexplorer ";
  FSexplorerHTML += "    <input type='submit' class='button' name='SUBMIT' value='Exit'>";
  FSexplorerHTML += "  </form>";

  FSexplorerHTML += "</div>";
  FSexplorerHTML += "<div style='width: 80%'>&nbsp;</div>";

  FSexplorerHTML += "</body></html>\r\n";

  httpServer.send(200, "text/html", FSexplorerHTML);

}

//===========================================================================================
String formatBytes(size_t bytes)
{
  if (bytes < 1024) {
    return String(bytes) + " Byte";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + " KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + " MB";
  }
  return String(bytes) + " Byte?";
}

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


//===========================================================================================
bool handleFileRead(String path)
{
  DebugTf("handleFileRead: [%s]\r\n", path.c_str());
  //if (path.endsWith("/")) path += "DSMRlogger.html";
  String contentType = getContentType(path);
  DebugTln(contentType);
  Debugln("\r");
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    httpServer.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;

} // handleFileRead()


//===========================================================================================
void handleFileDelete()
{
  String file2Delete, hostNameURL, IPaddressURL;
  if (httpServer.args() == 0) return handleRoot();
  if (httpServer.hasArg("Delete")) {
    file2Delete = httpServer.arg("Delete");
    file2Delete.toLowerCase();
#if defined(ESP8266)
    Dir dir = SPIFFS.openDir("/");
#else
    File dir = SPIFFS.open("/");
#endif
#if defined(ESP8266)
    while (dir.next())    {
      String path = dir.fileName();
#else
    File file = dir.openNextFile();
    while(file) {
      String path = file.name();
#endif
      path.replace(" ", "%20");
      path.replace("ä", "%GDC%A4");
      path.replace("Ä", "%GDC%84");
      path.replace("ö", "%GDC%B6");
      path.replace("Ö", "%GDC%96");
      path.replace("ü", "%GDC%BC");
      path.replace("Ü", "%GDC%9C");
      path.replace("ß", "%GDC%9F");
      path.replace("€", "%E2%82%AC");
      hostNameURL   = "http://" + String(_HOSTNAME) + ".local" + path + "?download=";
      hostNameURL.toLowerCase();
      IPaddressURL  = "http://" + WiFi.localIP().toString() + path + "?download=";
      IPaddressURL.toLowerCase();
      //if (httpServer.arg("Delete") != "http://" + WiFi.localIP().toString() + path + "?download=" )
      if ( (file2Delete != hostNameURL ) && (file2Delete != IPaddressURL ) ) {
        continue;
      }
#if defined(ESP8266)
      SPIFFS.remove(dir.fileName());
#else
      SPIFFS.remove(path);
#endif
      String header = "HTTP/1.1 303 OK\r\nLocation:";
      header += httpServer.uri();
      header += "\r\nCache-Control: no-cache\r\r\n\n";
      httpServer.sendContent(header);
      return;
    }

    reloadPage(httpServer.uri());
  }

} // handleFileDelete()


//===========================================================================================
void handleFileUpload()
{
  if (httpServer.uri() != "/FSexplorer/upload") return;
  HTTPUpload& upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    DebugT("handleFileUpload Name: ");
    Debug(filename);
    Debugln("\r");
    if (filename.length() > 30) {
      int x = filename.length() - 30;
      filename = filename.substring(x, 30 + x);
    }
    if (!filename.startsWith("/")) filename = "/" + filename;
    DebugT("handleFileUpload Name: ");
    Debug(filename);
    Debugln("\r");
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    DebugT("handleFileUpload Data: ");
    Debug(upload.currentSize);
    Debugln("\r");
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
    yield();
    DebugT("handleFileUpload Size: ");
    Debug(upload.totalSize);
    Debugln(" bytes\r");
    reloadPage("/FSexplorer");
  }

} // handleFileUpload()


//===========================================================================================
//void formatSpiffs() 
//{      
//  SPIFFS.format();  // Format SPIFFS
//  handleRoot();
//}

#endif  // HAS_FSEXPLORER
