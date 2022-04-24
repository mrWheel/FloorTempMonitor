#include <ArduinoJson.h>    // v6.19.3 - (see https://arduinojson.org/?utm_source=meta&utm_medium=library.properties)
#include <HTTPClient.h>     // v2.0.0 (part of Arduino ESP32 Core @2.0.2)
#include <FS.h>
#include <LittleFS.h>
#include "FloorTempMonitor.h"
#include "timers.h"
#include "LedPin.h"
#include "Debug.h"
#include "servos.h"

#include "rooms.h"

#include "profiling.h"

__TIMER(roomTempUpdate, 5);
__TIMER(servoUpdate,1);

#define tempMargin 0.1

void rooms::Init()
{
  Load();
  getTemps();
  setDesiredServoState();

}

void rooms::Loop()
{
  if (DUE(roomTempUpdate))
    getTemps();
    
  if (DUE(servoUpdate))
    setDesiredServoState();
  
}

void rooms::Load()
{
  File file = LittleFS.open("/rooms.ini", "r");
  int i = 0;

  char buffer[64];
  while (file.available()) {

    int l = file.readBytesUntil('\n', buffer, sizeof(buffer));
    buffer[l] = 0;

    int al, aa, am,
        tl, ta, tm,
        sl, sa, sm;

    sscanf(buffer, "%[^;];%d,%d;%f;%d/%d/%d;%d/%d/%d;%d/%d/%d",
           Rooms.Name[i],
           &Rooms.Servos[i][0],
           &Rooms.Servos[i][1],
           &Rooms.targetTemp[i],
           &aa, &al, &am,
           &ta, &tl, &tm,
           &sa, &sl, &sm);

    Rooms.GA_actual[i] = knx.GA_to_address (aa, al, am);
    Rooms.GA_target[i] = knx.GA_to_address (ta, tl, tm);
    Rooms.GA_state[i]  = knx.GA_to_address (sa, sl, sm);

    yield();
    Print(i);
    i++;
  }
  noRooms = i;
  file.close();
}

void rooms::Save()
{
  File file = LittleFS.open("/rooms.ini", "w");
  char buffer[64];

  for (int8_t i = 0 ; i < noRooms ; i++)
  {

    sprintf(buffer, "%s;%d,%d;%f;%d/%d/%d;%d/%d/%d;%d/%d/%d\n",
            Rooms.Name[i],
            Rooms.Servos[i][0],
            Rooms.Servos[i][1],
            Rooms.targetTemp[i],
            Rooms.GA_actual[i].ga.area,
            Rooms.GA_actual[i].ga.line,
            Rooms.GA_actual[i].ga.member,
            Rooms.GA_target[i].ga.area,
            Rooms.GA_target[i].ga.line,
            Rooms.GA_target[i].ga.member,
            Rooms.GA_state[i].ga.area,
            Rooms.GA_state[i].ga.line,
            Rooms.GA_state[i].ga.member
           );

    yield();
    //-aaw32- file.write(buffer, strlen(buffer));
    file.print(buffer);
    Print(i);
  }
  file.close(); //
}

void rooms::Print(byte i)
{
  DebugTf(" %-10.10s [%2d %2d] Actual/TargetTemp/prog2HR %4.1f/%4.1f/%4.1f knx %d/%d/%d | %d/%d/%d | %d/%d/%d\n",
          Rooms.Name[i],
          
          Rooms.Servos[i][0],
          Rooms.Servos[i][1],
          
          Rooms.actualTemp[i],
          Rooms.targetTemp[i],
          Rooms.prog2HR[i],

          Rooms.GA_actual[i].ga.area,
          Rooms.GA_actual[i].ga.line,
          Rooms.GA_actual[i].ga.member,
          
          Rooms.GA_target[i].ga.area,
          Rooms.GA_target[i].ga.line,
          Rooms.GA_target[i].ga.member,
          
          Rooms.GA_state[i].ga.area,
          Rooms.GA_state[i].ga.line,
          Rooms.GA_state[i].ga.member);
}

void rooms::getTemps()
{
  int apiRC;
  static WiFiClient apiWiFiclient;

  // Make IP address of homewizard

  byte IP[] = { 192, 168, 2, 24};

  digitalWrite(LED_WHITE, LED_ON);  // WHITE LED IS ON WHILE PROCESSING ROOM TEMP SENSORS

  timeThis(apiRC = apiWiFiclient.connect(IP, 80));
  delay(500);

  timeThis(apiWiFiclient.print("GET /geheim1967/telist HTTP/1.0\r\n\r\n"));
  yield();

  for ( int8_t x = 0 ; !apiWiFiclient.available() && x < 10 ; x++)
    delay(100);


  if ( apiWiFiclient.available()  && apiWiFiclient.find("response"))    // OK
  {
    char Response[1024];
    int seen = 0;

    while (seen < noRooms && apiWiFiclient.available() && apiWiFiclient.find("{") )
    {
      const char *ptr;

      yield();
      timeThis(apiWiFiclient.readBytesUntil('}',  Response, sizeof(Response)));

      ptr = Response;

      if (!strlen(ptr))
      {
        DebugTf("[HomeWizard] No response (left))\n");
        break;
      }

      // DebugTf("ptr = >%s<\n", ptr);
      while ( (ptr = strstr(ptr, "\"name\"")))
      {
        char jsonName[20];
        float jsonTemp;
        bool  nameFound = false;

        yield();

        sscanf(ptr, "\"name\":\"%[^\"]", jsonName);
        ptr = (char*) &ptr[7];

        //DebugTf("name found: >%s<\n", jsonName);

        if (!(ptr = strstr(ptr, "\"te\"")))
        {
          DebugTf("[HomeWizard] Incomplete JSON response, no 'te' found for %s\n", jsonName);
          break;
        }
        sscanf(ptr, "\"te\":%f", &jsonTemp);
        ptr = (char*) &ptr[5];

        // assign API result to correct Room

        for ( int8_t roomIndex = 0 ; roomIndex < noRooms ; roomIndex++)
        {
          yield();

          if (!strcmp(Rooms.Name[roomIndex], jsonName))
          {

            // update actual room temperature
            // let's keep track of the trend
            // hjm if(Rooms.actualTemp[roomIndex] != jsonTemp)
            {
              
              trend[roomIndex] = jsonTemp - Rooms.actualTemp[roomIndex];
              prog2HR[roomIndex] = Rooms.actualTemp[roomIndex] + 12 * trend[roomIndex];
              Rooms.actualTemp[roomIndex] = jsonTemp;
              //DebugTf("Match for room %s (%f) in room %s\n", jsonName, jsonTemp, Rooms[roomIndex].Name);
            }
            nameFound = true;
            Print(roomIndex);
            seen++;
            break; // found, lets not continue
          }
        }

        if (!nameFound)
          DebugTf("No match for room %s (%f)\n", jsonName, jsonTemp);
      }
    }
    digitalWrite(LED_WHITE, LED_OFF);

  } else {

    // WHEN NO HOMEWIZARD CONTACT WHITE LED STAYS ON

    DebugTf("[HomeWizard] API call failed with rc %d\n", apiRC);
    DebugTf("Assuming all rooms are ~ 20.0 +/- 1 degrees\n");

    for ( int8_t roomIndex = 0 ; roomIndex < noRooms ; roomIndex++)
    {
      float newTemp;
      
      newTemp = random(190, 210) / 10.0;
      trend[roomIndex] = newTemp - Rooms.actualTemp[roomIndex];
      prog2HR[roomIndex] = Rooms.actualTemp[roomIndex] + 12 * trend[roomIndex];
      Rooms.actualTemp[roomIndex] = newTemp;
    }
  }

  timeThis(apiWiFiclient.stop());
}

void rooms::setDesiredServoState(int8_t roomNr)
{ 
   // close servos based on room temperature?

  bool shouldClose;
  
  shouldClose = COMP_WITH_TARGET[roomNr] > (targetTemp[roomNr] - tempMargin);
  
  DebugTf("Setting desired state for valve(s) for room %02d based on %s set to %s\n", roomNr, "COMP_WITH_TARGET##", shouldClose ? "closed" : "opened");
  
  for (byte i = 0 ; i < 2 ; i++)
  {
    int8_t s;
    
    s = (int8_t) Servos[roomNr][i];

   
    if (s > 0 )
    {
      if (shouldClose)
        AllServos.Close(s,ROOM_HOT);
      else
        AllServos.Open(s, ROOM_HOT);
    }
  }
}

void rooms::setDesiredServoState()
{
  // set desired servo state based on room temp

  DebugTln("Setting desired servo states");
  
  for ( int8_t roomNr = 0 ; roomNr < noRooms ; roomNr++)
  {
    setDesiredServoState(roomNr); 
  }

}

float rooms::setTargetTemp(int8_t roomNr, float roomTarget)
{
  roomTarget = (floor((roomTarget*2)+0.5)/2.0);   // round to 0.5 degree

  DebugTf("setting room# %d to %03.1f\n", roomNr, roomTarget);
  
  targetTemp[roomNr] = roomTarget;
  Save(); // make change boot-persistent

  setDesiredServoState(roomNr);

  return roomTarget;
  
}

float rooms::setTargetTemp(const char * roomName, float roomTarget)
{
  DebugTf("setting %s to %03.1f\n", roomName, roomTarget);
  
  for( int8_t  v=0 ; v < noRooms ; v++)
  {
    DebugTf("%s == %s ?\n", roomName, Name[v]);
    if(!strcmp(roomName, Name[v]))
    {
      DebugTf("Room found: setting temp\n");
      roomTarget = setTargetTemp(v, roomTarget);
      DebugTf("now: %f\n", roomTarget);
      return roomTarget;
    }
  }
  Debugln("ERROR: Room not found!");
  return -99.0;
}

rooms Rooms;
