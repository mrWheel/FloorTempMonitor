/*
 * ATtiny85 Watch Dog for an ESP8266/ESP32
 * 
 * Copyright 2022 Willem Aandewiel
 * Version 2.0  23-04-2022
 * 
 * 
 * Arduino IDE version 1.8.13
 * ATTinyCore 1.5.2 (By Spence Kondo)
 * 
 * Board              : "ATtiny25/45/85 (no bootloader)"
 * Chip               : "ATtiny85"
 * Clock Source       : "8MHz (internal)"
 * Timer 1 Clock      : "CPU (CPU Frequency)"
 * LTO(1.6.11+ only)  : "disabled"
 * millis()/micros()  : "Enabled"
 * save EEPROM        : "EEPROM not retained"
 * B.O.D. Level (Only set on bootload): "B.O.D. Enabled (1.8v)"
 * 
 * ATMEL ATTINY85
 *                        +--\/--+
 *             RESET PB5 1|      |8 VCC
 *    <--[ESP32_EN]  PB3 2|      |7 PB2  (INT0) <----- heartbeat
 *              [x]  PB4 3|      |6 PB1  [RST_LED] -->
 *                   GND 4|      |5 PB0  [WDT_LED] -> 
 *                        +------+
 *
 *  Boot sequence:
 *  ==============
 *  State       | RST_LED | WDT_LED       | Remark
 *  ------------+---------+---------------+------------------------------------
 *  Power On    | Blink   | GREEN         | 500ms On / 500ms Off (~30 seconden)
 *              |         |               | Next: "Fase 1"
 *  ------------+---------+---------------+------------------------------------
 *  Fase 1      | Off     | Off           | Next: "Normal Operation"
 *  ------------+---------+---------------+------------------------------------
 *  Normal      | Off     | On for every  | Dims after 250ms 
 *  Operation   |         | heartbeat     | (_GLOW_TIME)
 *              |         | received      | 
 *              +         +---------------+------------------------------------
 *              | Blink every 3 seconds   | If no heartbeat received for 20
 *              |                         | seconds -> Next: "Normal Operation"
 *              | Blink every 1 seconds   | If no heartbeat received for 40
 *              |                         | seconds -> Next: "Normal Operation"
 *              | Blink every 0.5 seconds | No heartbeat received for 60
 *              |                         | seconds -> Next: "Reset" state 
 *  ------------+---------+---------------+------------------------------------
 *  Reset       | Blink   | Blink         | Reset ESP32
 *              |         |               | ~60 seconds
 *              |         |               | Next: "Fase 1" state
 *  ------------+---------+---------------+------------------------------------
 *  
*/

// https://github.com/GreyGnome/EnableInterrupt
#include <EnableInterrupt.h>

#define _PIN_RST_LED          0       // GPIO-01 ==> DIL-6 ==> PB1
#define _PIN_WDT_LED          1       // GPIO-00 ==> DIL-5 ==> PB0
#define _PIN_INTERRUPT        2       // GPIO-02 ==> DIL-7 ==> PB2  / INT0
#define _PIN_ESP32_EN         3       // GPIO-03 ==> DIL-2 ==> PB3
#define _PIN_DUMMY            4       // GPIO-04 ==> DIL-3 ==> PB4

#define _LED_ON             LOW
#define _LED_OFF           HIGH

#define _HALF_A_SECOND      500       // half a second
#define _MAX_HALF_SECONDS    40       // Max 20 seconds elapse befoure WDT kicks in!
#define _STARTUP_TIME     25000       // 25 seconden
#define _MAX_NO_HARTBEAT  60000       // 60 seconds
#define _LAST_WARNING     40000 
#define _FIRST_WARNING    20000 
#define _GLOW_TIME          250       // MilliSeconds

volatile  bool receivedInterrupt = false;
uint32_t  WDcounter;
uint8_t   feedsReceived = 0;
uint32_t  glowTimer, blinkWdtLedTimer, blinkRstLedTimer;
uint32_t  lastHartbeatTimer = 0;

//----------------------------------------------------------------
void interruptSR(void) 
{
  receivedInterrupt = true;
    
}   // interruptSR()


//----------------------------------------------------------------
void blinkRstLed(uint32_t durationMS)
{
  if ((millis() - blinkRstLedTimer) > durationMS)
  {
    blinkRstLedTimer = millis();
    digitalWrite(_PIN_RST_LED, !digitalRead(_PIN_RST_LED));
  }
  
} // blinkRstLed()


//----------------------------------------------------------------
void blinkWdtLed(uint16_t onMS, uint16_t offMS, uint32_t durationMS)
{
  uint32_t  timeToGo = millis() + durationMS;
  
  while (timeToGo > millis())
  {
    digitalWrite(_PIN_WDT_LED, _LED_OFF);
    delay(onMS);
    digitalWrite(_PIN_WDT_LED, _LED_ON);
    delay(offMS);
  }
  digitalWrite(_PIN_WDT_LED, _LED_OFF);
  
} // blinkWdtLed()


//----------------------------------------------------------------
void delayMS(uint32_t durationMS)
{
  uint32_t  timeToGo = millis() + durationMS;
  
  while (timeToGo > millis())
  {
    if (millis() > blinkWdtLedTimer)
    {
      digitalWrite(_PIN_WDT_LED, _LED_ON);
    }
  }

} // delayMS()


//----------------------------------------------------------------
void setup() 
{
    pinMode(_PIN_WDT_LED,  OUTPUT);
    digitalWrite(_PIN_WDT_LED, _LED_ON);

    pinMode(_PIN_ESP32_EN, INPUT); // _PULLUP?
    pinMode(_PIN_RST_LED,  OUTPUT);

    for(int r=0; r<60; r++) //-- ongeveer 30 seconden
    {
      digitalWrite(_PIN_RST_LED, !digitalRead(_PIN_RST_LED));
      //digitalWrite(_PIN_WDT_LED, !digitalRead(_PIN_RST_LED));
      delay(500);
    }
    digitalWrite(_PIN_RST_LED, _LED_OFF); // begin met relays-led "off"
    digitalWrite(_PIN_WDT_LED, _LED_OFF);

  //enableInterrupt(_PIN_INTERRUPT, interruptSR, RISING);
    enableInterrupt(_PIN_INTERRUPT, interruptSR, CHANGE);

    receivedInterrupt = false;
    WDcounter         = 0;
    feedsReceived     = 0;
    lastHartbeatTimer = millis();

} // setup()


//----------------------------------------------------------------
void loop() 
{
  if (receivedInterrupt)
  {
    receivedInterrupt = false;
    lastHartbeatTimer = millis();
    glowTimer         = millis();
    digitalWrite(_PIN_WDT_LED, _LED_ON);
    digitalWrite(_PIN_RST_LED, _LED_OFF);
    //WDcounter++;
    //if (WDcounter > 120)
  }
  
  if ((millis() - glowTimer) > _GLOW_TIME)
  {
    digitalWrite(_PIN_WDT_LED, _LED_OFF);
  }

  //-- no heartbeats for too long!
  //-- initiate reset sequence ..
  if ((millis() - lastHartbeatTimer) > _MAX_NO_HARTBEAT)
  {
    //disableInterrupt(_PIN_INTERRUPT);
    for(int i=0; i<20; i++)
    {
      digitalWrite(_PIN_RST_LED, !digitalRead(_PIN_RST_LED));
      delay(250);
    }
    //digitalWrite(_PIN_ESP32_EN,  HIGH);
    digitalWrite(_PIN_RST_LED, _LED_ON);
    pinMode(_PIN_ESP32_EN, OUTPUT);
    digitalWrite(_PIN_ESP32_EN, HIGH);
    delay(500);
    digitalWrite(_PIN_ESP32_EN,  LOW);
    delay(500);
    digitalWrite(_PIN_ESP32_EN, HIGH);

    pinMode(_PIN_ESP32_EN, INPUT);
    WDcounter = 0;
    //-- now give ESP time (60 seconds) to startup!
    for(int i=0; i<60; i++)
    {
      digitalWrite(_PIN_RST_LED, _LED_ON);
      digitalWrite(_PIN_WDT_LED, _LED_ON);
      delay(250);
      digitalWrite(_PIN_WDT_LED, _LED_OFF);
      delay(250);
      digitalWrite(_PIN_RST_LED, _LED_OFF);
      delay(500);
    }
    //enableInterrupt(_PIN_INTERRUPT, interruptSR, CHANGE);
  }
  
  else if ((millis() - lastHartbeatTimer) > _LAST_WARNING)
  {
    blinkRstLed(1000);  // 1 seconds!
  }
  
  else if ((millis() - lastHartbeatTimer) > _FIRST_WARNING)
  {
    blinkRstLed(3000);  // 3 seconds!
  }
  
  if ((millis() - blinkRstLedTimer) > 500)
  {
    digitalWrite(_PIN_RST_LED, _LED_OFF);
  }
  
} // loop()

/* eof */
