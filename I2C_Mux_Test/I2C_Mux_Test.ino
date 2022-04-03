/*
***************************************************************************
**
**  Program     : I2C_Mux_Test [Arduino UNO]
*/
#define _FW_VERSION  "v0.4 (05-04-2020)"
/*
**  Description : Test FTM I2C Relay Multiplexer
**
**  Copyright (c) 2020 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/


#define I2C_MUX_ADDRESS      0x48    // the 7-bit address 
//#define _SDA                  4    // A4!!!
//#define _SCL                  5    // A5!!!
#define LED_ON                  0
#define LED_OFF               255
#define TESTPIN                13
#define LED_RED                12
#define LED_GREEN              13
#define LED_WHITE              14

#define LOOP_INTERVAL       10000
#define PROFILING

//#define Debugf      Serial.printf

#include "I2C_MuxLib.h"


I2CMUX  relay; //Create instance of the I2CMUX object

byte          whoAmI;
byte          majorRelease, minorRelease;
bool          I2C_MuxConnected;
int           count4Wait = 0;
int           numRelays; 
uint32_t      loopTimer, switchBoardTimer, builtinLedTimer, ledRedTimer, ledGreenTimer, ledWhiteTimer;
uint32_t      offSet;
String        command;

//===========================================================================================
void requestEvent()
{
  // nothing .. yet
}

//===========================================================================================
//assumes little endian
void printRegister(size_t const size, void const * const ptr)
{
  unsigned char *b = (unsigned char*) ptr;
  unsigned char byte;
  int i, j;
  Serial.print(F("["));
  for (i=size-1; i>=0; i--) {
    for (j=7; j>=0; j--) {
      byte = (b[i] >> j) & 1;
      Serial.print(byte);
    }
  }
  Serial.print(F("] "));
} // printRegister()


//===========================================================================================
void displayPinState()
{
  Serial.print("  Pin: ");
  for (int p=1; p<=16; p++) {
    Serial.print(p % 10);
  }
  Serial.println();

  Serial.print("State: ");
  for (int p=1; p<=16; p++) {
    int pState = relay.digitalRead(p);
    if (pState == HIGH)
          Serial.print("H");
    else  Serial.print("L");
  }
  Serial.println();
  
} //  displayPinState()


void cycleRelays()
{
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    //Serial.print("\n["); Serial.print(count4Wait); Serial.println("]");
    whoAmI       = relay.getWhoAmI();
    if (whoAmI != I2C_MUX_ADDRESS) {
      Serial.println("No connection with Multiplexer .. abort!");
      offSet = 0;
      return;
    }
    //offSet += 1000;
    Serial.print("\nWaiting is now ["); Serial.print((LOOP_INTERVAL + offSet)/1000); Serial.println("] seconds");
    //Serial.println("getMajorRelease() ..");
    majorRelease = relay.getMajorRelease();
    //Serial.println("getMinorRelease() ..");
    minorRelease = relay.getMinorRelease();
    
    Serial.print("\nSlave say's he's [");  Serial.print(whoAmI, HEX); 
    Serial.print("] Release[");            Serial.print(majorRelease);
    Serial.print(".");                     Serial.print(minorRelease);
    Serial.println("]");

    numRelays = relay.getNumRelays();
    Serial.print("Board has [");  Serial.print(numRelays);
    Serial.println("] relays\r\n"); 

    for (int p=1; p<=numRelays; p++) {
      Serial.print(p % 10);
    }
    Serial.println();
    for (int p=1; p<=numRelays; p++) {
      yield();
      relay.digitalWrite(p, HIGH);
      Serial.print("H");
      delay(250);
    }
    delay(100);
    Serial.println();

    for (int p=1; p<=16; p++) {
      Serial.print(p % 10);
    }
    Serial.println();
    for (int p=1; p<=16; p++) {
      yield();
      relay.digitalWrite(p, LOW);
      Serial.print("L");
      delay(250);
    }

    delay(100);
    Serial.println("\r\n");

} // cycleRelays()


//===========================================================================================
bool Mux_Status()
{
  whoAmI       = relay.getWhoAmI();
  if (whoAmI != I2C_MUX_ADDRESS) 
  {
    Serial.print("\nSlave say's he's [");  Serial.print(whoAmI, HEX); Serial.println(']');
    return false;
  }
  displayPinState();
  //Serial.println("getMajorRelease() ..");
  majorRelease = relay.getMajorRelease();
  //Serial.println("getMinorRelease() ..");
  minorRelease = relay.getMinorRelease();
    
  Serial.print("\nSlave say's he's [");  Serial.print(whoAmI, HEX); 
  Serial.print("] Release[");            Serial.print(majorRelease);
  Serial.print(".");                     Serial.print(minorRelease);
  Serial.println("]");
  numRelays = relay.getNumRelays();
  Serial.print("Board has [");  Serial.print(numRelays);
  Serial.println("] relays\r\n"); 

} // Mux_Status()



//===========================================================================================
bool setupI2C_Mux()
{
  Serial.print(F("Setup Wire .."));
  //Wire.begin(_SDA, _SCL); // join i2c bus (address optional for master)
  Wire.begin();
  Wire.setClock(100000L); // <-- don't make this 400000. It won't work
  Serial.println(F(".. done"));

  if (relay.begin()) {
    Serial.println("Connected to the relay multiplexer!");
  } else {
    Serial.println("Not Connected to the relay multiplexer !ERROR!");
    delay(1000);
    return false;
  }
  whoAmI       = relay.getWhoAmI();
  if (whoAmI != I2C_MUX_ADDRESS) {
    return false;
  }
  displayPinState();
  delay(1000);
  
  return true;
  
} // setupI2C_Mux()


//===========================================================================================
void readCommand()
{
  String command = "";

  if (!Serial.available()) {
    return;
  }
  Serial.setTimeout(500);  // ten seconds
#ifdef ESP8266
  char buff[100] = {};
  Serial.readBytesUntil('\r', buff, sizeof(buff));
  command = String(buff);
#else
  command = Serial.readStringUntil("\r");
#endif
  command.toLowerCase();
  for (int i = 0; i < command.length(); i++)
  {
    if (command[i] < ' ' || command[i] > '~') command[i] = 0;
  }
  Serial.print("command["); Serial.print(command); Serial.println("]");

  if (command == "board=8")   {numRelays =  8; relay.setNumRelays(8); }
  if (command == "board=16")  {numRelays = 16; relay.setNumRelays(16); }
  if (command == "0=1")       relay.digitalWrite(0,  HIGH); 
  if (command == "0=0")       relay.digitalWrite(0,  LOW); 
  if (command == "1=1")       relay.digitalWrite(1,  HIGH); 
  if (command == "1=0")       relay.digitalWrite(1,  LOW); 
  if (command == "2=1")       relay.digitalWrite(2,  HIGH); 
  if (command == "2=0")       relay.digitalWrite(2,  LOW); 
  if (command == "3=1")       relay.digitalWrite(3,  HIGH); 
  if (command == "3=0")       relay.digitalWrite(3,  LOW); 
  if (command == "4=1")       relay.digitalWrite(4,  HIGH); 
  if (command == "4=0")       relay.digitalWrite(4,  LOW); 
  if (command == "5=1")       relay.digitalWrite(5,  HIGH); 
  if (command == "5=0")       relay.digitalWrite(5,  LOW); 
  if (command == "6=1")       relay.digitalWrite(6,  HIGH); 
  if (command == "6=0")       relay.digitalWrite(6,  LOW); 
  if (command == "7=1")       relay.digitalWrite(7,  HIGH); 
  if (command == "7=0")       relay.digitalWrite(7,  LOW); 
  if (command == "8=1")       relay.digitalWrite(8,  HIGH); 
  if (command == "8=0")       relay.digitalWrite(8,  LOW); 
  if (command == "9=1")       relay.digitalWrite(9,  HIGH); 
  if (command == "9=0")       relay.digitalWrite(9,  LOW); 
  if (command == "10=1")      relay.digitalWrite(10, HIGH); 
  if (command == "10=0")      relay.digitalWrite(10, LOW); 
  if (command == "11=1")      relay.digitalWrite(11, HIGH); 
  if (command == "11=0")      relay.digitalWrite(11, LOW); 
  if (command == "12=1")      relay.digitalWrite(12, HIGH); 
  if (command == "12=0")      relay.digitalWrite(12, LOW); 
  if (command == "13=1")      relay.digitalWrite(13, HIGH); 
  if (command == "13=0")      relay.digitalWrite(13, LOW); 
  if (command == "14=1")      relay.digitalWrite(14, HIGH); 
  if (command == "14=0")      relay.digitalWrite(14, LOW); 
  if (command == "15=1")      relay.digitalWrite(15, HIGH); 
  if (command == "15=0")      relay.digitalWrite(15, LOW); 
  if (command == "16=1")      relay.digitalWrite(16, HIGH); 
  if (command == "16=0")      relay.digitalWrite(16, LOW); 
  if (command == "all=1")
  {
    for (int i=1; i<=numRelays; i++)
    {
      relay.digitalWrite(i, HIGH); 
    }
  }
  if (command == "all=0")
  {
    for (int i=1; i<=numRelays; i++)
    {
      relay.digitalWrite(i, LOW); 
    }
  }
  if (command == "status")      Mux_Status();
  if (command == "pinstate")    displayPinState();
  if (command == "test")        cycleRelays();
  if (command == "testrelays")  relay.writeCommand(1<<CMD_TESTRELAYS);
  if (command == "writeconfig") relay.writeCommand(1<<CMD_WRITECONF);
//if (command == "reboot")      relay.writeCommand(1<<CMD_REBOOT);
  
} // readCommand()


//===========================================================================================
void setup()
{
  Serial.begin(115200);
  Serial.println(F("\r\nStart I2C-Relay-Multiplexer Test ....\r\n"));

  //Serial.printf("        HIGH [%d]\n", HIGH);
  //Serial.printf("        LOW  [%d]\n", LOW);
  //Serial.printf("       INPUT [%d]\n", INPUT);
  //Serial.printf("INPUT_PULLUP [%d]\n", INPUT_PULLUP);
  //Serial.printf("      OUTPUT [%d]\n", OUTPUT);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LED_ON);
  pinMode(LED_RED, OUTPUT);
  digitalWrite(LED_RED, LED_ON);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_GREEN, LED_ON);
  pinMode(LED_WHITE, OUTPUT);
  digitalWrite(LED_WHITE, LED_ON);

  I2C_MuxConnected = setupI2C_Mux();
  I2C_MuxConnected = true;
  loopTimer = millis() + LOOP_INTERVAL;
  Mux_Status();
  Serial.println(F("setup() done .. \n"));

} // setup()


//===========================================================================================
void loop()
{
  if (!I2C_MuxConnected)
  {
    delay(2500);
    I2C_MuxConnected = setupI2C_Mux();
    Mux_Status();
  }
  /*
  if ((millis() - loopTimer) > (LOOP_INTERVAL + offSet)) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    cycleRelays();
    loopTimer = millis();
  }
  */
  readCommand();
  /**
  if ((millis() - switchBoardTimer) > 20000) {
    switchBoardTimer = millis();
    if (numRelays == 16)
          numRelays =  8;
    else  numRelays = 16;
    relay.setNumRelays(numRelays);
  }
  **/
  /**
  if ((millis() - ledGreenTimer) > 3500) {
    ledGreenTimer = millis();
    digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
  }
  if ((millis() - ledWhiteTimer) > 500) {
    ledWhiteTimer = millis();
    digitalWrite(LED_WHITE, !digitalRead(LED_WHITE));
    digitalWrite(LED_BUILTIN, LED_OFF);
  }
  **/
  
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
