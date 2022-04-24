#ifndef API_H
#define API_H

#include <WebServer.h>        // v2.0.0 (part of Arduino ESP32 Core @2.0.2)
#include <ArduinoJson.h>      // v6.19.3 -(https://arduinojson.org/?utm_source=meta&utm_medium=library.properties)

/*

  void _returnJSON(JsonObject obj);
  void _returnJSON400(const char * message);
  void _cacheJSON();

  void usage();

  int _find_sensorIndex_in_query();

  void calibrate_low(int sensorIndex, float lowCalibratedTemp);
  void calibrate_high(int sensorIndex, float hiCalibratedTemp);
  void calibrate_sensor(int sensorIndex, float realTemp);

  void GET_status();

  void GET_sensor_list();
  void GET_sensor_calibrate();
  void GET_sensor_describe();
  void GET_sensor_temperature();
  void GET_sensor_status();

  void PUT_room();
  void GET_room_temperature();
  void GET_room_list();
  
  void GET_servo_status();
  void GET_servo_statusarray();
  void GET_servo_reasonarray();
  
*/


  extern boolean cacheEmpty;

  void API_Init(WebServer *srv);


#endif
