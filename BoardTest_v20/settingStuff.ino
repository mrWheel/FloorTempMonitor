/*
* settingStuff
* 
*/

void readSettings()
{
  char buff[80];
  
  File s = LittleFS.open("/settings.ini", FILE_READ);
  while (s.available())
  {
    s.readBytesUntil('\n', buff, sizeof(buff));
    noReboots = atoi(buff);
  }
} //  readSettings()


void writeSettings()
{
  File s = LittleFS.open("/settings.ini", FILE_WRITE);
  s.println(noReboots);
  
} //  writeSettings()
