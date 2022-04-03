/*
***************************************************************************
**
**    Program : I2Cstuff (part of I2C_Multiplexer)
**
**    Copyright (C) 2020 Willem Aandewiel
**
**    TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

//------------------------------------------------------------------
void startI2C()
{
  Wire.end();

  Wire.begin(registerStack.whoAmI);
  //Wire.begin(_I2C_DEFAULT_ADDRESS);
  Wire.setClock(100000L);

  
  // (Re)Declare the Events.
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  // wdt_reset();

} // startI2C()


//------------------------------------------------------------------
boolean isConnected()
{
  Wire.beginTransmission((uint8_t)0);
  if (Wire.endTransmission() != 0) {
    return (false); // Master did not ACK
  }
  // wdt_reset();
  return (true);

} // isConnected()


//------------------------------------------------------------------
void processCommand(byte command)
{
  byte GPIO_PIN, PINMODE, HIGH_LOW, GPIOSTATE;

  //Serial.print("Processing command ["); Serial.print(command, BIN); Serial.println("]");
  if ((command & (1<<CMD_PINMODE))) {
    //Serial.print("Command pinMode("); 
    GPIO_PIN = Wire.read();
    //Serial.print(GPIO_PIN);
    //Serial.print(", ");
    PINMODE = Wire.read();
    //Serial.print(HIGH_LOW);
    //Serial.println(")");
    if (registerStack.numberOfRelays == 8)
    {
      if (GPIO_PIN >= 1 && GPIO_PIN <= 8) 
      {
        pinMode(p2r8[GPIO_PIN], PINMODE);
      }
    }
    else
    {
      if (GPIO_PIN >= 1 && GPIO_PIN <= 16) 
      {
        pinMode(p2r16[GPIO_PIN], PINMODE);
      }
    }
  }
  else if ((command & (1<<CMD_DIGITALWRITE))) {
    //Serial.print("Command digitalWrite("); 
    GPIO_PIN = Wire.read();
    //Serial.print(GPIO_PIN);
    //Serial.print(", ");
    HIGH_LOW = Wire.read();
    //Serial.print(HIGH_LOW);
    //Serial.println(")");
    if (registerStack.numberOfRelays == 8)
    {
      if (GPIO_PIN >= 1 && GPIO_PIN <= 8) 
      {
        digitalWrite(p2r8[GPIO_PIN], !HIGH_LOW);
      }
    }
    else
    {
      if (GPIO_PIN >= 1 && GPIO_PIN <= 16) 
      {
        digitalWrite(p2r16[GPIO_PIN], !HIGH_LOW);
      }
    }
  }
  else if ((command & (1<<CMD_DIGITALREAD))) {
    //Serial.print("Command digitalRead("); 
    GPIO_PIN = Wire.read();
    //Serial.print(GPIO_PIN);
    //Serial.print(") => ");
    if (registerStack.numberOfRelays == 8)
    {
      if (GPIO_PIN >= 1 && GPIO_PIN <= 8) 
      {
        registerStack.lastGpioState = !digitalRead(p2r8[GPIO_PIN]);
        //Serial.print("State is ["); Serial.print(registerStack.lastGpioState); 
        //Serial.println("]");
      }
    }
    else
    {
      if (GPIO_PIN >= 1 && GPIO_PIN <= 16) 
      {
        registerStack.lastGpioState = !digitalRead(p2r16[GPIO_PIN]);
        //Serial.print("State is ["); Serial.print(registerStack.lastGpioState); 
        //Serial.println("]");
      }
    }
  }
  if ((command & (1<<CMD_TESTRELAYS))) 
  {
    testRelays();
  }
  if ((command & (1<<CMD_NUMRELAYS))) 
  {
    byte nor = Wire.read();
    if (nor == 8 || nor == 16)
    {
      registerStack.numberOfRelays = nor;
      writeConfig();
    }
  }
  if ((command & (1<<CMD_WRITECONF))) 
  {
    writeConfig();
  }
  if ((command & (1<<CMD_READCONF))) 
  {
    readConfig();
  }
  //-----> execute reBoot always last!! <-----
  if ((command & (1<<CMD_REBOOT))) 
  {
    //Serial.println("Got reboot command ...");
    reBoot();
  }

} // processCommand()

//------------------------------------------------------------------
//-- The master sends updated info that will be stored in the ------
//-- register(s)
//-- All Setters end up here ---------------------------------------
void receiveEvent(int numberOfBytesReceived) 
{
  waitForFirstCommand = false;
  //inactiveTimer = millis();

  //(void)numberOfBytesReceived;  // cast unused parameter to void to avoid compiler warning
  //Serial.println("receiveEvent() ..");
  
  registerNumber = Wire.read(); // Get the memory map offset from the user

  if (registerNumber == _CMD_REGISTER) {   // eeprom command
    byte command = Wire.read(); // read the command
    processCommand(command);
    return;
  }

  //Begin recording the following incoming bytes to the temp memory map
  //starting at the registerNumber (the first byte received)
  for (byte x = 0 ; x < numberOfBytesReceived - 1 ; x++) {
    byte temp = Wire.read();
    if ( (x + registerNumber) < sizeof(registerLayout)) {
      //Store the result into the register map
      if ((registerNumber + x) >= 0 && (registerNumber + x) <= 3) {
        //Serial.println("Trying to write to readonly register!");
        return;
      }
      registerPointer[registerNumber + x] = temp;
    }
  }

} //  receiveEvent()

//------------------------------------------------------------------
//-- The master aks's for the data from registerNumber onwards -----
//-- in the register(s) --------------------------------------------
//-- All getters get there data from here --------------------------
void requestEvent()
{
  waitForFirstCommand = false;
  
  // inactiveTimer = millis();

  //Serial.print("requestEvent() ..");

  //----- return max. 4 bytes to master, starting at registerNumber -------
  for (uint8_t x = 0; ( (x < 4) && (x + registerNumber) < (sizeof(registerLayout) - 1) ); x++) {
    if ((x + registerNumber) == 0x04) {
      //Serial.print(registerPointer[0x04]);
      //Serial.print(" ");
    }
    Wire.write(registerPointer[(x + registerNumber)]);
  }
  //Serial.println();
  //Serial.print(">> status       ["); Serial.print(registerPointer[0]);       Serial.println("]");
  //Serial.print(">> whoAmI       ["); Serial.print(registerPointer[1],HEX);   Serial.println("]");
  //Serial.print(">> majorRelease ["); Serial.print(registerPointer[2]); Serial.println("]");
  //Serial.print(">> minorRelease ["); Serial.print(registerPointer[3]); Serial.println("]");


} // requestEvent()


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
