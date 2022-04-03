/*
***************************************************************************  
**
**    Program : I2C_Multiplexer.ino
**    Date    : 05-04-2020
*/
#define _MAJOR_VERSION  1
#define _MINOR_VERSION  7
/*
**
**  Copyright 2020, 2021, 2022 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.                                                            
***************************************************************************      
*/
/*
*     Use the standard Arduino UNO bootloader
*     
* Settings:
*     Board:  [miniCore] ATmega328 (Arduino/Genuino UNO)
*     Variant:   ATmega328P
*     Clock: "Extternal 16 MHz" (Ceramic Resonator)
*     B.O.D. Level: B.O.D. Enabled (1.8v)    [if possible]
*     ?? B.O.D. Mode (active): B.O.D. Disabled  [if possible]
*     ?? B.O.D. Mode (sleep): B.O.D. Disabled   [if possible]
*     Save EEPROM: EEPROM retained           [if possible]
*     Compiler LTO: "LTO disabled"
*     Bootloader: "No bootloader"
*     
*     USBtinyISP slow (MiniCore)
*     Set [Tab] on "M"                                                                    
*     
* Set fuses with AVRFuses:    
*     -* Brown-out Detection trigger level: Brown-out detection disabled
*     -* Brown-out Detection trigger level: Brown-out detection at VCC=2.7 V
*     - Serial programming downloading (SPI) enabled
*     - Preserve EEPROM through the Cheip Erase Cycle
*     - Select boot size: Boot Flash size=256 words staret address=$3F00
*     - Boot Reset vector Enabled
*     - Select Clock Source: Ext. Crystal Osc. 8.0--  MHz; start-up time PWRDWN/RESET: 258 CK/14 CK + 65ms
*     Fuses OK (E:FF, H:D6, L:DE) -> Brown-out detection disabled
*     Fuses OK (E:FD, H:D6, L:DE) -> Brown-out detection 2.7 v
*/

// #include <avr/wdt.h>
// #include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#define _I2C_DEFAULT_ADDRESS  0x48  // 72 dec

#define _RELAY_ON             0
#define _RELAY_OFF            255
#define MAX_INACTIVE_TIME     5000   // milliSeconds

struct registerLayout {
  byte      status;         // 0x00
  byte      whoAmI;         // 0x01
  byte      majorRelease;   // 0x02
  byte      minorRelease;   // 0x03
  byte      lastGpioState;  // 0x04
  byte      numberOfRelays; // 0x05
  byte      filler[3];      // 0x06 .. 0x08
};

#define _CMD_REGISTER   0xF0

//These are the defaults for all settings
registerLayout registerStack = {
  .status         = 0,                    // 0x00
  .whoAmI         = _I2C_DEFAULT_ADDRESS, // 0x01
  .majorRelease   = _MAJOR_VERSION,       // 0x02
  .minorRelease   = _MINOR_VERSION,       // 0x03
  .lastGpioState  = 0 ,                   // 0x04
  .numberOfRelays = 16,                   // 0x05
  .filler =  {0xFF, 0xFF, 0xFF}           // 0x06 .. 0x08
};
  //----
byte  I2CMUX_COMMAND         = 0xF0 ; // -> this is NOT a "real" register!!


//Cast 32bit address of the object registerStack with uint8_t so we can increment the pointer
uint8_t           *registerPointer = (uint8_t *)&registerStack;

volatile byte     registerNumber; 

bool  waitForFirstCommand = true;
uint32_t toggleTimer;

//                    x  <-------PD--------->  <---PB--->  <-----PC----->
//         relay      0, 1, 2, 3, 4, 5, 6, 7,  8, 9,10,11, 12,13,14,15,16
int8_t p2r16[17] = { -1, 1, 0, 3, 2, 5, 4, 7,  6, 9, 8, 13,10,15,14,17,16} ;  // 16 relay board
int8_t  p2r8[17] = { -1, 1, 3, 5, 7, 9,13,15, 17, 0, 0, 0,  0, 0, 0, 0, 0} ;  //  8 relay board


//------ commands ----------------------------------------------------------
enum  {  CMD_PINMODE, CMD_DIGITALWRITE, CMD_DIGITALREAD
       , CMD_TESTRELAYS, CMD_NUMRELAYS
       , CMD_READCONF, CMD_WRITECONF, CMD_REBOOT 
      };

//==========================================================================
void testRelays()
{
  digitalWrite(0, LOW);
  //Serial.println();
  for (int x=0; x<10;x++) {
    for (int i=1; i<=16; i++) {
      digitalWrite(p2r16[i], LOW);
      delay(1000);
    }
    delay(1000);
    for (int i=1; i<=16; i++) {
      digitalWrite(p2r16[i], HIGH);
      delay(250);
    }
    delay(1000);
  }

} //  testRelays()


//==========================================================================
void reBoot()
{
  //wdt_reset();
  //while (true) {}

} //  reBoot()


//==========================================================================
void setup()
{
  // MCUSR=0x00; //<<<-- keep this in!
  // wdt_disable();

  //Serial.begin(115200);     // this is PD0 (RX) and PD1 (TX)
  //Serial.print("\n\n(re)starting I2C Mux Slave ..");
  PORTB = B00100111;  // PB0=D08,PB1=D09,PB2=D10,PB5=D13
  DDRB  = B00100111;  // set GPIO pins on PORTB to OUTPUT
  PORTC = B00001111;  // PC0=D12,PC1=D13,PC2=D14,PC3=D15
  DDRC  = B00001111;  // set GPIO pins on PORTC to OUTPUT  
  PORTD = B11111111;  // PD0=D00,PD1=D01,PC2=D02,PC3=D03,PC4=D04,PD5=D05,PD6=D06,PD7=D07
  DDRD  = B11111111;  // set all GPIO pins on PORTD to OUTPUT 

  registerStack.lastGpioState = LOW;    

  readConfig();
  delay(2000);
  
  if (registerStack.numberOfRelays == 8)
  {
    for (int p=1; p<=8; p++) {
      digitalWrite(p2r8[p], _RELAY_ON); 
      delay(200); 
      digitalWrite(p2r8[p], _RELAY_OFF);  // relay 8!!!
      delay(200);
    }
  }
  else
  {
    for (int p=1; p<=16; p++) {
      digitalWrite(p2r16[p], _RELAY_ON); 
      delay(200); 
      digitalWrite(p2r16[p], _RELAY_OFF);  // relay 8!!!
      delay(200);
    }
  }

  //testRelays();

  startI2C();
  
  // inactiveTimer = millis();

} // setup()


//==========================================================================
void loop()
{
  /*** it just doesn't work :-(
  if ((millis() - inactiveTimer) > MAX_INACTIVE_TIME) {
    wdt_enable(WDTO_1S);  
    wdt_reset();
  } else {
    wdt_disable();
  }
  ***/
  if (waitForFirstCommand)
  {
    if ((millis()-toggleTimer) > 2500)
    {
      toggleTimer = millis();
      digitalWrite(p2r16[16], !digitalRead(p2r16[16])); 
    }
  }

} // loop()


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
