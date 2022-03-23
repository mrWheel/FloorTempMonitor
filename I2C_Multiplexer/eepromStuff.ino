/*
***************************************************************************
**
**    Program : eepromStuff (part of I2C_Multiplexer)
**
**    Copyright (C) 2020 Willem Aandewiel
**
**    TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

//--------------------------------------------------------------------------
static void readConfig()
{
  registerLayout registersSaved;

  //Serial.println("readConfig()..");

  eeprom_read_block(&registersSaved, 0, sizeof(registersSaved));
  //--- the registerStack will not change in the same Major Version --
  if ( registersSaved.majorRelease == _MAJOR_VERSION ) {
    registerStack = registersSaved;

  //--- the Major version has changed and there is no way of -----
  //--- knowing the registerStack has not changed, so rebuild ----
  } else {  
    registerStack.whoAmI          = _I2C_DEFAULT_ADDRESS;
    registerStack.majorRelease    = _MAJOR_VERSION;
    registerStack.minorRelease    = _MINOR_VERSION;
    registerStack.numberOfRelays  = 16;

    for(int i=0;i<registerStack.numberOfRelays;i++) 
    {
      digitalWrite(p2r16[1], _RELAY_ON);
      delay(300); 
      digitalWrite(p2r16[1], _RELAY_OFF); 
      delay(300); 
    }

    writeConfig();
  }

  for(int i=0;i<registerStack.numberOfRelays;i++) 
  {
    digitalWrite(p2r16[1], _RELAY_ON);
    delay(500); 
    digitalWrite(p2r16[1], _RELAY_OFF); 
    delay(200); 
  }

} // readConfig()

//--------------------------------------------------------------------------
static void writeConfig()
{
  //Serial.println("writeConfig()..");
  eeprom_update_block(&registerStack, 0, sizeof(registerStack));
  //eeprom_write_block(&registerStack, 0, sizeof(registerStack));

  for(int i=0;i<registerStack.numberOfRelays;i++) 
  {
    digitalWrite(p2r16[1], _RELAY_ON);
    delay(500); 
    digitalWrite(p2r16[1], _RELAY_OFF); 
    delay(500); 
  }

} // writeConfig()


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
