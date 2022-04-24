/*
***************************************************************************
**  Program  : helperStuff, part of FloorTempMonitor
**  Version  : v0.6.9
**
**  Copyright 2019, 2020, 2021, 2022 Willem Aandewiel
**
**  TERMS OF USE: MIT License. See bottom of file.
***************************************************************************
*/

/*********************************************************************************
  Uitgangspunten:

  S0 - Sensor op position '0' is verwarmingsketel water UIT (Flux In)
       deze sensor moet een servoNr '-1' hebben -> want geen klep.
  
  S1 - Sensor op position '1' is (Flux) retour naar verwarmingsketel
       servoNr '-1' -> want geen klep

*/
#include <FS.h>
#include <LittleFS.h>           // v2.0.0 (part of Arduino Core ESP32 @2.0.2)

#include "FTMConfig.h"
#include "Debug.h"
#include "profiling.h"
#include "timers.h"

#include "sensors.h"
#include "servos.h"
#include "restapi.h"
#include "LedPin.h"
#include "foreach.h"

#define SENSOR_FIRST    0
#define SENSOR_LAST     (noSensors-1)

#define SENSOR_REC_FIRST 0 
#define SENSOR_REC_LAST  (noSensorRecs - 1)

DECLARE_TIMERm(sensorPoll,      1)  //-- fire every minute

extern bool LittleFSmounted;

static int  noSensorRecs=0;

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
static OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
static DallasTemperature waterSensors(&oneWire);

void sensors::Init()
{
  Load();
  setupDallasSensors();
  Save();
  handleSensors();
}

void sensors::Loop()
{
  if (_needToPoll())
    handleSensors();
}
// function to print a device address
//===========================================================================================
char * sensors::sensorIDFormat(DeviceAddress deviceAddress)
{
  static char devID[24];

  sprintf(devID, "0x%02x%02x%02x%02x%02x%02x%02x%02x", deviceAddress[0], deviceAddress[1]
          , deviceAddress[2], deviceAddress[3]
          , deviceAddress[4], deviceAddress[5]
          , deviceAddress[6], deviceAddress[7]);
  return devID;
} // getSensorID()

#define TESTDATA
//===========================================================================================
// getRawTemp: gets real, uncalibrated temp from sensor or random temp in TESTDATA scenario 
float sensors::getRawTemp(int8_t devNr)
{
  float tempR;
    
#ifdef TESTDATA       
  static float tempR0;
  DebugTln("--> TESTDATA <--");
  if (devNr == 0 ) {
    tempR0 = (tempR0 + (random(273.6, 591.2) / 10.01)) /2.0;   // hjm: 25.0 - 29.0 ?  
    tempR = tempR0;          
  } else  
    tempR = (random(201.20, 308.8) / 10.02);               
#else
  tempR = waterSensors.getTempCByIndex(cbIndex[devNr]);
#endif

  if (tempR < -2.0 || tempR > 102.0) {
    DebugTf("Sensor [%d][%s] invalid reading [%.3f*C]\n", devNr, name[devNr], tempR);
    return 99.9;                               
  }
  DebugTf("Water temp sensor %02d returned %.3f*C\n", devNr,tempR);
  return tempR;  
}

//===========================================================================================
float sensors::_calibrated(float tempR, int8_t devNr)
{
  return (tempR + tempOffset[devNr]) * tempFactor[devNr];
}

void sensors::updateSensorData(int8_t devNr)
{
  tempRaw[devNr] = getRawTemp(devNr);
  tempC[devNr] = _calibrated(tempRaw[devNr], devNr);
}

//===========================================================================================
boolean sensors::_needToPoll()
{
  boolean toReturn = false;
   
  if ( DUE (sensorPoll) ) {
    toReturn = true;
 
    // let LED flash on during probing of sensors
    
    digitalWrite(LED_BUILTIN, LED_ON);
    
    // call waterSensors.requestTemperatures() to issue a global temperature
    // request to all devices on the bus
   
    DebugT("Requesting water temperatures...\n");
    timeThis(waterSensors.requestTemperatures());
    yield();
    digitalWrite(LED_BUILTIN, LED_OFF);
  } 
    
  return toReturn;
}

//===========================================================================================
void sensors::handleSensors()
{
  
    cacheEmpty = true; // force cache refresh upon next use

    FOREACH(SENSOR, sensorNr) 
    {
      yield();
      timeThis(updateSensorData(sensorNr));
    }
      
  
} // handleSensors()

int8_t sensors::getIndexFor(const char * Value)
{
  
      FOREACH(SENSOR, i) // used to be noRecs
      {
        DebugTf("Comparing %s/%s with %s\n", name[i], sensorID[i], Value);
        
        if ( !strcmp(name[i],Value) || !strcmp(sensorID[i],Value))
        {
            return i;
        }
      }

      return -1;
}

void sensors::Print()
{
  FOREACH(SENSOR, s) 
  {
    Debugf("[%2d] => [%2d], [%s], [%-20.20s], [%7.6f], [%7.6f]\n", s
           , cbIndex[s]
           , sensorID[s]
           , name[s]
           , tempOffset[s]
           , tempFactor[s]);
     
  }
} // Print()

void sensors::setupDallasSensors()
{
  byte i;
  byte present =  0;
  byte type_s  = -1;
  byte data[12];
  float celsius, fahrenheit;

  waterSensors.begin();
  noSensors = waterSensors.getDeviceCount();
  DebugTf("Locating devices...found %d devices\n", noSensors);

  if(noSensors != noSensorRecs)
  {
    DebugTf("Change in sensors detected: got %d records for %d sensors\n", noSensorRecs, noSensors);
  }
  oneWire.reset_search();
  FOREACH(SENSOR, sensorNr) {
    Debugln();
    DebugTf("Checking device [%2d]\n", sensorNr);
    
    if ( !oneWire.search(DS18B20)) {
      DebugTln("No more addresses.");
      Debugln();
      oneWire.reset_search();
      delay(250);
      break;
    }

    DebugTf("ROM = %s", sensorIDFormat(DS18B20));

    if (OneWire::crc8(DS18B20, 7) != DS18B20[7]) {
      Debugln(" => CRC is not valid!");
      sensorNr--;
    }
    else {
      Debugln();
 
      // the first ROM byte indicates which chip
      switch (DS18B20[0]) {
        case 0x10:
          DebugTln("  Chip = DS18S20");  // or old DS1820
          type_s = 1;
          break;
        case 0x28:
          DebugTln("  Chip = DS18B20");
          type_s = 0;
          break;
        case 0x22:
          DebugTln("  Chip = DS1822");
          type_s = 0;
          break;
        default:
          DebugTln("Device is not a DS18x20 family device.");
          type_s = -1;
          break;
      } // switch(addr[0]) 

      oneWire.reset();
      oneWire.select(DS18B20);
      //oneWire.write(0x44, 1);        // start conversion, with parasite power on at the end
  
      delay(1000);     // maybe 750ms is enough, maybe not
      // we might do a oneWire.depower() here, but the reset will take care of it.
  
      present = oneWire.reset();
      oneWire.select(DS18B20);    
      oneWire.write(0xBE);         // Read Scratchpad

      DebugT("  Data = ");
      Debug(present, HEX);
      Debug(" ");
      for ( i = 0; i < 9; i++) {           // we need 9 bytes
        data[i] = oneWire.read();
        Debug(data[i], HEX);
        Debug(" ");
      }
      Debug(" CRC=");
      Debug(OneWire::crc8(data, 8), HEX);
      Debugln();

      // Convert the data to actual temperature
      // because the result is a 16 bit signed integer, it should
      // be stored to an "int16_t" type, which is always 16 bits
      // even when compiled on a 32 bit processor.
      int16_t raw = (data[1] << 8) | data[0];
      if (type_s) {
        raw = raw << 3; // 9 bit resolution default
        if (data[7] == 0x10) {
          // "count remain" gives full 12 bit resolution
          raw = (raw & 0xFFF0) + 12 - data[6];
        }
      } else {
        byte cfg = (data[4] & 0x60);
        // at lower res, the low bits are undefined, so let's zero them
        if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
        else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
        else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
        //// default is 12 bit resolution, 750 ms conversion time
      }
      celsius = (float)raw / 16.0;
      fahrenheit = celsius * 1.8 + 32.0;
      DebugT("  Temperature = ");
      Debug(celsius);
      Debug(" Celsius, ");
      Debug(fahrenheit);
      Debugln(" Fahrenheit");
      
      DebugTf("Device [%2d] sensorID: [%s] ..\n", sensorNr, sensorIDFormat(DS18B20));
      
      sensorMatchOrAdd(sensorIDFormat(DS18B20),sensorNr); // add sensor to _SA if not read from .ini file & update .index to sensorNr

    } // CRC is OK
  
  } // for noSensors ...
  
} // setupDallasSensors()

bool sensors::sensorMatchOrAdd(char* devID, int sensorNr)
{ 
  // see if sensor detected on bus can be matched with existing record in .ini (_SA)

  FOREACH (SENSOR_REC, i) // used to be sensorRec
  {
    if(!strcmp(sensorID[i], devID))
    {
      DebugTf("Match found for %s in %s\n", devID, name[i]);
      cbIndex[i] = sensorNr;
      return true;
    }
  }

  // the sensor found on the bus is new!

  yield();

  cbIndex[noSensorRecs] = sensorNr;
  
  sprintf(sensorID[noSensorRecs], "%s", devID);
  sprintf(name[noSensorRecs], "new Sensor");
  tempOffset[noSensorRecs] = 0.00000;
  tempFactor[noSensorRecs] = 1.00000;
  servoNr[noSensorRecs]    = -1; // not attached to a servo
  deltaTemp[noSensorRecs]  = 20.0;

  noSensorRecs++;

  return false;

} 

void sensors::Save()
{
    File file = LittleFS.open("/sensors.ini", "w");
    char buffer[80];

    for (int8_t i=0 ; i < noSensorRecs ; i++)
    {    
      sprintf(buffer,"%s;%s;%f;%f;%d;%f\n",
        sensorID[i], 
        name[i],
        tempOffset[i], 
        tempFactor[i],
        servoNr[i],
        deltaTemp[i]);
    
      yield();
      //-aaw32- file.write(buffer, strlen(buffer));
      file.print(buffer);
        
    }
    file.close(); //

}

void sensors::Load()
{
  File  file;
  char  buffer[80];

  digitalWrite(LED_BUILTIN, LED_ON);

  DebugTf("readIniFile(/sensors.ini) \r\n");

  if (!LittleFSmounted) {
    DebugTln("No LittleFS filesystem..\r");
    return;
  }
  noSensorRecs=0;

  file = LittleFS.open("/sensors.ini", "r");

  while (file.available() ) {
    int _servoNr;
    int l = file.readBytesUntil('\n', buffer, sizeof(buffer));
    buffer[l] = 0;
    
    if(l < 2)
      break;
      
    yield();

    sscanf(buffer,"%[^;];%[^;];%f;%f;%d;%f\n",
       sensorID[noSensorRecs], 
       name[noSensorRecs],
      &tempOffset[noSensorRecs], 
      &tempFactor[noSensorRecs],
      &_servoNr,
      &deltaTemp[noSensorRecs]
    );

    servoNr[noSensorRecs]     = (int8_t) _servoNr;  // can not scanf into 8bit int
    cbIndex[noSensorRecs] = noSensorRecs;           // will be overridden by sensorMatchOrAdd !!
    tempC[noSensorRecs]       = -99;

    noSensorRecs++;
  }

  file.close();
  digitalWrite(LED_BUILTIN, LED_OFF);

}

void sensors::ForceUpdate()
{
    timeThis(waterSensors.requestTemperatures());  // update sensors! 
}

void sensors::calibrate_low(int si, float lowCalibratedTemp)
{

  tempRaw[si] = waterSensors.getTempCByIndex(cbIndex[si]);

  // this should be lowTemp, change tempOffset
  // so that reading + offset equals lowTemp
  // that is, offset = lowTemp - raw reading

  tempC[si] = lowCalibratedTemp;
  tempOffset[si] = lowCalibratedTemp - tempRaw[si];
  tempFactor[si] = 1.0;

  Save();
}

void sensors::calibrate_high(int si, float hiCalibratedTemp)
{

  tempRaw[si] = waterSensors.getTempCByIndex(cbIndex[si]);
  
  // calculate correction factor
  // so that 
  // raw_reading * correction_factor = actualTemp
  // --> correction_factor = actualTemp/raw_reading

  // r(read)=40, t(actual)=50 --> f=50/40=1.25
  // r=20  --> t=20.0*1.25=25.0

  // make sure to use the calibrated reading, 
  // so add the offset!
  
  tempC[si] = hiCalibratedTemp;
  tempFactor[si] = hiCalibratedTemp  / ( tempRaw[si] + tempOffset[si] );

  Save();

}

void sensors::checkDeltaTemps() 
{
  
  // update HOT WATER LED_OFF HJM: Not the best way to detect burner ON/OFF

  if (tempC[0] > HEATER_ON_TEMP) {
    // heater is On
    digitalWrite(LED_WHITE, LED_ON);
  } else {
    digitalWrite(LED_WHITE, LED_OFF);
  }
  // Sensor 0 is output from heater (heater-out)
  // Sensor 1 is retour to heater
  // deltaTemp is the difference from heater-out and sensor

  // LOOP over all Sensors to find all servos 

  for (int8_t s=2; s < noSensors; s++) {
    
    // DebugTf("[%2d] servoNr of [%s] ==> [%d]\n", s, _SA[s].name, _SA[s].servoNr);

    if (servoNr[s] < 0) {
      Debugln(" *SKIP*");
      continue;
    }
    
    int8_t servo=servoNr[s];

    // open or close based on return temp 

    if ( tempC[0] > HEATER_ON_TEMP && tempC[s] > HOTTEST_RETURN)
    {
      AllServos.Close(servo, WATER_HOT);    
    }
    if ( tempC[s] <= HOTTEST_RETURN )
    {
      AllServos.Open(servo, WATER_HOT);
    }
   
  } // for s ...
 
  
} // checkDeltaTemps()

sensors AllSensors;

/***************************************************************************
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the
* following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
* THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
***************************************************************************/
