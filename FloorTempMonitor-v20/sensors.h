/*
 *
 */

#ifndef FTMSENSOR_H
#define FTMSENSOR_H
 
#include <Arduino.h>
#include <OneWire.h>            // v2.3.6
#include <DallasTemperature.h>  // v3.8.0 

#define MAX_UFHLOOPS_PER_ROOM   2
#define MAX_NAMELEN             20

#define ONE_WIRE_BUS          19    // Data Wire is plugged into GPIO-19
#define _MAX_SENSORS          12    // 9 Servo's/RELAY + heater in & out
#define _MAX_NAME_LEN         12

typedef	char  Id_t[20];
typedef char  Name_t[_MAX_NAME_LEN];

class sensors {

  DeviceAddress DS18B20;

  float     _calibrated(float tempR, int8_t dev);
  boolean   _needToPoll();
  float     getRawTemp(int8_t devNr);
  void      handleSensors();
  char *    sensorIDFormat(DeviceAddress device);
  void      setupDallasSensors();
  void      updateSensorData(int8_t devNr);
  void      Load();
  void      Save();
  bool      sensorMatchOrAdd(char* devID, int sensorNr);
  void      checkDeltaTemps();

public:

  void      Init();
  void      Loop();

  void      Print();
  int8_t    getIndexFor(const char * Value);

  void      ForceUpdate();

  void      calibrate_high (int si, float hiCalibratedTemp);
  void      calibrate_low  (int si, float hiCalibratedTemp);


  Name_t    name[_MAX_SENSORS];
  Id_t      sensorID[_MAX_SENSORS];

  float     tempC[_MAX_SENSORS];           
  float     tempRaw[_MAX_SENSORS];        
  float     deltaTemp[_MAX_SENSORS];     

  float     tempOffset[_MAX_SENSORS];     // calibration
  float     tempFactor[_MAX_SENSORS];     // calibration
  
  int8_t    servoNr[_MAX_SENSORS];        // index in servoArray
  int8_t    cbIndex[_MAX_SENSORS];        // index on CB bus NOT index in _SA array!

  int8_t    noSensors;
};

extern sensors AllSensors;

#endif
