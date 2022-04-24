#ifndef ROOMS_H
#define ROOMS_H

#define MAXROOMS                7
#define MAX_UFHLOOPS_PER_ROOM   2
#define MAX_NAMELEN             20
#define _MAX_NAME_LEN           12

// Compare with Target (actual or prognosed)

#define COMP_WITH_TARGET Rooms.actualTemp
//#define COMP_WITH_TARGET Rooms.prog2HR


#include <esp-knx-ip.h>

typedef char RoomName_t[_MAX_NAME_LEN];
typedef int  ServoList_t[2];

class rooms {

    void Print(byte i);
    void getTemps();
    void setDesiredServoState(int8_t roomNr);
    void setDesiredServoState();

  public:
  
    RoomName_t    Name[MAXROOMS];       // display name matches SensorNames
    ServoList_t   Servos[MAXROOMS];     // array of indices in Servos array (max 2)
    
    float   targetTemp[MAXROOMS];                        // target temp for room
    float   actualTemp[MAXROOMS];                        // actual room temp
    float   prog2HR[MAXROOMS];
    float   trend[MAXROOMS];
    bool    heatingState[MAXROOMS];                      // false is not heating
   
    address_t GA_actual[MAXROOMS],
              GA_target[MAXROOMS],
              GA_state[MAXROOMS];

    int noRooms = 0;

    void Init();
    void Loop();

    void Load();
    void Save();

    float setTargetTemp(const char * roomName, float roomTarget);
    float setTargetTemp(int8_t roomNr, float roomTarget);

};

extern rooms Rooms;

#endif
