/*
  #define USE_UNICAST             // HJM: Use UniCast in stead of multicast
  #define UNICAST_IP              IPAddress(192, 168, 2, 2) // [Default IPAddress(224, 0, 23, 12)]
  #define UNICAST_PORT            3671
*/

//-- https://github.com/c-o-m-m-a-n-d-e-r/esp-knx-ip
//-- modified version for ESP32
//-- AaW changed line 267
//-- from "cemi_addi_t additional_info[];"
//-- to   "cemi_addi_t *additional_info;"

#include <esp-knx-ip.h>     // v0.4

#define KNXROOMS Rooms.noRooms

DECLARE_TIMERs(knxUpdate, 30);

float   knxActual[MAXROOMS],
        knxTarget[MAXROOMS];
bool    knxState[MAXROOMS];

void myKNX_init(WebServer *srv) {

  callback_id_t tempCallBackId,
                targetCallBackId,
                stateCallBackId;

  

  tempCallBackId =   knx.callback_register("Temperature", temp_cb,   nullptr, ok_fun);
  
  targetCallBackId = knx.callback_register("Target",      target_cb, nullptr, ok_fun);

  stateCallBackId =  knx.callback_register("State",       state_cb,  nullptr, ok_fun);

  DebugTf("teCB %d taCB %d & stBC %d\n", tempCallBackId, targetCallBackId, stateCallBackId );

  for ( int8_t Room = 0 ; Room < KNXROOMS ; Room++)
  {
    address_t GA;

    knxActual[Room] = 15.0f;
    // GA = knx.GA_to_address(10, Room, 1); // actual = 1
    knx.callback_assign (tempCallBackId, Rooms.GA_actual[Room]);

    knxTarget[Room] = 25.0f;
    // GA = knx.GA_to_address(10, Room, 2); // target = 2
    knx.callback_assign (targetCallBackId, Rooms.GA_target[Room]);

    knxState[Room] = false;
    // GA = knx.GA_to_address(10, Room, 3); // state = 3
    knx.callback_assign (stateCallBackId, Rooms.GA_state[Room]);

  }

  knx.physical_address_set(knx.PA_to_address(1, 1, 222));
  
  // Start knx
  
  // knx.start(nullptr);
  knx.start(srv);

  myKNX_update();
}

bool ok_fun()
{
  return true;
}

void myKNX_loop()
{
  knx.loop();

  if ( DUE(knxUpdate) )
    myKNX_update();
}

void myKNX_update()
{
  address_t GA;

  // knx.write_1bit(knx.GA_to_address(1,0,60), 1);

  for ( int8_t Room = 0 ; Room < KNXROOMS ; Room++)
  {
    if (knxActual[Room] != Rooms.actualTemp[Room])
    {
      // update actual temp on KNX bus
      
      knxActual[Room] = Rooms.actualTemp[Room];
      DebugTf("writing to %d/%d/%d actual temp %4.1f\n",
              Rooms.GA_actual[Room].ga.area,
              Rooms.GA_actual[Room].ga.line,
              Rooms.GA_actual[Room].ga.member,
              knxActual[Room]);
      knx.write_2byte_float(Rooms.GA_actual[Room], knxActual[Room]);
      delay(50);
    }
    if (knxTarget[Room] != Rooms.targetTemp[Room])
    {
      // update target on KNX bus
      knxTarget[Room] = Rooms.targetTemp[Room];
      DebugTf("writing to %d/%d/%d target temp %4.1f\n",
              Rooms.GA_target[Room].ga.area,
              Rooms.GA_target[Room].ga.line,
              Rooms.GA_target[Room].ga.member,
              knxTarget[Room]);
      knx.write_2byte_float(Rooms.GA_target[Room], knxTarget[Room]);
      delay(50);
    }

    bool isHeating = false;

    for (int8_t k = 0 ; k < 2 ; k++)
      if ( Rooms.Servos[Room][k] >= 0)
      {
        DebugTf("Servo [%02d] state Room[%d] is %d\n", Rooms.Servos[Room][k],
                Room,
                AllServos.State(Rooms.Servos[Room][k]) );
        if ( AllServos.State(Rooms.Servos[Room][k]) == SERVO_IS_OPEN || AllServos.State(Rooms.Servos[Room][k]) == SERVO_IS_OPENING )
          isHeating = true;
      }

    if ( knxState[Room] != isHeating )
    {
      knxState[Room] = isHeating;

      DebugTf("writing to %d/%d/%d state heating is %s\n",
              Rooms.GA_state[Room].ga.area,
              Rooms.GA_state[Room].ga.line,
              Rooms.GA_state[Room].ga.member,
              isHeating ? "on" : "off");

      knx.write_1bit(Rooms.GA_state[Room], isHeating);

      // DebugTf("writing to 10/%d/3 heating is %s\n", Room, isHeating ? "on" : "off");
      // knx.write_1bit(knx.GA_to_address(10, Room, 3), isHeating);
      delay(50);
    }
  }

}

void temp_cb(message_t const &msg, void *arg)
{
  address_t _from = msg.received_on;

  int roomNr = _from.ga.line;

  DebugTf("temp CB callback triggered for room %d\n", roomNr);

  switch (msg.ct)
  {
    case KNX_CT_READ:
      {
        DebugTf("About to reply %f\n", Rooms.actualTemp[roomNr]);
        knx.answer_2byte_float(msg.received_on, Rooms.actualTemp[roomNr]);
        break;
      }
  }
}

void target_cb(message_t const &msg, void *arg)
{
  address_t _from = msg.received_on;

  int roomNr = _from.ga.line;
  DebugTf("target CB callback triggered with %d for room %d\n", msg.ct, roomNr);

  switch (msg.ct)
  {
    case KNX_CT_READ:
      {
        DebugTf("About to reply target for the room is %f\n", Rooms.targetTemp[roomNr]);
        knx.answer_2byte_float(msg.received_on, Rooms.targetTemp[roomNr]);

        break;
      }
    case KNX_CT_WRITE:
      {
        knxTarget[roomNr] = knx.data_to_2byte_float(msg.data);
        knxTarget[roomNr] = Rooms.setTargetTemp(roomNr, knxTarget[roomNr]);
        DebugTf("Set target for the room to %f\n", Rooms.targetTemp[roomNr]);

        break;
      }
  }
}

void state_cb(message_t const &msg, void *arg)
{
  address_t _from = msg.received_on;

  int roomNr = _from.ga.line;

  DebugTf("state CB callback triggered for room %d\n", roomNr);

  switch (msg.ct)
  {
    case KNX_CT_READ:
      {
        DebugTf("About to reply %s\n", knxState[roomNr] ? "on" : "off");
        knx.answer_1bit(msg.received_on, knxState[roomNr]);
        break;
      }
  }
}
