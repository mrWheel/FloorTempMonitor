#include <ArduinoJson.h>
#if defined(ESP8266)
  #include <ESP8266HTTPClient.h>
#else
  #include <HTTPClient.h>
#endif

#include "FloorTempMonitor32.h"

HTTPClient apiClient;
WiFiClient apiWiFiclient;

String apiEndpoint = "http://192.168.2.24/geheim1967/telist";

DECLARE_TIMERs(roomUpdate,60);

void roomsInit(){

    const char *rInit[MAXROOMS];

    // hardcoded for now, should come from rooms.ini file

    rInit[0] = "Living;1,7;22.5";
    rInit[1] = "Hallway;2,-1;22.5";
    rInit[2] = "Kitchen;3,5;21.5";
    rInit[3] = "Basement;4,-1;17.5";
    rInit[4] = "Dining;6,-1;22.5";
    rInit[5] = "Office;8,-1;20.5";
    rInit[6] = "Utility;9,-1;18.5";

    noRooms = 7;

    for(byte i=0 ; i < noRooms ; i++)
    {
        sscanf(rInit[i],"%[^;];%d,%d;%f", 
            Rooms[i].Name, 
            &Rooms[i].Servos[0],
            &Rooms[i].Servos[1],
            &Rooms[i].targetTemp);
        
        timeCritical();
        dumpRoom(i);
    }
}

void dumpRoom(byte i)
{
    DebugTf("RoomDump: Name: %s Servos [%d,%d] Target/ActualTemp %f/%f \n",  
            Rooms[i].Name, 
            Rooms[i].Servos[0],
            Rooms[i].Servos[1],
            Rooms[i].targetTemp,
            Rooms[i].actualTemp);
}

#define QuotedString(x) "\"##x##\""

void handleRoomTemps()
{
    int apiRC;

    if (!DUE(roomUpdate))
        return;

    // Make API call

#ifdef TESTDATA
    if ( 2 > 1)    // TESTDATA!!!
#else
    timeThis(apiClient.begin(apiWiFiclient, apiEndpoint));

    timeThis(apiRC=apiClient.GET());

    if ( apiRC > 0)    // OK
#endif
    {
        const char *ptr;    
        String Response;
        // get response

#ifdef TESTDATA
        Response = "{\"status\": \"ok\", \"version\": \"3.403\", \"request\": {\"route\": \"/telist\" }, "
                       " \"response\": ["
                       " {\"id\":0,\"name\":\"Kitchen\",\"code\":\"11391347\",\"model\":1,\"lowBattery\":\"no\",\"version\":2.32,\"te\":20.8,\"hu\":39,\"te+\":21.0,\"te+t\":\"00:01\",\"te-\":19.9,\"te-t\":\"06:30\",\"hu+\":41,\"hu+t\":\"00:01\",\"hu-\":39,\"hu-t\":\"03:30\",\"outside\":\"yes\",\"favorite\":\"no\"}"
                       ",{\"id\":1,\"name\":\"Basement\",\"code\":\"2815108\",\"model\":1,\"lowBattery\":\"no\",\"version\":2.32,\"te\":20.1,\"hu\":47,\"te+\":20.2,\"te+t\":\"01:45\",\"te-\":19.9,\"te-t\":\"05:25\",\"hu+\":49,\"hu+t\":\"00:14\",\"hu-\":47,\"hu-t\":\"03:47\",\"outside\":\"no\",\"favorite\":\"no\"}"
                       ",{\"id\":2,\"name\":\"Living\",\"code\":\"14974854\",\"model\":1,\"lowBattery\":\"no\",\"version\":2.32,\"te\":21.4,\"hu\":40,\"te+\":21.4,\"te+t\":\"15:00\",\"te-\":20.2,\"te-t\":\"06:55\",\"hu+\":42,\"hu+t\":\"00:00\",\"hu-\":40,\"hu-t\":\"07:53\",\"outside\":\"no\",\"favorite\":\"no\"}"
                       ",{\"id\":3,\"name\":\"Office\",\"code\":\"10570279\",\"model\":1,\"lowBattery\":\"no\",\"version\":2.32,\"te\":21.5,\"hu\":41,\"te+\":21.8,\"te+t\":\"00:01\",\"te-\":20.2,\"te-t\":\"06:06\",\"hu+\":43,\"hu+t\":\"00:01\",\"hu-\":41,\"hu-t\":\"13:59\",\"outside\":\"no\",\"favorite\":\"no\"}"
                       ",{\"id\":4,\"name\":\"Dining\",\"channel\":2,\"model\":0,\"te\":17.2,\"hu\":48,\"te+\":19.8,\"te+t\":\"07:52\",\"te-\":15.8,\"te-t\":\"07:00\",\"hu+\":52,\"hu+t\":\"07:55\",\"hu-\":47,\"hu-t\":\"02:33\",\"outside\":\"no\",\"favorite\":\"no\"}"
                       ",{\"id\":5,\"name\":\"Hallway\",\"channel\":5,\"model\":0,\"te\":17.8,\"hu\":48,\"te+\":19.6,\"te+t\":\"07:52\",\"te-\":15.8,\"te-t\":\"07:00\",\"hu+\":52,\"hu+t\":\"07:55\",\"hu-\":47,\"hu-t\":\"02:33\",\"outside\":\"no\",\"favorite\":\"no\"}"
                       ",{\"id\":6,\"name\":\"Utility\",\"channel\":3,\"model\":0,\"te\":19.8,\"hu\":48,\"te+\":21.6,\"te+t\":\"07:52\",\"te-\":18.8,\"te-t\":\"07:00\",\"hu+\":52,\"hu+t\":\"07:55\",\"hu-\":47,\"hu-t\":\"02:33\",\"outside\":\"no\",\"favorite\":\"no\"}"
                                      "]}";
#else
        timeThis(Response = apiClient.getString()); 
#endif
        // process all records in resonse (name, te) pairs

        Response.trim();
        ptr=&Response.c_str()[0];

        if (!strlen(ptr) || ptr[strlen(ptr)-1] != '}')
        {
            DebugTf("[HomeWizard] No response or last char not '}'\n");
            // just a warning, do proceed to find usable values in result
        }

        // DebugTf("ptr = >%s<\n", ptr);
        while( (ptr=strstr(ptr,"\"name\"")))
        {
            char jsonName[20];
            float jsonTemp;
            bool  nameFound=false;

            //DebugTf("ptr = >%s<\n", ptr);

            timeCritical();
            
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
                timeCritical();

                if(!strcmp(Rooms[roomIndex].Name,jsonName))
                {
                    Rooms[roomIndex].actualTemp = jsonTemp;
                    //DebugTf("Match for room %s (%f) in room %s\n", jsonName, jsonTemp, Rooms[roomIndex].Name);

                    nameFound=true;
                    dumpRoom(roomIndex);
                    byte s;
                    for(byte i=0 ; (s=Rooms[roomIndex].Servos[i]) > 0 ; i++)
                    {
                        if (Rooms[roomIndex].actualTemp > Rooms[roomIndex].targetTemp+0.2 && servoArray[s].servoState == 0)
                        {
                            servoClose(s);
                        }
                    }

                    break;
                }
            }
            
            if(!nameFound)
                DebugTf("No match for room %s (%f)\n", jsonName, jsonTemp);   
        }  
    } else 
        DebugTf("[HomeWizard] API call failed with rc %d\n", apiRC);
    
    apiClient.end();  
}
