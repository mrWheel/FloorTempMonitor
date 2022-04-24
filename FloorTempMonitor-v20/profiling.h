/*
 * profiling
 * 
 * timeThis(FN) 
 *  will profile any function FN given as argument
 *  
 * when PROFILING is not defined, it just executes the FN
 * when PROFILING is defined, it times the duration
 * when duration in above PROFILING_THRESHOLD it prints duration using Debug
 * 
 * PROFILING_THRESHOLD default to 3ms
 * 
 * !! define PROFILING and PROFILING_THRESHOLD before including profiling.h
 *  
 */
 
#ifndef PROFILING_H
#define PROFILING_H

#ifdef PROFILING

	#include "Debug.h"
  #include "timers.h"

  #ifndef PROFILING_THRESHOLD
    #define PROFILING_THRESHOLD 3
  #endif

  #define timeThis(FN)    ({ \
	  unsigned long start_time=millis(); \
    FN; \
    unsigned long duration=ELAPSED(start_time, millis() ); \
    yield(); \
    if (duration >= PROFILING_THRESHOLD) \
    	DebugTf("Function %s [called from %s:%d] took %lu ms\n",\
            	#FN, __FUNCTION__, __LINE__, duration); \
    })
          
#else // PROFILING not required

        #define timeThis(FN)    FN ;

#endif // PROFILING

#endif // PROFILING_H
