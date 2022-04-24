#ifndef LEDPIN_H
#define LEDPIN_H

#define PIN_HEARTBEAT         4  //-- pulse @ 0.5-1.0 Hz

#ifndef LED_BUILTIN
  #define LED_BUILTIN         2  //-- blue LED
#endif

#define PIN_WDT_RST            0
#define LED_WHITE             16
#define LED_GREEN             17
#define LED_RED               18
#define LED_BLUE              LED_BUILTIN
#define LED_ON                LOW
#define LED_OFF               HIGH
#define _SDA                  21
#define _SCL                  22

#endif
