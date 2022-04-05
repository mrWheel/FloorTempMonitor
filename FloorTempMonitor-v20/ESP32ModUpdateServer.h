/* 
***************************************************************************  
**  Program  : ESP32ModUpdateServer.h
**  Version  : v2.0.0
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
** 
***************************************************************************      
*/

#ifndef ESP32_HTTP_UPDATE_SERVER_H
#define ESP32_HTTP_UPDATE_SERVER_H

/*
  B
*/

#include <WebServer.h>    // v2.0.0 (part of Arduino Core ESP32 @2.0.2)
#include <Update.h>       // v2.0.0 (part of Arduino Core ESP32 @2.0.2)

class ESP32HTTPUpdateServer
{
private:
  WebServer* _server;

  String _username;
  String _password;
  bool _serialDebugging = true;
  const char *_serverIndex;
  const char *_serverSuccess;

public:
  ESP32HTTPUpdateServer(bool serialDebugging = true)
  {
    _server = NULL;
    _username = "";
    _password = "";
    _serialDebugging = serialDebugging;
  }

  void setIndexPage(const char *indexPage)
  {
  _serverIndex = indexPage;
  }

  void setSuccessPage(const char *successPage)
  {
  _serverSuccess = successPage;
  }  

  void setup(WebServer* server, const char* path = "/update", const char* username = "", const char* password = "")
  {
    _server = server;
    _username = username;
    _password = password;

    // Get of the index handling
    _server->on(path, HTTP_GET, [&]() {
      // Force authentication if a user and password are defined
      //-aaw- if (_username.length() > 0 && _password.length() > 0 && !_server->authenticate(_username.c_str(), _password.c_str()))
      //-aaw-   return _server->requestAuthentication();

      _server->sendHeader("Connection", "close");
      _server->send(200, "text/html", _serverIndex);
    });

    // Post of the file handling
    _server->on(path, HTTP_POST, [&]() {
      _server->client().setNoDelay(true);
      _server->send_P(200, "text/html", (Update.hasError()) ? "FAIL" : _serverSuccess);
      delay(100);
      _server->client().stop();
      TelnetStream.stop();
      ESP.restart();
    }, [&]() {
      HTTPUpload& upload = _server->upload();

      if (upload.status == UPLOAD_FILE_START) 
      {
        // Check if we are authenticated
        if (!(_username.length() == 0 || _password.length() == 0 || _server->authenticate(_username.c_str(), _password.c_str())))
        {
          if (_serialDebugging)
            Serial.printf("Unauthenticated Update\n");

          return;
        }

        // Debugging message for upload start
        if (_serialDebugging) 
        {
          Serial.setDebugOutput(true);
          Serial.printf("Update: %s\n", upload.filename.c_str());
        }

        // Starting update
        bool error = Update.begin(UPDATE_SIZE_UNKNOWN);
        if (_serialDebugging && error)
          Update.printError(Serial);
      }
      else if (upload.status == UPLOAD_FILE_WRITE) 
      {
        Debug('.'); //-- also triggers heartbeat!
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize && _serialDebugging) 
          Update.printError(Serial);
      }
      else if (upload.status == UPLOAD_FILE_END) 
      {
        Debugf("\r\nUpdate Success: %u\r\nRebooting...\r\n", upload.totalSize);
        DebugFlush();
        if (Update.end(true) && _serialDebugging)
          Serial.printf("Update Success: %u bytes!\nRebooting...\n", upload.totalSize);
        else if(_serialDebugging)
          Update.printError(Serial);

        if(_serialDebugging)
          Serial.setDebugOutput(false);
      }
      else 
      {
        Debugf("\r\nUpdate Failed Unexpectedly (likely broken connection): status=%d\r\n", upload.status);
        DebugFlush();
        if(_serialDebugging)
          Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
      }
    });

    _server->begin();
  }
};

#endif


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
