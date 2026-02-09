/**
 *  Analog Bridge — IMU (MPU9250) Implementation
 *
 *  Ported from AVR analog-bridge.ino lines 94-786.
 *  Changes from AVR:
 *    - EEPROM → Preferences (NVS) for calibration storage
 *    - Wire.begin() with explicit SDA/SCL pins
 *    - No F() macros (unnecessary on ESP32)
 *    - Uses shared calibration_data.h for struct + axis defines
 */
#include "imu.h"
#include "config.h"
#include <Wire.h>
#include <FaBo9Axis_MPU9250.h>
#include <Preferences.h>

// MPU9250 register addresses (from FaBo library header)
#ifndef MPU9250_SLAVE_ADDRESS
#define MPU9250_SLAVE_ADDRESS 0x68
#endif
#ifndef MPU9250_ACCEL_XOUT_H
#define MPU9250_ACCEL_XOUT_H  0x3B
#endif

static FaBo9Axis mpu9250;
static IMUCalibration cal = {};
static bool ready = false;
static Preferences prefs;

//----------------------------------------------------------------
// NVS Calibration Storage
//----------------------------------------------------------------

static bool loadCalibration() {
  prefs.begin("imu-cal", true);  // read-only
  uint16_t magic = prefs.getUShort("magic", 0);
  if (magic != CAL_MAGIC) {
    prefs.end();
    memset(&cal, 0, sizeof(cal));
    cal.magScale[0] = cal.magScale[1] = cal.magScale[2] = 1.0f;
    return false;
  }
  prefs.getBytes("accelBias", cal.accelBias, sizeof(cal.accelBias));
  prefs.getBytes("magBias", cal.magBias, sizeof(cal.magBias));
  prefs.getBytes("magScale", cal.magScale, sizeof(cal.magScale));
  prefs.end();

  // Gyro bias is always re-zeroed at boot
  cal.gyroBias[0] = cal.gyroBias[1] = cal.gyroBias[2] = 0.0f;
  cal.magic = CAL_MAGIC;
  return true;
}

static void saveCalibration() {
  prefs.begin("imu-cal", false);  // read-write
  prefs.putUShort("magic", CAL_MAGIC);
  prefs.putBytes("accelBias", cal.accelBias, sizeof(cal.accelBias));
  prefs.putBytes("magBias", cal.magBias, sizeof(cal.magBias));
  prefs.putBytes("magScale", cal.magScale, sizeof(cal.magScale));
  prefs.end();
  cal.magic = CAL_MAGIC;
}

//----------------------------------------------------------------
// Public API
//----------------------------------------------------------------

bool imuInit() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);

  if (mpu9250.begin()) {
    ready = true;
    Serial.println("INF: MPU9250 OK");

    if (loadCalibration()) {
      Serial.println("INF: NVS calibration loaded");
    } else {
      Serial.println("INF: No NVS calibration (use 'c'/'m' to calibrate)");
    }

    imuCalibrateGyro();
    return true;
  }

  ready = false;
  Serial.println("ERR: MPU9250 not found, continuing without IMU");
  return false;
}

bool imuIsReady() {
  return ready;
}

const IMUCalibration& imuGetCalibration() {
  return cal;
}

void imuRead(SensorData &data) {
  if (!ready) return;

  // Burst read: accel(6) + temp(2) + gyro(6) = 14 bytes from 0x3B
  uint8_t buf[14];
  Wire.beginTransmission(MPU9250_SLAVE_ADDRESS);
  Wire.write(MPU9250_ACCEL_XOUT_H);
  Wire.endTransmission(false);  // repeated start
  Wire.requestFrom((uint8_t)MPU9250_SLAVE_ADDRESS, (uint8_t)14);
  for (uint8_t i = 0; i < 14 && Wire.available(); i++) {
    buf[i] = Wire.read();
  }

  // --- Raw readings in chip frame ---
  // Accelerometer (bytes 0-5) — 2g full scale: raw / 16384.0 = g
  int16_t rawAx = ((int16_t)buf[0]  << 8) | buf[1];
  int16_t rawAy = ((int16_t)buf[2]  << 8) | buf[3];
  int16_t rawAz = ((int16_t)buf[4]  << 8) | buf[5];
  float chipAcc[3];
  chipAcc[0] = (float)rawAx / 16384.0f - cal.accelBias[0];
  chipAcc[1] = (float)rawAy / 16384.0f - cal.accelBias[1];
  chipAcc[2] = (float)rawAz / 16384.0f - cal.accelBias[2];

  // Temperature (bytes 6-7)
  int16_t rawTemp = ((int16_t)buf[6] << 8) | buf[7];
  data.imuTemp = (float)rawTemp / 333.87f + 21.0f;

  // Gyroscope (bytes 8-13) — 250dps full scale: raw / 131.0 = deg/s
  int16_t rawGx = ((int16_t)buf[8]  << 8) | buf[9];
  int16_t rawGy = ((int16_t)buf[10] << 8) | buf[11];
  int16_t rawGz = ((int16_t)buf[12] << 8) | buf[13];
  float chipGyro[3];
  chipGyro[0] = (float)rawGx / 131.0f - cal.gyroBias[0];
  chipGyro[1] = (float)rawGy / 131.0f - cal.gyroBias[1];
  chipGyro[2] = (float)rawGz / 131.0f - cal.gyroBias[2];

  // Magnetometer — separate AK8963 I2C device
  float chipMag[3];
  mpu9250.readMagnetXYZ(&chipMag[0], &chipMag[1], &chipMag[2]);
  chipMag[0] = (chipMag[0] - cal.magBias[0]) * cal.magScale[0];
  chipMag[1] = (chipMag[1] - cal.magBias[1]) * cal.magScale[1];
  chipMag[2] = (chipMag[2] - cal.magBias[2]) * cal.magScale[2];

  // --- Axis remap: chip frame → car frame (SAE: X=fwd, Y=right, Z=down) ---
  data.accx = chipAcc[AXIS_FWD_IDX]    * AXIS_FWD_SIGN;
  data.accy = chipAcc[AXIS_RIGHT_IDX]  * AXIS_RIGHT_SIGN;
  data.accz = chipAcc[AXIS_DOWN_IDX]   * AXIS_DOWN_SIGN;
  data.rotx = chipGyro[AXIS_FWD_IDX]   * AXIS_FWD_SIGN;
  data.roty = chipGyro[AXIS_RIGHT_IDX] * AXIS_RIGHT_SIGN;
  data.rotz = chipGyro[AXIS_DOWN_IDX]  * AXIS_DOWN_SIGN;
  data.magx = chipMag[AXIS_FWD_IDX]    * AXIS_FWD_SIGN;
  data.magy = chipMag[AXIS_RIGHT_IDX]  * AXIS_RIGHT_SIGN;
  data.magz = chipMag[AXIS_DOWN_IDX]   * AXIS_DOWN_SIGN;
}

void imuCalibrateGyro() {
  float sum[3] = {0, 0, 0};

  Serial.print("INF: Gyro zero — hold still (");
  Serial.print(GYRO_CAL_SAMPLES);
  Serial.print(" samples)...");

  for (int i = 0; i < GYRO_CAL_SAMPLES; i++) {
    uint8_t buf[6];
    Wire.beginTransmission(MPU9250_SLAVE_ADDRESS);
    Wire.write(0x43);  // GYRO_XOUT_H
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU9250_SLAVE_ADDRESS, (uint8_t)6);
    for (uint8_t j = 0; j < 6 && Wire.available(); j++) {
      buf[j] = Wire.read();
    }
    int16_t gx = ((int16_t)buf[0] << 8) | buf[1];
    int16_t gy = ((int16_t)buf[2] << 8) | buf[3];
    int16_t gz = ((int16_t)buf[4] << 8) | buf[5];
    sum[0] += (float)gx / 131.0f;
    sum[1] += (float)gy / 131.0f;
    sum[2] += (float)gz / 131.0f;
    delay(10);
  }

  cal.gyroBias[0] = sum[0] / GYRO_CAL_SAMPLES;
  cal.gyroBias[1] = sum[1] / GYRO_CAL_SAMPLES;
  cal.gyroBias[2] = sum[2] / GYRO_CAL_SAMPLES;

  Serial.println(" done");
  Serial.printf("INF: Gyro bias: %.3f, %.3f, %.3f dps\n",
    cal.gyroBias[0], cal.gyroBias[1], cal.gyroBias[2]);
}

void imuCalibrateAccel() {
  const int N = 256;
  float sum[3] = {0, 0, 0};

  Serial.printf("INF: Accel cal — place level, hold still (%d samples)...", N);

  for (int i = 0; i < N; i++) {
    uint8_t buf[6];
    Wire.beginTransmission(MPU9250_SLAVE_ADDRESS);
    Wire.write(MPU9250_ACCEL_XOUT_H);
    Wire.endTransmission(false);
    Wire.requestFrom((uint8_t)MPU9250_SLAVE_ADDRESS, (uint8_t)6);
    for (uint8_t j = 0; j < 6 && Wire.available(); j++) {
      buf[j] = Wire.read();
    }
    int16_t ax = ((int16_t)buf[0] << 8) | buf[1];
    int16_t ay = ((int16_t)buf[2] << 8) | buf[3];
    int16_t az = ((int16_t)buf[4] << 8) | buf[5];
    sum[0] += (float)ax / 16384.0f;
    sum[1] += (float)ay / 16384.0f;
    sum[2] += (float)az / 16384.0f;
    delay(10);
  }

  cal.accelBias[0] = sum[0] / N;
  cal.accelBias[1] = sum[1] / N;
  cal.accelBias[2] = sum[2] / N - 1.0f;  // expect +1g (chip Z-up at rest)

  saveCalibration();
  Serial.println(" done, saved to NVS");
  Serial.printf("INF: Accel bias: %.4f, %.4f, %.4f g\n",
    cal.accelBias[0], cal.accelBias[1], cal.accelBias[2]);
}

void imuCalibrateMag() {
  float minV[3] = { 9999,  9999,  9999};
  float maxV[3] = {-9999, -9999, -9999};
  const unsigned long duration = 15000;
  unsigned long start = millis();
  int samples = 0;

  Serial.println("INF: Mag cal — slowly tumble sensor through all orientations");
  Serial.println("INF: You have 15 seconds. Rotate in all axes...");

  while (millis() - start < duration) {
    float mx, my, mz;
    mpu9250.readMagnetXYZ(&mx, &my, &mz);

    if (mx < minV[0]) minV[0] = mx;
    if (mx > maxV[0]) maxV[0] = mx;
    if (my < minV[1]) minV[1] = my;
    if (my > maxV[1]) maxV[1] = my;
    if (mz < minV[2]) minV[2] = mz;
    if (mz > maxV[2]) maxV[2] = mz;
    samples++;

    if (samples % 200 == 0) Serial.print('.');
    delay(10);
  }

  Serial.println();
  Serial.printf("INF: %d samples collected\n", samples);

  cal.magBias[0] = (maxV[0] + minV[0]) / 2.0f;
  cal.magBias[1] = (maxV[1] + minV[1]) / 2.0f;
  cal.magBias[2] = (maxV[2] + minV[2]) / 2.0f;

  float range[3];
  range[0] = (maxV[0] - minV[0]) / 2.0f;
  range[1] = (maxV[1] - minV[1]) / 2.0f;
  range[2] = (maxV[2] - minV[2]) / 2.0f;
  float avgRange = (range[0] + range[1] + range[2]) / 3.0f;

  if (avgRange < 1.0f) {
    Serial.println("ERR: Mag range too small — did you rotate the sensor?");
    return;
  }

  cal.magScale[0] = avgRange / range[0];
  cal.magScale[1] = avgRange / range[1];
  cal.magScale[2] = avgRange / range[2];

  saveCalibration();
  Serial.println("INF: Mag cal saved to NVS");
  Serial.printf("INF: Hard-iron: %.1f, %.1f, %.1f uT\n",
    cal.magBias[0], cal.magBias[1], cal.magBias[2]);
  Serial.printf("INF: Soft-iron: %.3f, %.3f, %.3f\n",
    cal.magScale[0], cal.magScale[1], cal.magScale[2]);
}

void imuPrintCalibration() {
  Serial.println("--- IMU Calibration ---");
  bool valid = (cal.magic == CAL_MAGIC);
  Serial.printf("NVS:        %s\n", valid ? "VALID" : "EMPTY (using defaults)");
  Serial.printf("Gyro bias:  %.3f, %.3f, %.3f dps (auto-zeroed at boot)\n",
    cal.gyroBias[0], cal.gyroBias[1], cal.gyroBias[2]);
  Serial.printf("Accel bias: %.4f, %.4f, %.4f g\n",
    cal.accelBias[0], cal.accelBias[1], cal.accelBias[2]);
  Serial.printf("Mag bias:   %.1f, %.1f, %.1f uT\n",
    cal.magBias[0], cal.magBias[1], cal.magBias[2]);
  Serial.printf("Mag scale:  %.3f, %.3f, %.3f\n",
    cal.magScale[0], cal.magScale[1], cal.magScale[2]);

  const char* axisName[] = {"X", "Y", "Z"};
  Serial.printf("Axis remap: fwd=%s%s right=%s%s down=%s%s\n",
    axisName[AXIS_FWD_IDX], AXIS_FWD_SIGN > 0 ? "+" : "-",
    axisName[AXIS_RIGHT_IDX], AXIS_RIGHT_SIGN > 0 ? "+" : "-",
    axisName[AXIS_DOWN_IDX], AXIS_DOWN_SIGN > 0 ? "+" : "-");
}

void imuEraseCalibration() {
  memset(&cal, 0, sizeof(cal));
  cal.magScale[0] = cal.magScale[1] = cal.magScale[2] = 1.0f;

  prefs.begin("imu-cal", false);
  prefs.clear();
  prefs.end();

  Serial.println("INF: Calibration erased from NVS");
}
