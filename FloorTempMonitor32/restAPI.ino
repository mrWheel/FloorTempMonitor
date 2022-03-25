#include <ArduinoJson.h>
#include "FloorTempMonitor32.h"

DECLARE_TIMER(cacheRefresh, 60)

#define CALIBRATE_HIGH 25

// global vars

static DynamicJsonDocument _cache(4096);   // the cache for sensors
static DynamicJsonDocument toRetDoc(1024);  // generic doc to return, clear() before use!

// some helper functions

void _returnJSON(AsyncWebServerRequest *request, JsonObject obj); // prototype????
void _returnJSON(AsyncWebServerRequest *request, JsonObject obj)
{
  String toReturn;

  serializeJson(obj, toReturn);

  //DebugTln(toReturn);
  
  //httpServer.send(200, "application/json", toReturn);
  request->send(200, "application/json", toReturn);
}

void _returnJSON400(AsyncWebServerRequest *request, const char * message)
{
  
  //httpServer.send(400, "application/json", message);
  request->send(400, "application/json", message);
}

void _cacheJSON()
{
  if(!DUE(cacheRefresh)) 
  {
    DebugTf("No need to refresh JSON cache\n");
    return;
  }

  DebugTf("Time to populate the JSON cache\n");

  _cache.clear();
    
  JsonArray sensors = _cache.createNestedArray("sensors");
  
  for(int8_t s=0; s<noSensors; s++)
  {
    DynamicJsonDocument so(512);
        
    so["index"]       = sensorArray[s].index;
    so["position"]    = sensorArray[s].position;
    so["sensorID"]    = sensorArray[s].sensorID;
    so["name"]        = sensorArray[s].name;
    so["offset"]      = sensorArray[s].tempOffset;
    so["factor"]      = sensorArray[s].tempFactor;
    so["temperature"] = sensorArray[s].tempC;
    so["servonr"]     = sensorArray[s].servoNr;
    // so["servostate"]  = sensorArray[s].servoNr < 0 ? 0 : servoArray[sensorArray[s].servoNr].servoState;
    so["deltatemp"]   = sensorArray[s].deltaTemp;
    so["counter"]     = s;
    
    sensors.add(so);
  }
} // _cacheJSON()


int _find_sensorIndex_in_query(AsyncWebServerRequest *request) // api/describe_sensor?[name|sensorID]=<string>
{  
   _cacheJSON(); // first update cache - if necessary

  JsonArray arr = _cache["sensors"];

  // describe_sensor takes name= or sensorID= as query parameters
  // e.g. api/describe_sensor?sensorID=0x232323232323
  // returns the JSON struct of the requested sensor
  
  const char * Keys[]= {"sensorID", "name"};

  // loop over all possible query keys
  
  for ( const char * Key : Keys )
  {
    DebugTf("Key[%s]\n\r", Key);
    //const char* Value=httpServer.arg(Key).c_str();
    //if(request->hasArg(Key))
      const char* Value = request->arg(Key).c_str();
    //else
    //  const char* Value = {'\0'};
      
    // when query is ?name=kitchen
    // key = name and Value = kitchen
    // if query doesn't contain name=, Value is ""
    
    if ( strlen(Value) > 0 )
    {
      // the Key was in the querystring with a non-zero Value
      
      // loop over the array entries to find object that contains Key/Value pair
      
      for(JsonObject obj : arr)
      {
        Debugf("Comparing %s with %s\n", obj[Key].as<char*>(), Value);
        
        if ( !strcmp(obj[Key],Value))
        {
            // first record with matching K/V pair is returned
            //_returnJSON(obj);
            return(obj["counter"].as<int>());
        }
      }

      // no object in array contains Key/Value pair, hence return 400

      _returnJSON400(request, "valid key, but no record with requested value");
      return -1;
    }
  }

  // the query doesn't ask for a field we know (or support)

  _returnJSON400(request, "invalid key");
  return -1;
}
  
// --- the handlers ---

void handleAPI_list_sensors(AsyncWebServerRequest *request) // api/list_sensors
{  
  _cacheJSON();

  JsonObject obj = _cache.as<JsonObject>() ;
  
  _returnJSON(request, obj);
   
} 

JsonArray calibrations;

void calibrate_low(int sensorIndex, float lowCalibratedTemp)
{

  DynamicJsonDocument c(64);
  
  float tempRaw = sensors.getTempCByIndex(_SA[sensorIndex].index);

  // this should be lowTemp, change tempOffset
  // so that reading + offset equals lowTemp
  // that is, offset = lowTemp - raw reading 

  _SA[sensorIndex].tempOffset = lowCalibratedTemp - tempRaw;
  _SA[sensorIndex].tempFactor = 1.0;
  
  c["index"]  = sensorIndex;
  c["offset"] = _SA[sensorIndex].tempOffset;

  calibrations.add(c);
  
  updateIniRec(_SA[sensorIndex]);

}

void calibrate_high(int sensorIndex, float hiCalibratedTemp)
{
  float tempRaw = sensors.getTempCByIndex(_SA[sensorIndex].index);
  DynamicJsonDocument c(64);

  // calculate correction factor
  // so that 
  // raw_reading * correction_factor = actualTemp
  // --> correction_factor = actualTemp/raw_reading

  // r(read)=40, t(actual)=50 --> f=50/40=1.25
  // r=20  --> t=20.0*1.25=25.0

  // make sure to use the calibrated reading, 
  // so add the offset!
  
  _SA[sensorIndex].tempFactor = hiCalibratedTemp  / ( tempRaw + _SA[sensorIndex].tempOffset );

  c["index"]  = sensorIndex;
  c["factor"] = _SA[sensorIndex].tempFactor;

  calibrations.add(c);
  updateIniRec(_SA[sensorIndex]);
}

void calibrate_sensor(int sensorIndex, float realTemp)
{
  if ( realTemp < CALIBRATE_HIGH )
    calibrate_low(sensorIndex, realTemp);
  else
    calibrate_high(sensorIndex, realTemp);
}



void handleAPI_calibrate_sensor(AsyncWebServerRequest *request)
{
  //DynamicJsonDocument toRetCalDoc(512);

  toRetDoc.clear();

  // a temperature should be given and optionally a sensor(name or ID)
  // when sensor is missing, all sensors are calibrated
  // /api/calibrate_sensor?temp=<float>[&[name|sensorID]=<string>]
  // *** actualTemp should come from a calibrated source
  
  //const char* actualTempString=httpServer.arg("temp").c_str();
  //if(request->hasArg("temp"))
    const char* actualTempString = request->arg("temp").c_str();
  //else
  //  actualTempString = {};

  
  // check correct use
  
  if (strlen(actualTempString) <= 0)
  {
     _returnJSON400(request, "temp is mandatory query parameter in calibration function");
     return;
  }
  float actualTemp = atof(actualTempString);
  
  // prepare some JSON stuff to return
  
  calibrations = toRetDoc.createNestedArray("calibrations");
  
  // was a name or sensorID specified --> calibrate only that one sensor
  
  //if (strlen(httpServer.arg("name").c_str()) +
  //    strlen(httpServer.arg("sensorID").c_str()) > 0)
  if (strlen(request->arg("name").c_str()) +
      strlen(request->arg("sensorID").c_str()) > 0)
  {
    int sensorToCalibrate = _find_sensorIndex_in_query(request);

    calibrate_sensor(sensorToCalibrate, actualTemp);
      
  } else {

    // otherwise, calibrate all sensors
    
    for (int sensorToCalibrate=0 ; sensorToCalibrate < noSensors ; sensorToCalibrate++)
      calibrate_sensor(sensorToCalibrate, actualTemp);

  }
  
  _returnJSON(request, toRetDoc.as<JsonObject>());
}

void handleAPI_describe_sensor(AsyncWebServerRequest *request) // api/describe_sensor?[name|sensorID]=<string>
{  
  int si = _find_sensorIndex_in_query(request);
  
  Debugf("sensorIndex=%d\n", si);
  
  if (si < 0 )
    return;
  
  JsonObject toRet = _cache["sensors"][si];
  _returnJSON(request, toRet);
}


void handleAPI_get_temperature(AsyncWebServerRequest *request) // api/get_temperature?[name|sensorID]=<string>
{
  int si = _find_sensorIndex_in_query(request);
  //DynamicJsonDocument toRetDoc(64);

  toRetDoc.clear();

  Debugf("sensorIndex=%d\n", si);
  
  if (si < 0 )
    return;
  
  toRetDoc["temperature"] = _cache["sensors"][si]["temperature"];
  // JsonObject toRet = toRetDoc.as<JsonObject>();
  //_returnJSON(request, toRet);

  _returnJSON(request, toRetDoc.as<JsonObject>() );
}

void handleAPI_get_status(AsyncWebServerRequest *request)
{
  // DynamicJsonDocument toRetDoc(256);

  toRetDoc.clear();
  
  toRetDoc["uptime"] = upTime();
  toRetDoc["rssi"] = WiFi.RSSI();
  toRetDoc["disconnects"] = connectionMuxLostCount;
#if defined(ESP8266)
  toRetDoc["ESPrr"] = ESP.getResetReason();
  toRetDoc["ESPheap"] = ESP.getFreeHeap();
  toRetDoc["ESPhfrag"] = ESP.getHeapFragmentation();
  toRetDoc["ESPhmax"] = ESP.getMaxFreeBlockSize();
#endif

  _returnJSON(request, toRetDoc.as<JsonObject>() );

}
// bogus handler for testing


void nothingAtAll(AsyncWebServerRequest *request)
{
  request->send(200, "text/html",  "options are: /status, /sensor/{list,describe,temperature,calibrate}, /room/{list,temperature}");

}

void handleAPI_room_temperature(AsyncWebServerRequest *request)
{
  _returnJSON400(request, "Not implemented yet");
}


void handleAPI_room_list(AsyncWebServerRequest *request)
{
  toRetDoc.clear();

  JsonArray rooms = toRetDoc.createNestedArray("rooms");

  int roomId = 0;

  for (room_t room : Rooms)
  {
    DynamicJsonDocument r(1024);
    byte i;
    
    r.clear();

    r["name"]   = room.Name;
    r["target"] = room.targetTemp;
    r["actual"] = room.actualTemp;

    JsonArray servos = r.createNestedArray("servos");

    for ( i=0 ; i < 2 ; i++)
    {
      if(room.Servos[i]<0)
        break;
/*
      DynamicJsonDocument s(256);
      
      s.clear();
      s["servoNr"] = room.Servos[i];
      s["index"] = i;
*/

      servos.add(room.Servos[i]);
    }
    r["servocount"] = i;
    r["id"] = roomId++;
    rooms.add(r);

  }

  _returnJSON(request, toRetDoc.as<JsonObject>());

} // handleAPI_room_list()


void handleAPI_servo_status(AsyncWebServerRequest *request)
{
  const char* statusText;
  //const char* Value=httpServer.arg("id").c_str();
  //if(request->hasParam("id"))
    const char* Value = request->arg("id").c_str();
  //else
  //  Value = {};
  
  if(!strlen(Value))
    _returnJSON400(request, "mandatory id missing");

  byte i = atoi(Value);

  toRetDoc.clear();

  toRetDoc["servo"] = i;
  toRetDoc["status"]  = servoArray[i].servoState;
  
  switch(servoArray[i].servoState) {
    case SERVO_IS_OPEN:       
      statusText = "OPEN";
      break;
    case SERVO_IN_LOOP:       
      statusText = "LOOP";
      break;
    case SERVO_COUNT0_CLOSE:  
      statusText = "COUNT0";
      break;
    case SERVO_IS_CLOSED:     
      statusText = "CLOSED";
      break;
  } 

  toRetDoc["message"] = statusText;

  _returnJSON(request, toRetDoc.as<JsonObject>());

}

void handleAPI_servo_statusarray(AsyncWebServerRequest *request)
{
  toRetDoc.clear();

  JsonArray servos = toRetDoc.createNestedArray("servos");

  for (byte i=0 ; i < 10; i++)
  {  
      servos.add(servoArray[i].servoState);
  }
  _returnJSON(request, toRetDoc.as<JsonObject>());
}


void apiInit()
{
  //httpServer.on("/api/status", handleAPI_get_status);
  httpServer.on("/api/status", [](AsyncWebServerRequest *request){
    handleAPI_get_status(request);
  });

  //httpServer.on("/api/sensor/list", handleAPI_list_sensors);
  httpServer.on("/api/sensor/list", [](AsyncWebServerRequest *request){
    handleAPI_list_sensors(request);
  });
  //httpServer.on("/api/sensor/status", handleAPI_list_sensors);
  httpServer.on("/api/sensor/status", [](AsyncWebServerRequest *request){
    handleAPI_list_sensors(request);
  });

  //httpServer.on("/api/sensor/describe", handleAPI_describe_sensor);
  httpServer.on("/api/sensor/describe", [](AsyncWebServerRequest *request){
    handleAPI_describe_sensor(request);
  });
  //httpServer.on("/api/sensor/temperature", handleAPI_get_temperature);
  httpServer.on("/api/sensor/temperature", [](AsyncWebServerRequest *request){
    handleAPI_get_temperature(request);
  });
  //httpServer.on("/api/sensor/calibrate", handleAPI_calibrate_sensor);
  httpServer.on("/api/sensor/calibrate", [](AsyncWebServerRequest *request){
    handleAPI_calibrate_sensor(request);
  });

  //httpServer.on("/api/room/list", handleAPI_room_list);
  httpServer.on("/api/room/list", [](AsyncWebServerRequest *request){
    handleAPI_room_list(request);
  });

  //httpServer.on("/api/room/temperature", handleAPI_room_temperature);
  httpServer.on("/api/room/temperature", [](AsyncWebServerRequest *request){
    handleAPI_room_temperature(request);
  });

  //httpServer.on("/api/servo/status", handleAPI_servo_status);
  httpServer.on("/api/servo/status", [](AsyncWebServerRequest *request){
    handleAPI_servo_status(request);
  });
  //httpServer.on("/api/servo/statusarray", handleAPI_servo_statusarray);
  httpServer.on("/api/servo/statusarray", [](AsyncWebServerRequest *request){
    handleAPI_servo_statusarray(request);
  });

  httpServer.on("/api",  nothingAtAll);

} // apiInit()
