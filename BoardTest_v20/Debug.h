/* 
***************************************************************************  
**  Program  : SPrint.h, part of this project
**  Version  : v2.0.1
**
**  Copyright (c) 2021 - 2022 Willem Aandewiel
**  Met dank aan Erik
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
  bool  doVerbose = true;
  
/*---- start macro's ------------------------------------------------------------------*/

#define SPrint(...)      ({ Serial.print(__VA_ARGS__);         \
                            TelnetStream.print(__VA_ARGS__);   \
                            handleHeartBeat();                 \
                        })
#define SPrintln(...)    ({ Serial.println(__VA_ARGS__);       \
                            TelnetStream.println(__VA_ARGS__); \
                            handleHeartBeat();                 \
                        })
#define SPrintf(...)     ({ Serial.printf(__VA_ARGS__);        \
                            TelnetStream.printf(__VA_ARGS__);  \
                            handleHeartBeat();                 \
                        })

#define SPrintFlush()    ({ Serial.flush();                    \
                            TelnetStream.flush();              \
                            handleHeartBeat();                 \
                        })

  #define Debug(...)      ({ if (doVerbose)                       \
                             { Serial.print(__VA_ARGS__);         \
                               TelnetStream.print(__VA_ARGS__);   \
                             }  \
                             handleHeartBeat();                   \
                          })
  #define Debugln(...)    ({ if (doVerbose)                       \
                             { Serial.println(__VA_ARGS__);       \
                               TelnetStream.println(__VA_ARGS__); \
                             }  \
                             handleHeartBeat();                   \
                          })
  #define Debugf(...)     ({ if (doVerbose)                       \
                             { Serial.printf(__VA_ARGS__);        \
                               TelnetStream.printf(__VA_ARGS__);  \
                             }  \
                             handleHeartBeat();                   \
                          })

  #define DebugT(...)     ({ if (doVerbose)                       \
                             { _debugBOL(__FUNCTION__, __LINE__); \
                               SPrint(__VA_ARGS__);               \
                             }  \
                          })
  #define DebugTln(...)   ({ if (doVerbose)                       \
                             { _debugBOL(__FUNCTION__, __LINE__); \
                               SPrintln(__VA_ARGS__);             \
                             }  \
                          })
  #define DebugTf(...)    ({ if (doVerbose)                       \
                             { _debugBOL(__FUNCTION__, __LINE__); \
                               SPrintf(__VA_ARGS__);              \
                             }  \
                          })

/*---- einde macro's ------------------------------------------------------------------*/

// needs #include <TelnetStream.h>       // Version 0.0.1 - https://github.com/jandrassy/TelnetStream

char _bol[128];
void _debugBOL(const char *fn, int line)
{
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    sprintf(_bol, "[%02d:%02d:%02d] %-12.12s(%4d): ", \
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, \
                fn, line);
  }
  else
  {
    sprintf(_bol, "%-12.12s(%4d): ", \
                fn, line);
  }              
  Serial.print (_bol);
  TelnetStream.print (_bol);
}

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
****************************************************************************
*/
