/*
***************************************************************************
**  Program  : servos, part of FloorTempMonitor
**  Version  : v0.6.9
**
**  Copyright 2019, 2020, 2021, 2022 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

#include "I2C_MuxLib.h"
#include "servos.h"
#include "Debug.h"

// #define SPEEDUP

#include "timers.h"

#define PROFILING
#define PROFILING_THRESHOLD 45

#include "profiling.h"
#include "ledpin.h"

// speedup 60 times ==> seconds where should be minutes!

#ifdef SPEEDUP
static int hour() {
  return millis() / ( 1000 * 60 );
}
#else
extern int hour();
#endif

#define CYCLE_TIME (hour() >= _HOUR_CLOSE_ALL)
#define CYCLE_RESETTIME (hour() < _HOUR_CLOSE_ALL)

__TIMER(checkI2C, 2);
__TIMER(lockReduce, 1);
__TIMER(servoAlignment, 5);
__TIMER(lubricationCheck, 5)
__TIMER(openLoopCount, 1);

__TIMER(heartBeatRelay, 2);

#define ASSERT_SERVO_NR(__s__) { if (__s__ <1 || __s__ >= _MAX_SERVOS) DebugTf("ERROR: ServoNr [%02d] out of range\n", __s__); }

void servos::Init()
{
  setupI2C_Mux();

  delay(500);

  //for (byte i = 1 ; i < _MAX_SERVOS ; i++) // hjm what is with servo 0 ?
  FORALL(SERVO, i)
  {
    ASSERT_SERVO_NR(i);
    
    servoState[i] = 0; // I2cExpander.digitalRead(i);
    closeCount[i] = 0;
    servoStateLock[i] = 0;
    closeReason[i] = 0;
  }
}

void servos::Loop()
{
  if  (DUE(checkI2C))
    timeThis (checkI2C_Mux());

  if  (DUE(lockReduce))
    timeThis (lockReduce());

  if  (DUE(servoAlignment))
    timeThis (align());

  if  (DUE(lubricationCheck))
    timeThis (lubricate());

  if  (DUE(openLoopCount))
    timeThis ( countOpenLoops() );

#ifdef SPEEDUP

 // hjm probably temporary for test board only

  if (DUE(heartBeatRelay))
  {
    I2cExpander.digitalWrite(HEARTBEAT_RELAY, CLOSE_SERVO);
  }
  if (SINCE(heartBeatRelay) > 200)
  {
    I2cExpander.digitalWrite(HEARTBEAT_RELAY, OPEN_SERVO);
  }
#endif
}

void servos::lockReduce()
{
  DebugTln("-- LockTick --");

  for (uint8_t i = 1 ; i < _MAX_SERVOS ; i++)
  {
    ASSERT_SERVO_NR(i);
    
    if (servoStateLock[i] > 0)
    {
      servoStateLock[i]--;
      if (servoStateLock[i] == 0)
      {
        DebugTf("Servo [%02d] is unlocked now -- ", i);
        if ( servoState[i] == SERVO_IS_CLOSING)
        {
          servoState[i] = SERVO_IS_CLOSED;
          Debugln("Closed");
        }
        if ( servoState[i] == SERVO_IS_OPENING)
        {
          servoState[i] = SERVO_IS_OPEN;
          Debugln("Open");
        }
      }
    }
  }
}

void servos::countOpenLoops()
{
  openLoops = 0;

  // SERVO[0] does not exist!!

  for (uint8_t i = 1 ; i < _MAX_SERVOS ; i++)
    if (servoState[i] == SERVO_IS_OPEN || servoState[i] == SERVO_IS_OPENING)
      openLoops++;

  DebugTf("Open loop count now is %d\n", openLoops);

  I2cExpander.digitalWrite(HEAT_RELAY, openLoops > 0);
}

bool servos::_isLocked(uint8_t s)
{
  ASSERT_SERVO_NR(s);
  
  if (servoStateLock[s] != (uint8_t) 0)
  {
    DebugTf("Servo [%02d] has %d ticks to go before state change allowed\n", s, servoStateLock[s]);
    return 1;
  }
  return 0;
}

// Open & Close with reason parameter do NOT actually open/close servo
// only used to register desired state

void servos::Close(uint8_t s, uint8_t reason)
{
  // see if not already closed/ing for this reason
  ASSERT_SERVO_NR(s);
  if (!(closeReason[s] & reason))
  {
    closeReason[s] |= reason;
    _close(s);
  }
}

void servos::_close(uint8_t s)
{
  ASSERT_SERVO_NR(s);
  
  if (_isLocked(s))
    return;

  if (closeReason[s] != 0) // there is a reason to close
  {
    // if ( I2cExpander.digitalRead(s) == OPEN_SERVO ) // not closed allready?
    if(servoState[s] != SERVO_IS_CLOSED)
    {
      delay(100);
      I2cExpander.digitalWrite(s, CLOSE_SERVO);
      delay(100);

      servoState[s] = SERVO_IS_CLOSING;
      servoStateLock[s] = SERVO_IS_CLOSING_LOCK;
      closeCount[s]++;

      DebugTf("Servo [%2d] changed to IS_CLOSING state\n", s);
    } else {
      DebugTf("Servo [%2d] already in IS_CLOSING state\n", s);
    }
  }
}

void servos::Open(uint8_t s, uint8_t reason)
{
  // check if reason is already active

  ASSERT_SERVO_NR(s);
  
  if (closeReason[s] & reason)
  {
    closeReason[s] &= ~reason;
    _open(s);
  }
}

void servos::_open(uint8_t s)
{
  ASSERT_SERVO_NR(s);
  
  if (_isLocked(s))
    return;

  if (closeReason[s] == 0)
  {
    // if ( I2cExpander.digitalRead(s) == OPEN_SERVO ) // not open allready?
    if(servoState[s] == SERVO_IS_OPEN)
    {
      DebugTf("Servo [%2d] already in OPEN(ING) state\n", s);
      return;
    }
    
    delay(100);
    I2cExpander.digitalWrite(s, OPEN_SERVO);
    delay(100);
    
    servoState[s] = SERVO_IS_OPENING;
    servoStateLock[s] = SERVO_IS_OPENING_LOCK;
    DebugTf("Servo [%2d] changed to OPENING state\n", s);
  } else {
    DebugTf("[%2d] STAY closed for reason %d\n", s, closeReason[s]);
  }
}

void servos::_align(uint8_t s)
{
  // compare servo state with administrated servo state
  ASSERT_SERVO_NR(s);
  
  if (_isLocked(s))
    return;

  bool actualOpen = (I2cExpander.digitalRead(s) == OPEN_SERVO);
  
  bool desiredOpen = (closeReason[s] == 0);
  //bool actualOpen  = (servoState[s]  == SERVO_IS_OPEN);
  
  if (actualOpen != desiredOpen) {
    DebugTf("Servo [%02d] misaligned [actual=%c, desired=%c]\n", s, actualOpen ? 'O' : 'C', desiredOpen ? 'O' : 'C');
    I2cExpander.digitalWrite(s, desiredOpen ? OPEN_SERVO : CLOSE_SERVO);
  } else
    DebugTf("Servo [%02d] is in sync [actual=%c, desired=%c]\n", s, actualOpen ? 'O' : 'C', desiredOpen ? 'O' : 'C');

}

void servos::align()
{
  DebugTf("Aligning servos ...\n");

  //for (int8_t i = 1 ; i < _MAX_SERVOS ; i++)
  FORALL(SERVO,i)
  {
    _align(i);
  }
}

void servos::_lubricate(uint8_t cycleNr)
{
  DebugTf("lubricate starting at [%02d]\n", cycleNr);

  // servo is moving --> no MAINT cycle needed

  if ( servoState[cycleNr] == SERVO_IS_OPENING ||
       servoState[cycleNr] == SERVO_IS_CLOSING ) 
  {
    DebugTf("Servo [%02d] in %s\n", cycleNr, (servoState[cycleNr] == SERVO_IS_OPENING) ? "Opening" : "Closing");
    if ( closeReason[cycleNr] & MAINT )
    {
      DebugTf("Servo [%02d] moves for MAINT purposes\n", cycleNr);
    } else {
      DebugTf("Servo [%02d] moves for other than MAINT purposes\n", cycleNr);
      cycleNu = cycleNr + 1; // proceed to next servo
    }
    return;
  }

  for ( ; cycleNr <  _MAX_SERVOS  ; cycleNr++) {
    
    DebugTf("Servo [%02d] is checked for movement\n", cycleNr);

    if (closeCount[cycleNr] == 0) 
    {
      DebugTf("Servo [%02d] never closed - now closing for MAINT\n", cycleNr);
      Close(cycleNr, MAINT);
      return;
    }

    if (closeCount[cycleNr] == 1 && servoState[cycleNr] == SERVO_IS_CLOSED) 
    {
      DebugTf("Servo [%02d] closed once & never opened again - force open\n", cycleNr);
      Open(cycleNr, MAINT | ROOM_HOT | WATER_HOT); // force open for by un-closing for every reason
      return;
    }
    
    DebugTf("Servo [%02d] closed %d times\n", cycleNr, closeCount[cycleNr]);

    cycleNu = cycleNr + 1; // proceed to next servo
    return;
  }

  DebugTln("Done lubricating all Servos!");
  
  //for (int8_t s = 0; s < _MAX_SERVOS; s++) 
  FORALL(SERVO, s)
    closeCount[s] = 0;
  
  cycledToday = true; // stop for today

} // cycleAllNotUsedServos()

void servos::lubricate()
{
  if ( !cycledToday && CYCLE_TIME )
  {
    if ( !cycledToday && cycleNu == 0)
      cycleNu = 1;  // start at servo 1

    _lubricate(cycleNu);

  } else if ( cycledToday && CYCLE_RESETTIME ) {
    DebugTln("Reset 24HR lubrication cycle");
    cycledToday = false;
    cycleNu = 0;
  }

} // handleCycleServos()

void servos::checkI2C_Mux()
{
  byte whoAmI /*, majorRelease, minorRelease */ ;

  if ( I2cExpander.connectedToMux())
  {
    if ( (whoAmI = I2cExpander.getWhoAmI()) == I2C_MUX_ADDRESS)
    {
      if (I2cExpander.digitalRead(0) == CLOSE_SERVO)
      {
        I2cExpander.digitalWrite(0, OPEN_SERVO);
      }
      digitalWrite(LED_RED, LED_OFF);
      return;
    }
    else
    {
      DebugTf("Connected to different I2cExpander %x\r\n", whoAmI );
    }
  }
  else
  {
    DebugTln("ERROR: Connection lost to I2cExpander");
  }

  connectionMuxLostCount++;
  digitalWrite(LED_RED, LED_ON);

  if (setupI2C_Mux())
  {
    DebugTf("OK: Connection reset to I2cExpander after %d tries\n", connectionMuxLostCount);
    connectionMuxLostCount = 0;
  }

} // checkI2C_MUX();

bool servos::setupI2C_Mux()
{
  byte whoAmI;

  DebugT("Setup Wire ..");
  
  //Wire.begin(_SDA, _SCL); // join i2c bus (address optional for master)
  
  Wire.begin();
  Wire.setClock(100000L); // <-- don't make this 400000. It won't work
  Debugln(".. done");

  if (I2cExpander.begin())
  {
    DebugTln("Connected to the I2C multiplexer!");
    connectionMuxEstablished++;
  }
  else
  {
    DebugTln("Not Connected to the I2C multiplexer !ERROR!");
    delay(100);
    return false;
  }
  whoAmI = I2cExpander.getWhoAmI();
  if (whoAmI != I2C_MUX_ADDRESS) {
    digitalWrite(LED_RED, LED_ON);
    return false;
  }
  digitalWrite(LED_RED, LED_OFF);
  //-- switch heater relay OFF
  I2cExpander.digitalWrite(HEAT_RELAY, OPEN_SERVO);

  return true;

} // setupI2C_Mux()

servos AllServos;

/***************************************************************************

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to permit
  persons to whom the Software is furnished to do so, subject to the
  following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
  OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
  THE USE OR OTHER DEALINGS IN THE SOFTWARE.

***************************************************************************/
