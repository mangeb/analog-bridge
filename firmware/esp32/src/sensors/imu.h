/**
 *  Analog Bridge — IMU (MPU9250) Module
 *
 *  Burst I2C read for accel+temp+gyro (14 bytes from 0x3B),
 *  magnetometer via FaBo9Axis library, calibration + axis remap.
 */
#ifndef AB_IMU_H
#define AB_IMU_H

#include "sensor_data.h"
#include "calibration_data.h"

// Initialize I2C and MPU9250. Returns true if sensor found.
bool imuInit();

// Read all 9 axes + temperature into SensorData.
// Applies calibration biases and axis remapping.
void imuRead(SensorData &data);

// Auto-zero gyroscope: average GYRO_CAL_SAMPLES readings.
// Must be called while car is stationary. Takes ~2.5s.
void imuCalibrateGyro();

// Calibrate accelerometer: average 256 samples on level surface.
// Saves to NVS.
void imuCalibrateAccel();

// Calibrate magnetometer: tumble sensor through all orientations for 15s.
// Saves hard-iron and soft-iron correction to NVS.
void imuCalibrateMag();

// Print current calibration values to Serial.
void imuPrintCalibration();

// Erase NVS calibration, revert to defaults.
void imuEraseCalibration();

// Get reference to calibration struct (for status display)
const IMUCalibration& imuGetCalibration();

// Is IMU available?
bool imuIsReady();

#endif // AB_IMU_H
