/**
 *  Analog Bridge — SD Logger Implementation
 *
 *  Ported from AVR analog-bridge.ino lines 944-1128.
 *  Changes from AVR:
 *    - SPI.begin() with explicit pin assignment
 *    - No F() macros
 *    - printDegE7() for lat/lon formatting preserved for CSV compat
 */
#include "sd_logger.h"
#include "config.h"
#include <SPI.h>
#include <SD.h>

static File logFile;
static char logFilename[16] = "";
static unsigned long logRowCount = 0;
static uint8_t sdErrorCount = 0;
static unsigned long lastFlush = 0;

//----------------------------------------------------------------
// Helper: print degE7 as decimal degrees (same as AVR for CSV compat)
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
// Write a single CSV row
//----------------------------------------------------------------
static void printRow(Print &out, const SensorData &data, float now,
                     bool keyframePending, uint16_t keyframeCount) {
  out.print(now, 3);               out.print(',');
  printDegE7(out, data.lat);       out.print(',');
  printDegE7(out, data.lon);       out.print(',');
  out.print(data.speed);           out.print(',');
  out.print(data.alt);             out.print(',');
  out.print(data.dir);             out.print(',');
  out.print(data.satellites);      out.print(',');
  out.print(data.accx);            out.print(',');
  out.print(data.accy);            out.print(',');
  out.print(data.accz);            out.print(',');
  out.print(data.rotx);            out.print(',');
  out.print(data.roty);            out.print(',');
  out.print(data.rotz);            out.print(',');
  out.print(data.magx);            out.print(',');
  out.print(data.magy);            out.print(',');
  out.print(data.magz);            out.print(',');
  out.print(data.imuTemp, 1);      out.print(',');
  out.print(data.afr);             out.print(',');
  out.print(data.afr1);            out.print(',');
  out.print(data.vss);             out.print(',');
  out.print(data.map);             out.print(',');
  out.print(data.oilp);            out.print(',');
  out.print(data.coolant);         out.print(',');
  out.print(data.gpsStale ? 1 : 0); out.print(',');
  out.println(keyframePending ? keyframeCount : 0);
}

//----------------------------------------------------------------
// Public API
//----------------------------------------------------------------

void sdInit() {
  SPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  pinMode(SD_CS_PIN, OUTPUT);
  Serial.println("INF: SD SPI initialized");
}

bool sdOpenLogFile(const char* filenameBase, const char* dateStr) {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("ERR: SD card failed or not present");
    return false;
  }

  char fname[24];
  int index = 0;
  snprintf(fname, sizeof(fname), "%s_%d.csv", filenameBase, index);

  while (SD.exists(fname)) {
    index++;
    snprintf(fname, sizeof(fname), "%s_%d.csv", filenameBase, index);
  }

  Serial.printf("INF: Opening log %s\n", fname);
  logFile = SD.open(fname, FILE_WRITE);
  if (!logFile) return false;

  strncpy(logFilename, fname, sizeof(logFilename) - 1);
  logFilename[sizeof(logFilename) - 1] = '\0';
  logRowCount = 0;
  sdErrorCount = 0;

  // Write header
  if (dateStr && dateStr[0]) {
    logFile.println(dateStr);
  }
  logFile.println("time,lat,lon,speed,alt,dir,sats,accx,accy,accz,rotx,roty,rotz,magx,magy,magz,imuTemp,afr,afr1,vss,map,oilp,coolant,gpsStale,keyframe");
  logFile.println("(s),(deg),(deg),(mph),(ft),(deg),(#),(g),(g),(g),(dps),(dps),(dps),(uT),(uT),(uT),(C),(afr),(afr),(mph),(inHgVac),(psig),(F),(flag),(#)");
  logFile.flush();
  lastFlush = millis();

  return true;
}

bool sdWriteRow(const SensorData &data, float elapsedSec,
                bool keyframePending, uint16_t keyframeCount) {
  if (!logFile) return true;  // no file = nothing to write, not an error

  printRow(logFile, data, elapsedSec, keyframePending, keyframeCount);
  logRowCount++;

  // Flush every 1 second
  if (millis() - lastFlush > FLUSH_INTERVAL) {
    logFile.flush();
    if (logFile.getWriteError()) {
      sdErrorCount++;
      logFile.clearWriteError();
      Serial.printf("ERR: SD write fail #%d\n", sdErrorCount);
      if (sdErrorCount >= SD_MAX_ERRORS) {
        Serial.println("ERR: SD card failed, stopping recording");
        return false;  // caller should stop recording
      }
    } else {
      sdErrorCount = 0;
    }
    lastFlush = millis();
  }

  return true;
}

void sdCloseLogFile() {
  if (logFile) {
    logFile.flush();
    logFile.close();
  }
}

const char* sdGetFilename() {
  return logFilename;
}

unsigned long sdGetRowCount() {
  return logRowCount;
}
