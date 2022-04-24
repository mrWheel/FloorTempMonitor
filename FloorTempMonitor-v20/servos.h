/*
 * servos.h
 *
 */

// constants

#ifndef SERVOS_H
#define SERVOS_H

#include "I2C_MuxLib.h"

#define I2C_MUX_ADDRESS       0x48    // the 7-bit address 

#define _MAX_SERVOS           10

#define SERVO_FIRST            1
#define SERVO_LAST             9

#define FORALL(THING, loopvar)       for(int8_t loopvar = THING##_FIRST ; loopvar <= THING##_LAST ; loopvar++)

#define CLOSE_SERVO           HIGH    
#define OPEN_SERVO            LOW

#define HEAT_RELAY            14      // show heat demand also to replace OT with simple ON/OFF
#define HEARTBEAT_RELAY       16

#define SERVO_IS_CLOSING_LOCK 5     // number of ticks to wait before next change
#define SERVO_IS_OPENING_LOCK 5

#define _HOUR_CLOSE_ALL       3     // @03:00 start closing servos one-by-one. Don't use 1:00!

enum  e_servoState            { SERVO_IS_OPEN, SERVO_IS_CLOSED, SERVO_IS_OPENING, SERVO_IS_CLOSING, ERROR };
enum  e_close_reason          { ROOM_HOT = 0x01, WATER_HOT = 0x02, MAINT=0x04 };

class servos {

  // MUX stuff
  
	I2CMUX    I2cExpander;   //Create instance of the I2CMUX object

	int       connectionMuxLostCount=0;
  int       connectionMuxEstablished=0;
  
  // lubrication state
  
  int8_t    cycleNu=0;
  boolean   cycledToday = true; 

  // maintain counter of open loops/servos
  
  int8_t    openLoops=0;

  // state info for each servo
  
  uint8_t   servoState[_MAX_SERVOS];    // one of the e_servoState values
  uint8_t   closeCount[_MAX_SERVOS];    // counter for closings, needed for lubrication
  uint8_t   closeReason[_MAX_SERVOS];   // reason why servo closed (e_close_reason)
  uint8_t   servoStateLock[_MAX_SERVOS];// how many more 'ticks' is this servo locked in SERVO_IS_*ING position

  bool      _isLocked(uint8_t s);       // determine and log locked state of servo

  void      _close(uint8_t s);          // called by Close to handle actual closing
  void      _open(uint8_t s);           // called by Open to handle actual opening
  void      _align(uint8_t s);          // called by align for each servo
  void      _lubricate(uint8_t cycleNr); // called by lubricate for each servo

  void      align();                    // aligns physical servos with internal admin
  void      lockReduce();               // reduces servoStateLock with one tick
  void      lubricate();                // handles daily lubrication

  void      countOpenLoops();           // update openLoops

  // internal helper functions for I2C Mux
  
  bool      setupI2C_Mux();             
  void      checkI2C_Mux();

public:

  // getters for private attributes
  
  uint8_t   State(uint8_t s)  { return servoState[s];  };
  uint8_t   Reason(uint8_t s) { return closeReason[s]; }; 
  uint8_t   OpenLoops()       { return openLoops;  };

  int       ConnectionMuxLostCount() { return connectionMuxLostCount; };
  
  // verbs
  
  void      Init();
  void      Loop();
 
	void      Close(uint8_t s, uint8_t reason);
	void      Open (uint8_t s, uint8_t reason);

};

extern servos AllServos;
#endif
