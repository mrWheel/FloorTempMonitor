/*
***************************************************************************
**  Program  : ntpStuff, part of FloorTempMonitor
**  Version  : v1.0.0
**
**  Copyright 2022 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file. 
***************************************************************************
*/

#ifdef USE_NTP_TIME

/*
**  struct tm {
**    int tm_sec;         // seconds,  range 0 to 59
**    int tm_min;         // minutes, range 0 to 59
**    int tm_hour;        // hours, range 0 to 23
**    int tm_mday;        // day of the month, range 1 to 31
**    int tm_mon;         // month, range 0 to 11
**    int tm_year;        // The number of years since 1900
**    int tm_wday;        // day of the week, range 0 to 6
**    int tm_yday;        // day in the year, range 0 to 365
**    int tm_isdst;       // daylight saving time 
**  };
*/

//===========================================================================================
void handleNTP()
{
  static int  lastHour = 99;

  if (hour() != lastHour)
  {
    if (getLocalTime(&timeInfo)) lastHour = hour();
  }                  
}

//===========================================================================================
int hour()
{
  return timeInfo.tm_hour;
}

//===========================================================================================
int minute()
{
  return timeInfo.tm_min;
}

//===========================================================================================
int second()
{
  return timeInfo.tm_sec;
}

//===========================================================================================
void printLocalTime()
{
  //struct tm timeInfo;
  if(!getLocalTime(&timeInfo))
  {
    DebugTln("Failed to obtain time");
    digitalWrite(LED_RED, LED_ON);
    return;
  }
  DebugTln(&timeInfo, "%A, %B %d %Y %H:%M:%S");
  digitalWrite(LED_WHITE, LED_ON);
  digitalWrite(LED_RED, LED_OFF);

} //  printLocalTime()

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
