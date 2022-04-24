#include <ArduinoJson.h>    // v6.19.3 - (see https://arduinojson.org/?utm_source=meta&utm_medium=library.properties)
#include <HTTPClient.h>     // v2.0.0 (part of Arduino ESP32 Core @2.0.2)

#include "FloorTempMonitor.h"
#include "timers.h"
#include "LedPin.h"

DECLARE_TIMERs(roomUpdate,10);

#define tempMargin 0.1

void roomsInit()
{
   roomsLoad();
   getRoomTemps();
}

void roomsLoop()
{
    if (DUE(roomUpdate))
    {
        getRoomTemps();
        setDesiredServoState();
    }
}

void roomsLoad()
{
    File file = LittleFS.open("/rooms.ini", "r");
    int i = 0;

    char buffer[64];
    while (file.available()) {
        int l = file.readBytesUntil('\n', buffer, sizeof(buffer));
        buffer[l] = 0;
        int al,aa,am,tl,ta,tm,sl,sa,sm;

        sscanf(buffer,"%[^;];%d,%d;%f;%d/%d/%d;%d/%d/%d;%d/%d/%d", 
            Rooms[i].Name, 
            &Rooms[i].Servos[0],
            &Rooms[i].Servos[1],
            &Rooms[i].targetTemp,
            &aa, &al, &am,
            &ta, &tl, &tm,
            &sa, &sl, &sm);
        
        Rooms[i].GA_actual = knx.GA_to_address (aa, al, am);
        Rooms[i].GA_target = knx.GA_to_address (ta, tl, tm);
        Rooms[i].GA_state  = knx.GA_to_address (sa, sl, sm);

        yield();
        roomPrint(i);
        i++;
    }
    noRooms = i;
    file.close();
}

void roomsSave()
{
    File file = LittleFS.open("/rooms.ini", "w");
    char buffer[64];

    for (int8_t i=0 ; i < noRooms ; i++)
    {    

        sprintf(buffer,"%s;%d,%d;%f;%d/%d/%d;%d/%d/%d;%d/%d/%d\n", 
            Rooms[i].Name, 
            Rooms[i].Servos[0],
            Rooms[i].Servos[1],
            Rooms[i].targetTemp,
            Rooms[i].GA_actual.ga.area,
            Rooms[i].GA_actual.ga.line,
            Rooms[i].GA_actual.ga.member,
            Rooms[i].GA_target.ga.area,
            Rooms[i].GA_target.ga.line,
            Rooms[i].GA_target.ga.member,
            Rooms[i].GA_state.ga.area,
            Rooms[i].GA_state.ga.line,
            Rooms[i].GA_state.ga.member
            );
        
        yield();
        //-aaw32- file.write(buffer, strlen(buffer));
        file.print(buffer);
        roomPrint(i);
    }
    file.close(); //
}

void roomPrint(byte i)
{
    DebugTf(" %-10.10s [%2d %2d] Target/ActualTemp %4.1f/%4.1f knx %d/%d/%d | %d/%d/%d | %d/%d/%d\n",  
            Rooms[i].Name, 
            Rooms[i].Servos[0],
            Rooms[i].Servos[1],
            Rooms[i].targetTemp,
            Rooms[i].actualTemp,
            Rooms[i].GA_actual.ga.area,
            Rooms[i].GA_actual.ga.line,
            Rooms[i].GA_actual.ga.member,
            Rooms[i].GA_target.ga.area,
            Rooms[i].GA_target.ga.line,
            Rooms[i].GA_target.ga.member,
            Rooms[i].GA_state.ga.area,
            Rooms[i].GA_state.ga.line,
            Rooms[i].GA_state.ga.member);
}

void getRoomTemps()
{
    int apiRC;
    static WiFiClient apiWiFiclient;

    // Make IP address of homewizard
    
    byte IP[] = { 192, 168, 2, 24};

    digitalWrite(LED_WHITE, LED_ON);  // WHITE LED IS ON WHILE PROCESSING ROOM TEMP SENSORS

    timeThis(apiRC=apiWiFiclient.connect(IP, 80));
    delay(500);

    timeThis(apiWiFiclient.print("GET /geheim1967/telist HTTP/1.0\r\n\r\n"));
    yield();

    for( int8_t x=0 ; !apiWiFiclient.available() && x < 10 ; x++)
        delay(100);
        
    
    if ( apiWiFiclient.available()  && apiWiFiclient.find("response"))    // OK
    {
      char Response[1024];
      int seen=0;

      while(seen < noRooms && apiWiFiclient.available() && apiWiFiclient.find("{") )
      {
        const char *ptr;    
        
        yield();
        timeThis(apiWiFiclient.readBytesUntil('}',  Response, sizeof(Response)));
        
        ptr=Response;

        if (!strlen(ptr))
        {
            DebugTf("[HomeWizard] No response (left))\n");
            break;
        }

        // DebugTf("ptr = >%s<\n", ptr);
        while( (ptr=strstr(ptr,"\"name\"")))
        {
            char jsonName[20];
            float jsonTemp;
            bool  nameFound=false;

            yield();
            
            sscanf(ptr,"\"name\":\"%[^\"]", jsonName);
            ptr = (char*) &ptr[7];

            //DebugTf("name found: >%s<\n", jsonName);
            
            if(!(ptr = strstr(ptr,"\"te\"")))
            {
                DebugTf("[HomeWizard] Incomplete JSON response, no 'te' found for %s\n", jsonName);
                break;
            }
            sscanf(ptr,"\"te\":%f", &jsonTemp);
            ptr = (char*) &ptr[5];

            // assign API result to correct Room 

            for( int8_t roomIndex=0 ; roomIndex < noRooms ; roomIndex++)
            {
                yield();

                if(!strcmp(Rooms[roomIndex].Name,jsonName))
                {
                    Rooms[roomIndex].actualTemp = jsonTemp;
                    //DebugTf("Match for room %s (%f) in room %s\n", jsonName, jsonTemp, Rooms[roomIndex].Name);

                    nameFound=true;
                    roomPrint(roomIndex);
                    seen++;
                    break; // found, lets not continue
                }
            }
            
            if(!nameFound)
                DebugTf("No match for room %s (%f)\n", jsonName, jsonTemp);   
        } 
      } 
      digitalWrite(LED_WHITE, LED_OFF);

    } else {

        // WHEN NO HOMEWIZARD CONTACT WHITE LED STAYS ON
        
        DebugTf("[HomeWizard] API call failed with rc %d\n", apiRC);
        DebugTf("Assuming all rooms are ~ 20.0 degrees\n");

        for( int8_t roomIndex=0 ; roomIndex < noRooms ; roomIndex++)
        {
            Rooms[roomIndex].actualTemp = random(190,210)/10.0;
        }
    }    

    timeThis(apiWiFiclient.stop());  
}

void setDesiredServoState()
{
    // set desired servo state based on room temp

    for( int8_t roomIndex=0 ; roomIndex < noRooms ; roomIndex++)
    {
        int8_t s;
        
        for(byte i=0 ; (s=Rooms[roomIndex].Servos[i]) > 0 && i < 2; i++)
        {
            // close servos based on room temperature?
            
            if (Rooms[roomIndex].actualTemp > Rooms[roomIndex].targetTemp-tempMargin )
            {
                AllServos.servoClose(s,ROOM_HOT);
            } else {
            
                AllServos.servoOpen(s,ROOM_HOT);
            }
        }
    }

}
