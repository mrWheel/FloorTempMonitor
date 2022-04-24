/*
   Debug Beginning Of Line (BOL)

   Outputs the BeginOfLine of a DebugT* line
   as per macro's in Debug.h

   Includes times when USE_NTP_TIME is configured

*/

#include "Debug.h"

void _debugBOL(const char *fn, int line)
{

   char _bol[64];

#ifdef USE_NTP_TIME

  struct tm tInfo;

  if (getLocalTime(&tInfo))
  {
    sprintf(_bol, "[%02d:%02d:%02d] %-12.12s(%4d): ",
            tInfo.tm_hour, tInfo.tm_min, tInfo.tm_sec,
            fn, line);
  } else {
    sprintf(_bol, "%-12.12s(%4d): ",
            fn, line);
  }
#else

  sprintf(_bol, "%-12.12s(%4d): ",
          fn, line);
#endif

  Serial.print (_bol);
  TelnetStream.print (_bol);
}
