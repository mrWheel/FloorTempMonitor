/*
***************************************************************************
**  Program  : fsysStuff, part of FloorTempMonitor
**  Version  : v0.6.1
**
**  Copyright 2019, 2020, 2021, 2022 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

//-aaw32- static      FSInfo SPIFFSinfo;

//===========================================================================================
uint32_t freeSpace()
{
  int32_t space;

  //-aaw32- SPIFFS.info(SPIFFSinfo);

  //-aaw32- space = (int32_t)(SPIFFSinfo.totalBytes - SPIFFSinfo.usedBytes);
  space = (int32_t)(LittleFS.totalBytes() - LittleFS.usedBytes());
  if (space < 1) space = 0;

  return space;

} // freeSpace()

//===========================================================================================
String listDir(String dirPath, uint8_t levels)
{
  String htmlOut;
  /*
  Dir dir = SPIFFS.openDir("/");

  DebugTln("\r\n");
  while (dir.next()) {
    File f = dir.openFile("r");
    Debugf("%-25s %6d \r\n", dir.fileName().c_str(), f.size());
    yield();
  }
  */
  htmlOut  = "<table>";
  File root = LittleFS.open(dirPath);
  if(!root)
  {
    Debugln("− failed to open directory");
    return "<pre>Error: failed to open directory..</pre>";
  }
  if(!root.isDirectory())
  {
    Debugln(" − not a directory");
    return "<pre>Error: not a directory..</pre>";
  }

  File file = root.openNextFile();
  while(file)
  {
    htmlOut += "<tr>";
    if(file.isDirectory())
    {
      Debug("DIR: ");
      htmlOut += "<td>Dir: &nbsp; ";
      Debugln(dirPath+file.name()+"/");
      htmlOut += file.name();
      htmlOut += "</td><td></td></tr>\n";
      if(levels)
      {
        listDir(dirPath + file.name() + "/", levels -1);
        htmlOut += "</tr><tr> </tr>\n";
      }
    }
    else 
    {
      Debug("  FILE: ");
      htmlOut += "<td>";
      Debug(file.name());
      htmlOut += file.name();
      Debug(" \tSize: ");
      htmlOut += "</td><td>";
      Debugln(file.size());
      htmlOut += file.size();
      htmlOut += "</td>";
    } //  dir?
    
    htmlOut += "</tr>\n";
    file = root.openNextFile();
    
  } // more files?

  htmlOut += "</table>\n";

  //-aaw32- SPIFFS.info(SPIFFSinfo);

  Debugln("\r");
  //-aaw32- SPIFFS has no member named blockSize()
  /** -aaw32-
  if (freeSpace() < (10 * SPIFFS.blockSize()))
        Debugf("Available SPIFFS space [%6d]kB (LOW ON SPACE!!!)\r\n", (freeSpace() / 1024));
  else  Debugf("Available SPIFFS space [%6d]kB\r\n", (freeSpace() / 1024));
  **/
  //-aaw32- Debugf("           SPIFFS Size [%6d]kB\r\n", (SPIFFSinfo.totalBytes / 1024));
  Debugf("  LittleFS Size [%6d]kB\r\n\n", (LittleFS.totalBytes() / 1024));
  //-aaw32- SPIFFS has no member named blockSize()
  //-aaw32- Debugf("     SPIFFS block Size [%6d]bytes\r\n", SPIFFSinfo.blockSize);
  //-aaw32- SPIFFS has no member named pageSize()
  //-aaw32- Debugf("      SPIFFS page Size [%6d]bytes\r\n", SPIFFSinfo.pageSize);
  //-aaw32- Debugf(" SPIFFS max.Open Files [%6d]\r\n\r\n", SPIFFSinfo.maxOpenFiles);

  return htmlOut;

} // listDir()



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
