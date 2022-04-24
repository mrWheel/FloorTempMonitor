#ifndef _FTM_H
#define _FTM_H

/* 
 *  
 * Compiler options and other generic 'settings'
 * this should be the first file included in source files that are part of the FloorTempMonitor
 
/******************** compiler options  ********************************************/

#define _FW_VERSION "v2.0.5 (13-04-2022)"

// #define TESTDATA
// #define SPEEDUP

#define USE_NTP_TIME
#define USE_UPDATE_SERVER

#define HAS_FSMANAGER           // USED?
#define SHOW_PASSWRDS           // USED?

#define _MAX_LITTLEFS_FILES     20

#define PROFILING               // comment this line out if you want not profiling 
#define PROFILING_THRESHOLD 45  // defaults to 3ms - don't show any output when duration below TH

#include "rom/rtc.h"

// prototypes for functions in project used in external .cpp files in same project

void getLastResetReason(RESET_REASON reason, char *txtReason, int txtReasonLen);
void handleHeartBeat();
void loop();
void setup();
char * upTime();

#endif
