/**
 *  Analog Bridge — Shared Sensor Data Structure
 *
 *  Central data contract used by all platform targets (AVR, ESP32).
 *  Every sensor writer fills this struct; every consumer reads it.
 *
 *  CSV column order matches the struct field order for clarity:
 *    time, lat, lon, speed, alt, dir, sats,
 *    accx/y/z, rotx/y/z, magx/y/z, imuTemp,
 *    afr, afr1, vss, map, oilp, coolant, gpsStale, keyframe
 */
#ifndef AB_SENSOR_DATA_H
#define AB_SENSOR_DATA_H

#include <stdint.h>

struct SensorData {
  // GPS
  long     lat, lon;            // degE7 (latitude × 1e7) for precision
  float    speed;               // mph
  float    alt;                 // ft
  float    dir;                 // degrees (heading)
  uint8_t  satellites;          // satellite count
  bool     gpsStale;            // true if no fix for > GPS_STALE_MS

  // IMU — accelerometer, gyroscope, magnetometer, temperature
  // Axes are in car frame after remap: X=fwd, Y=right, Z=down (SAE)
  float accx, accy, accz;      // g
  float rotx, roty, rotz;      // dps (degrees per second)
  float magx, magy, magz;      // uT (microtesla)
  float imuTemp;                // °C (MPU9250 die temperature)

  // ISP2 / Engine
  float afr, afr1;             // air-fuel ratio (bank 1 and 2)
  float vss;                   // vehicle speed (mph) from reluctor
  float map;                   // manifold vacuum/boost (inHgVac)
  float oilp;                  // oil pressure (psig)
  float coolant;               // coolant temperature (°F)
};

#endif // AB_SENSOR_DATA_H
