/*
 * 
 * 
 */

#include "FloorTempMonitor.h"

#include "restapi.h"

#include "servos.h"
#include "sensors.h"
#include "rooms.h"

#include "timers.h"
#include "profiling.h"

DECLARE_TIMER(cacheRefresh, 10)

#define CALIBRATE_HIGH 25.0

// global vars

boolean cacheEmpty = true;

// local vars

static DynamicJsonDocument _cache(4096);   // the cache for sensors
static DynamicJsonDocument toRetDoc(1024);  // generic doc to return, clear() before use!
static JsonArray calibrations;

static WebServer *srv;

// some helper functions

void _returnJSON(JsonObject obj)
{
  String toReturn;

  serializeJson(obj, toReturn);

  srv->send(200, "application/json", toReturn);
}

void _returnJSON400(const char * message)
{

  srv->send(400, "application/json", message);
}

void _cacheJSON()
{
  if (!DUE(cacheRefresh) && !cacheEmpty)
  {
    DebugTf("No need to refresh JSON cache\n");
    return;
  }
  cacheEmpty = false;

  DebugTf("Time to populate the JSON cache\n");

  _cache.clear();

  JsonArray sensors = _cache.createNestedArray("sensors");

  for (int8_t s = 0; s < AllSensors.noSensors; s++)
  {
    DynamicJsonDocument so(512);

    so["sensorIndex"] = AllSensors.cbIndex[s];
    so["sensorID"]    = AllSensors.sensorID[s];
    so["name"]        = AllSensors.name[s];
    so["offset"]      = AllSensors.tempOffset[s];
    so["factor"]      = AllSensors.tempFactor[s];
    so["temperature"] = AllSensors.tempC[s];
    so["servonr"]     = AllSensors.servoNr[s];
    so["deltatemp"]   = AllSensors.deltaTemp[s];
    so["counter"]     = s;

    sensors.add(so);
  }
}

// api/describe_sensor?[name|sensorID]=<string>
int _API_find_sensorIndex_in_query() 
{

  // describe_sensor takes name= or sensorID= as query parameters
  // e.g. api/describe_sensor?sensorID=0x232323232323
  // returns index in _SA

  const char * Keys[] = {"name", "sensorID" };

  // loop over all possible query keys

  for ( const char * Key : Keys )
  {

    const char* Value = srv->arg(Key).c_str();

    // when query is ?name=kitchen
    // key = name and Value = kitchen
    // if query doesn't contain name=, Value is ""

    if ( strlen(Value) > 0 )
    {
      // the Key was in the querystring with a non-zero Value

      // loop over the array entries to find object that contains Key/Value pair

      int8_t i = AllSensors.getIndexFor(Value);

      // no object in array contains Key/Value pair, hence return 400
      if (i < 0 )
        _returnJSON400("valid key, but no record with requested value");
      return i;
    }
  }

  // the query doesn't ask for a field we know (or support)

  _returnJSON400("invalid key. Use name or sensorID");
  return -1;
}

// --- the handlers ---
// api/list_sensors

void _API_GET_sensor_list() 
{
  _cacheJSON();

  JsonObject obj = _cache.as<JsonObject>() ;

  _returnJSON(obj);

}


void _API_calibrate_low(int sensorIndex, float lowCalibratedTemp)
{

  DynamicJsonDocument c(64);

  AllSensors.calibrate_low(sensorIndex, lowCalibratedTemp);

  c["idx"] = sensorIndex;
  c["raw"] = AllSensors.tempRaw[sensorIndex];
  c["off"] = AllSensors.tempOffset[sensorIndex];
  c["fct"] = AllSensors.tempFactor[sensorIndex];

  calibrations.add(c);

}

void _API_calibrate_high(int sensorIndex, float hiCalibratedTemp)
{
  DynamicJsonDocument c(64);

  AllSensors.calibrate_high(sensorIndex, hiCalibratedTemp);

  c["idx"] = sensorIndex;
  c["raw"] = AllSensors.tempRaw[sensorIndex];
  c["off"] = AllSensors.tempOffset[sensorIndex];
  c["fct"] = AllSensors.tempFactor[sensorIndex];

  calibrations.add(c);
}

void _API_calibrate_sensor(int sensorIndex, float realTemp)
{

  if ( realTemp < CALIBRATE_HIGH )
  {
    timeThis(_API_calibrate_low(sensorIndex, realTemp));
  } else {
    timeThis(_API_calibrate_high(sensorIndex, realTemp));
  }
}

void _API_GET_sensor_calibrate()
{
  //DynamicJsonDocument toRetCalDoc(512);

  DebugTf("starting handleAPI_calibrate_sensor()");
  AllSensors.ForceUpdate();

  toRetDoc.clear();

  // a temperature should be given and optionally a sensor(name or ID)
  // when sensor is missing, all sensors are calibrated
  // /api/calibrate_sensor?temp=<float>[&[name|sensorID]=<string>]
  // *** actualTemp should come from a calibrated source

  const char* actualTempString = srv->arg("temp").c_str();

  // check correct use

  if (strlen(actualTempString) <= 0)
  {
    _returnJSON400("temp is mandatory query parameter in calibration function");
    return;
  }
  cacheEmpty = true;  // invalidate cache forcefully

  float actualTemp = atof(actualTempString);

  // prepare some JSON stuff to return

  calibrations = toRetDoc.createNestedArray("calibrations");

  // was a name or sensorID specified --> calibrate only that one sensor

  if (strlen(srv->arg("name").c_str()) +
      strlen(srv->arg("sensorID").c_str()) > 0)
  {
    int sensorToCalibrate = _API_find_sensorIndex_in_query();

    if (sensorToCalibrate >= 0) {
      DebugTf("About to calibrate %s sensor with %f\n", AllSensors.name[sensorToCalibrate], actualTemp);
      _API_calibrate_sensor(sensorToCalibrate, actualTemp);
    }
  } else {

    // otherwise, calibrate all sensors
    DebugTf("About to calibrate all sensors with %f\n", actualTemp);
    for (int sensorToCalibrate = 0 ; sensorToCalibrate < AllSensors.noSensors ; sensorToCalibrate++)
      _API_calibrate_sensor(sensorToCalibrate, actualTemp);

  }

  _returnJSON(toRetDoc.as<JsonObject>());
}

// api/describe_sensor?[name|sensorID]=<string>
void _API_GET_sensor_describe() 
{
  int si = _API_find_sensorIndex_in_query();

  Debugf("sensorIndex=%d\n", si);

  if (si < 0 )
    return;

  JsonObject toRet = _cache["sensors"][si];
  _returnJSON(toRet);
}

// api/get_temperature?[name|sensorID]=<string>
void _API_GET_sensor_temperature() 
{
  int si = _API_find_sensorIndex_in_query();
  //DynamicJsonDocument toRetDoc(64);

  toRetDoc.clear();

  Debugf("sensorIndex=%d\n", si);

  if (si < 0 )
    return;

  toRetDoc["temperature"] = _cache["sensors"][si]["temperature"];
  // JsonObject toRet = toRetDoc.as<JsonObject>();
  //_returnJSON(toRet);

  _returnJSON( toRetDoc.as<JsonObject>() );
}

void _API_GET_status()
{
  // DynamicJsonDocument toRetDoc(256);

  char cMsg[120];
  
  toRetDoc.clear();

  toRetDoc["uptime"] = upTime();
  toRetDoc["rssi"] = WiFi.RSSI();
  toRetDoc["disconnects"] = AllServos.ConnectionMuxLostCount();
  
  getLastResetReason(rtc_get_reset_reason(0), cMsg, sizeof(cMsg));
  toRetDoc["ESPrr0"] =  String(cMsg);
  
  getLastResetReason(rtc_get_reset_reason(1), cMsg, sizeof(cMsg));
  toRetDoc["ESPrr1"] = String(cMsg);
  
  toRetDoc["ESPheap"] = ESP.getFreeHeap();
  //-aaw32-toRetDoc["ESPhfrag"] = ESP.getHeapFragmentation();
  //-aaw32-toRetDoc["ESPhmax"] = ESP.getMaxFreeBlockSize();

  toRetDoc["FWversion"] =  _FW_VERSION;
  toRetDoc["CompiledData"] = __DATE__;
  toRetDoc["CompiledTime"] = __TIME__;

  _returnJSON( toRetDoc.as<JsonObject>() );

}
// bogus handler for testing

void _API_usage()
{
  srv->send(200, "text/html",  "options are: /status, /sensor/{list,describe,temperature,calibrate}, /room/{list,temperature}");

}

void _API_GET_room_temperature()
{
  _returnJSON400("Not implemented yet");
}

void _API_GET_room_list()
{
  toRetDoc.clear();

  JsonArray rooms = toRetDoc.createNestedArray("rooms");

  //int roomId = 0;

  for (int8_t i = 0 ; i < Rooms.noRooms ; i++)
  {
    DynamicJsonDocument r(1024);
    byte s;

    r.clear();

    r["name"]   = Rooms.Name[i];
    r["target"] = Rooms.targetTemp[i];
    r["actual"] = Rooms.actualTemp[i];

    JsonArray servos = r.createNestedArray("servos");

    for ( s = 0 ; s < 2 ; s++)
    {
      if (Rooms.Servos[i][s] < 0)
        break;
      servos.add(Rooms.Servos[i][s]);
    }
    r["servocount"] = s;
    r["id"] = i;
    rooms.add(r);

  }

  _returnJSON(toRetDoc.as<JsonObject>());

}

void _API_GET_servo_status()
{
  char statusText[16];
  const char* Value = srv->arg("id").c_str();

  if (!strlen(Value))
    _returnJSON400("mandatory id missing");

  byte i = atoi(Value);

  toRetDoc.clear();

  toRetDoc["servo"] = i;
  toRetDoc["status"]  = AllServos.State(i);
  toRetDoc["reason"]  = AllServos.Reason(i);

  char reasonTxt[3] = {'r', 'w', '\0'};

  if (AllServos.Reason(i) & ROOM_HOT)
    reasonTxt[0] = 'R';
  else
    reasonTxt[0] = '-';

  if (AllServos.Reason(i) & WATER_HOT)
    reasonTxt[1] = 'W';
  else
    reasonTxt[1] = '-';

  switch (AllServos.State(i)) {
    case SERVO_IS_OPEN:
      strcpy(statusText, "OPEN");
      break;
    case SERVO_IS_OPENING:
      strcpy(statusText, "OPENING");
      break;
    case SERVO_IS_CLOSING:
      sprintf(statusText, "CLOSING [%s]", reasonTxt);
      break;
    case SERVO_IS_CLOSED:
      sprintf(statusText, "CLOSED [%s]", reasonTxt);
      break;
  }

  toRetDoc["message"] = statusText;

  _returnJSON(toRetDoc.as<JsonObject>());

}

void _API_GET_servo_statusarray()
{
  toRetDoc.clear();

  JsonArray servos = toRetDoc.createNestedArray("servos");

  for (byte i = 0 ; i < 10; i++)
  {
    servos.add(AllServos.State(i));
  }
  _returnJSON(toRetDoc.as<JsonObject>());
}

void _API_GET_servo_reasonarray()
{
  toRetDoc.clear();

  JsonArray servos = toRetDoc.createNestedArray("reason");

  for (byte i = 0 ; i < 10; i++)
  {
    servos.add(AllServos.Reason(i));
  }
  _returnJSON(toRetDoc.as<JsonObject>());
}

void _API_PUT_room()
{
 
  char room[20] = {};
  char temp[20] = {};

  for (int i = 0; i < srv->args(); i++)
  {
    DebugTf("[%d] -> [%s] -> [%s]\r\n", i, srv->argName(i), srv->arg(i));
    if (strcasecmp("room", srv->argName(i).c_str()) == 0)
      strlcpy(room, srv->arg(i).c_str(), sizeof(room));
    if (strcasecmp("temp", srv->argName(i).c_str()) == 0)
      strlcpy(temp, srv->arg(i).c_str(), sizeof(temp));
  }

  DebugTf("PUT %s to %s\n", room, temp);

  toRetDoc.clear();
  toRetDoc["room"] = room;

  toRetDoc["targetTemp"] = Rooms.setTargetTemp(room, atof( temp));

  _returnJSON(toRetDoc.as<JsonObject>());
}

void API_Init(WebServer *_srv)
{

  srv = _srv;
  
  srv->on("/api",                   _API_usage);

  srv->on("/api/status",            _API_GET_status);

  srv->on("/api/sensor/list",       _API_GET_sensor_list);
  srv->on("/api/sensor/describe",   _API_GET_sensor_describe);
  srv->on("/api/sensor/temperature",_API_GET_sensor_temperature);
  srv->on("/api/sensor/calibrate",  _API_GET_sensor_calibrate);

  srv->on("/api/room", HTTP_PUT,    _API_PUT_room);

  srv->on("/api/room/list",         _API_GET_room_list);
  srv->on("/api/room/temperature",  _API_GET_room_temperature);

  srv->on("/api/servo/status",      _API_GET_servo_status);
  srv->on("/api/servo/statusarray", _API_GET_servo_statusarray);
  srv->on("/api/servo/reasonarray", _API_GET_servo_reasonarray);

}
