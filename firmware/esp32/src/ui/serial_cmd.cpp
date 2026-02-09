/**
 *  Analog Bridge — Serial Command Implementation
 *
 *  Ported from AVR analog-bridge.ino lines 1133-1307.
 *  Changes from AVR:
 *    - No F() macros, uses printf for cleaner formatting
 *    - ESP.getFreeHeap() replaces freeMemory()
 *    - Added 'w' command for WiFi status
 *    - EEPROM references changed to NVS
 */
#include "serial_cmd.h"
#include "config.h"
#include "sensors/imu.h"
#include "sensors/isp2.h"
#include "sensors/gps.h"
#include "logging/sd_logger.h"
#include "web/web_server.h"
#include <Arduino.h>
#include <WiFi.h>

static CmdStartCallback    cbStart    = nullptr;
static CmdStopCallback     cbStop     = nullptr;
static CmdKeyframeCallback cbKeyframe = nullptr;

static bool liveDebug = LIVE_DEBUG_DEFAULT;
static unsigned long lastLiveDebug = 0;

//----------------------------------------------------------------
// Helper: print elapsed time as compact "Xh XXm XXs"
//----------------------------------------------------------------
static void printHMS(Print &out, unsigned long ms) {
  unsigned long totalSec = ms / 1000;
  unsigned long h = totalSec / 3600;
  unsigned long m = (totalSec % 3600) / 60;
  unsigned long s = totalSec % 60;
  if (h > 0) { out.print(h); out.print("h "); }
  if (h > 0 || m > 0) {
    if (h > 0 && m < 10) out.print('0');
    out.print(m); out.print("m ");
  }
  if ((h > 0 || m > 0) && s < 10) out.print('0');
  out.print(s); out.print('s');
}

//----------------------------------------------------------------
// Helper: print degE7 as decimal degrees
//----------------------------------------------------------------
static void printDegE7(Print &out, int32_t degE7) {
  if (degE7 < 0) {
    degE7 = -degE7;
    out.print('-');
  }
  int32_t deg = degE7 / 10000000L;
  out.print(deg);
  out.print('.');
  degE7 -= deg * 10000000L;
  int32_t factor = 1000000L;
  while ((degE7 < factor) && (factor > 1L)) {
    out.print('0');
    factor /= 10L;
  }
  out.print(degE7);
}

//----------------------------------------------------------------
// Compact live debug output at 2Hz
//----------------------------------------------------------------
static void printLiveDebug(const SensorData &data, float now, bool isRecording) {
  if (millis() - lastLiveDebug < 500) return;
  lastLiveDebug = millis();

  Serial.printf("%.1fs %s  %5.1fmph %dsat%s  AFR %4.1f/%4.1f  %5.1fmph %5.1f\"Hg  OIL%3.0f CLT%4.0f  G %5.2f\n",
    now,
    isRecording ? "[REC]" : "     ",
    data.speed, data.satellites, data.gpsStale ? "!" : " ",
    data.afr, data.afr1,
    data.vss, data.map,
    data.oilp, data.coolant,
    data.accy);
}

//----------------------------------------------------------------
// Public API
//----------------------------------------------------------------

void serialCmdInit(CmdStartCallback onStart,
                   CmdStopCallback onStop,
                   CmdKeyframeCallback onKeyframe) {
  cbStart    = onStart;
  cbStop     = onStop;
  cbKeyframe = onKeyframe;
}

void serialCmdPrintStatus(const SensorData &data, bool isRecording) {
  Serial.println("--- Analog Bridge v" FW_VERSION " (ESP32-S3) ---");
  Serial.print("Uptime:    "); printHMS(Serial, millis()); Serial.println();
  if (isRecording) {
    Serial.print("Recording: YES — ");
    Serial.printf("%s, %lu rows\n", sdGetFilename(), sdGetRowCount());
  } else {
    Serial.println("Recording: NO");
  }
  Serial.printf("GPS:       %s  sats=%d  115200/5Hz\n",
    data.gpsStale ? "STALE" : "OK", data.satellites);
  Serial.printf("IMU:       %s  cal=%s\n",
    imuIsReady() ? "OK" : "FAIL",
    imuGetCalibration().magic == CAL_MAGIC ? "YES" : "NO");
  Serial.printf("ISP2:      %d LC1, %d aux\n",
    isp2GetLc1Count(), isp2GetAuxCount());
  Serial.printf("WiFi:      %s  %d clients  IP %s\n",
    WiFi.getMode() == WIFI_AP ? "AP" : "STA",
    WiFi.softAPgetStationNum(),
    WiFi.softAPIP().toString().c_str());
  Serial.printf("Debug:     %s\n", liveDebug ? "ON" : "OFF");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
}

void serialCmdProcess(const SensorData &data, bool isRecording) {
  // Live debug output (runtime toggle)
  if (liveDebug) {
    float now = (float)millis() / 1000.0f;
    printLiveDebug(data, now, isRecording);
  }

  while (Serial.available() > 0) {
    char c = Serial.read();
    switch (c) {
      case '?':
        Serial.println("--- Analog Bridge Commands ---");
        Serial.println(" Recording:");
        Serial.println("  r  Start recording to SD card");
        Serial.println("  s  Stop recording (prints session summary)");
        Serial.println("  k  Insert keyframe marker into log");
        Serial.println(" Display:");
        Serial.println("  d  Toggle live debug stream (2Hz)");
        Serial.println("  p  Sensor snapshot (all values once)");
        Serial.println("  v  System status (uptime, GPS, IMU, ISP2, WiFi)");
        Serial.println("  i  ISP2 diagnostics (AFR, VSS, MAP, OIL, CLT)");
        Serial.println(" IMU Calibration:");
        Serial.println("  c  Accel — place level & still, ~2.5s, saves NVS");
        Serial.println("  m  Mag   — tumble all axes 15s, saves NVS");
        Serial.println("  C  Show current gyro/accel/mag cal values");
        Serial.println("  E  Erase NVS cal (revert to defaults)");
        Serial.println(" GPS:");
        Serial.println("  g  Reconfigure GPS (115200 baud + 5Hz)");
        Serial.println(" WiFi:");
        Serial.println("  w  WiFi status (IP, clients, signal)");
        Serial.println("  ?  This help");
        break;
      case 'r':
        if (cbStart) cbStart();
        break;
      case 's':
        if (cbStop) cbStop();
        break;
      case 'k':
        if (isRecording) {
          if (cbKeyframe) cbKeyframe();
        } else {
          Serial.println("WRN: Not recording — keyframe ignored");
        }
        break;
      case 'd':
        liveDebug = !liveDebug;
        Serial.printf("INF: Live debug %s\n", liveDebug ? "ON" : "OFF");
        break;
      case 'p':
        Serial.println("--- Sensor Snapshot ---");
        Serial.print("GPS: ");
        printDegE7(Serial, data.lat); Serial.print(", ");
        printDegE7(Serial, data.lon);
        Serial.printf("  %.1f mph  sats=%d%s\n",
          data.speed, data.satellites, data.gpsStale ? " [STALE]" : "");
        Serial.printf("IMU: acc=%.2f,%.2f,%.2f  gyro=%.1f,%.1f,%.1f  temp=%.1fC\n",
          data.accx, data.accy, data.accz,
          data.rotx, data.roty, data.rotz, data.imuTemp);
        Serial.printf("MAG: %.1f,%.1f,%.1f uT\n",
          data.magx, data.magy, data.magz);
        Serial.printf("ENG: AFR=%.1f/%.1f  VSS=%.1fmph  MAP=%.1f  OIL=%.0f  CLT=%.0f\n",
          data.afr, data.afr1, data.vss, data.map, data.oilp, data.coolant);
        break;
      case 'v':
        serialCmdPrintStatus(data, isRecording);
        break;
      case 'g':
        gpsReconfigure();
        break;
      case 'i':
        Serial.printf("ISP2 state: %d\n", isp2GetState());
        Serial.printf("LC1 devices: %d\n", isp2GetLc1Count());
        Serial.printf("Aux channels: %d\n", isp2GetAuxCount());
        Serial.printf("AFR: %.1f  AFR1: %.1f\n", data.afr, data.afr1);
        Serial.printf("VSS: %.1fmph  MAP: %.1f  OIL: %.0f  CLT: %.0f\n",
          data.vss, data.map, data.oilp, data.coolant);
        break;
      case 'c':
        if (isRecording) {
          Serial.println("WRN: Stop recording before calibrating");
        } else if (!imuIsReady()) {
          Serial.println("ERR: IMU not available");
        } else {
          imuCalibrateAccel();
        }
        break;
      case 'm':
        if (isRecording) {
          Serial.println("WRN: Stop recording before calibrating");
        } else if (!imuIsReady()) {
          Serial.println("ERR: IMU not available");
        } else {
          imuCalibrateMag();
        }
        break;
      case 'C':
        imuPrintCalibration();
        break;
      case 'E':
        imuEraseCalibration();
        break;
      case 'w':
        Serial.printf("WiFi mode:    %s\n", WiFi.getMode() == WIFI_AP ? "AP" : "STA");
        Serial.printf("SSID:         %s\n", WiFi.softAPSSID().c_str());
        Serial.printf("IP:           %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("Clients:      %d\n", WiFi.softAPgetStationNum());
        Serial.printf("WS clients:   %d\n", webGetClientCount());
        break;
    }
  }
}
