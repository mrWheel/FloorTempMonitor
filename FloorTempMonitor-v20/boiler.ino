
#include "FloorTempMonitor.h"

__TIMER(Boiler, 60);

#define OT "http://192.168.2.107/api/v0/override/"

#define B_OT_TARGET_WATER 1
#define B_OT_TARGET_ROOM 16
#define B_OT_ACTUAL_ROOM 24

#include "rooms.h"

byte boiler_IP[] = { 192, 168, 2, 107};

void updateBoiler()
{
  if (!DUE(Boiler))
    return;

  // look for room with highest demand

  float  largest_delta = -20.0;
  int8_t largest_i = -1;

  for (int8_t i = 0 ; i < Rooms.noRooms ; i++)
  {
    if (Rooms.targetTemp[i] - COMP_WITH_TARGET[i] > largest_delta)
    {
      largest_delta = Rooms.targetTemp[i] - COMP_WITH_TARGET[i];
      largest_i = i;
    }
  }

  // any room in need of heating?

  if ( largest_delta > 0.0)
  {
    DebugTf("Room with largest delta t is %s (%.1f)\n", Rooms.Name[largest_i], largest_delta);
    DebugTf("At least one room needs to be heated\n");

  } else {
    DebugTf("All rooms at or above target temperature\n");
  }

  // send highest demand to OpenTherm controller

  informBoiler(COMP_WITH_TARGET[largest_i], Rooms.targetTemp[largest_i] );

  informBoiler(largest_delta > 0.0);
}

void sendBoilerAPI(char * APILine)
{
  WiFiClient apiWiFiclient;

  DebugTf("%s", APILine);

  apiWiFiclient.connect(boiler_IP, 80);
  apiWiFiclient.print(APILine);
  delay(500);
  apiWiFiclient.stop();

}
void informBoiler(float actual, float target)
{
  char toPut[128];

  sprintf(toPut, "PUT /api/v0/override/%d/%.1f HTTP/1.1\r\n\r\n", B_OT_TARGET_ROOM, target);
  sendBoilerAPI(toPut);

  sprintf(toPut, "PUT /api/v0/override/%d/%.1f HTTP/1.1\r\n\r\n", B_OT_ACTUAL_ROOM, actual);
  sendBoilerAPI(toPut);
}

void informBoiler(bool on)
{
  char toPut[128];
  float waterShouldBe;

  sprintf(toPut, "PUT /api/v0/boiler/%s HTTP/1.1\r\n\r\n", on ? "on" : "off");
  sendBoilerAPI(toPut);

  if (on) {
    waterShouldBe = 40.0;
  } else
  {
    waterShouldBe = 0.0;
  }

  sprintf(toPut, "PUT /api/v0/override/%d/%.1f HTTP/1.1\r\n\r\n", B_OT_TARGET_WATER, waterShouldBe);
  sendBoilerAPI(toPut);

}
