/**
 *  Analog Bridge — IMU Calibration Data Structures
 *
 *  Shared between AVR (EEPROM) and ESP32 (NVS) targets.
 *  The storage backend differs, but the data structure and
 *  calibration math are identical.
 *
 *  Axis remapping: maps chip X/Y/Z to car Forward/Right/Down (SAE)
 *  Change the defines below when the sensor board is mounted differently.
 */
#ifndef AB_CALIBRATION_DATA_H
#define AB_CALIBRATION_DATA_H

#include <stdint.h>

//----------------------------------------------------------------
// Magic number — increment to invalidate old calibration data
//----------------------------------------------------------------
#define CAL_MAGIC        0xAB01

//----------------------------------------------------------------
// Gyro calibration sample count (auto-zero at boot)
//----------------------------------------------------------------
#define GYRO_CAL_SAMPLES 256

//----------------------------------------------------------------
// IMU Calibration struct
// Persisted to EEPROM (AVR) or NVS (ESP32)
//----------------------------------------------------------------
struct IMUCalibration {
  uint16_t magic;             // Must match CAL_MAGIC or struct is ignored

  // Gyro bias — auto-zeroed every boot, NOT persisted
  // Kept in struct for runtime convenience
  float gyroBias[3];          // dps offset (subtracted from raw)

  // Accelerometer offset — zeroed on level surface via 'c' command
  float accelBias[3];         // g offset (subtracted from raw)

  // Magnetometer hard-iron offset — min/max tumble cal via 'm' command
  float magBias[3];           // uT offset (subtracted from raw)

  // Magnetometer soft-iron scale — normalizes to sphere
  float magScale[3];          // multiplier per axis (nominally 1.0)
};

//----------------------------------------------------------------
// IMU axis remapping — maps chip orientation to car orientation
//
// Convention: X = forward, Y = right, Z = down (SAE / vehicle dynamics)
// Sign: +X = forward, +Y = right, +Z = down
//
// Default: chip X = car forward, Y = car right, Z = car down
// Examples:
//   Chip rotated 90° CW (looking down): fwd=+chipY, right=-chipX, down=+chipZ
//   Chip upside-down:                   fwd=+chipX, right=+chipY, down=-chipZ
//
// Each define: (source_array_index, sign)
// Index: 0=chipX, 1=chipY, 2=chipZ
//----------------------------------------------------------------
#define AXIS_FWD_IDX       0       // which chip axis is car-forward
#define AXIS_FWD_SIGN      1.0f    // +1 or -1
#define AXIS_RIGHT_IDX     1       // which chip axis is car-right
#define AXIS_RIGHT_SIGN    1.0f
#define AXIS_DOWN_IDX      2       // which chip axis is car-down
#define AXIS_DOWN_SIGN     1.0f

#endif // AB_CALIBRATION_DATA_H
