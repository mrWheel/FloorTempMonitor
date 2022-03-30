/*
 * Debug Beginning Of Line (BOL)
 * 
 * Outputs the first part of a DebugT* line
 * as per macro's in Debug.h
 * 
 * Includes times when USE_NTP_TIME is configured
 * 
 */

#include "Debug.h"

char _bol[128];
void _debugBOL(const char *fn, int line)
{
#ifdef USE_NTP_TIME
  struct tm timeInfo;
  if (getLocalTime(&timeInfo))
  {
    sprintf(_bol, "[%02d:%02d:%02d] %-12.12s(%4d): ", \
                timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec, \
                fn, line);
  }
  else
  {
    sprintf(_bol, "%-12.12s(%4d): ", \
                fn, line);
  }              
#else
  sprintf(_bol, "%-12.12s(%4d): ", \
                fn, line); 
#endif
                 
   Serial.print (_bol);
   TelnetStream.print (_bol);
}
