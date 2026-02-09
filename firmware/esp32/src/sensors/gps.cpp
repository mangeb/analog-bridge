/**
 *  Analog Bridge — GPS Implementation
 *
 *  Ported from AVR analog-bridge.ino lines 390-712.
 *  Changes from AVR:
 *    - TinyGPS++ replaces NeoGPS (ESP32 compatible, simpler API)
 *    - HardwareSerial(1) with explicit pins
 *    - UBX commands stored as plain const arrays (no PROGMEM needed)
 *    - No pgm_read_byte() — direct array access
 *    - Lat/lon stored as degE7 (int32) for CSV compatibility with AVR logs
 */
#include "gps.h"
#include "config.h"
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

static HardwareSerial gpsSerial(GPS_UART_NUM);
static TinyGPSPlus gps;
static bool firstFix = false;
static unsigned long lastFixMs = 0;
static char filenameBuf[16] = "CLOG";
static char dateBuf[24] = "";

//----------------------------------------------------------------
// UBX GPS configuration commands (no PROGMEM needed on ESP32)
//----------------------------------------------------------------

// Set navigation measurement rate to 200ms (5Hz)
static const uint8_t UBX_CFG_RATE_5HZ[] = {
  0xB5, 0x62,             // UBX sync chars
  0x06, 0x08,             // Class: CFG, ID: RATE
  0x06, 0x00,             // Payload length: 6 bytes
  0xC8, 0x00,             // measRate = 200ms (0x00C8)
  0x01, 0x00,             // navRate  = 1 cycle
  0x01, 0x00,             // timeRef  = UTC (1)
  0xDE, 0x6A              // Checksum
};

// Set UART1 baud rate to 115200
static const uint8_t UBX_CFG_PRT_115200[] = {
  0xB5, 0x62,             // UBX sync chars
  0x06, 0x00,             // Class: CFG, ID: PRT
  0x14, 0x00,             // Payload length: 20 bytes
  0x01,                   // portID = UART1
  0x00,                   // reserved
  0x00, 0x00,             // txReady (disabled)
  0xD0, 0x08, 0x00, 0x00, // mode: 8N1
  0x00, 0xC2, 0x01, 0x00, // baudRate = 115200
  0x07, 0x00,             // inProtoMask: UBX + NMEA + RTCM
  0x03, 0x00,             // outProtoMask: UBX + NMEA
  0x00, 0x00,             // flags
  0x00, 0x00,             // reserved
  0xC0, 0x7E              // Checksum
};

static void sendUBX(const uint8_t *cmd, size_t len) {
  for (size_t i = 0; i < len; i++) {
    gpsSerial.write(cmd[i]);
  }
}

//----------------------------------------------------------------
// Public API
//----------------------------------------------------------------

void gpsInit() {
  gpsReconfigure();
}

void gpsReconfigure() {
  // Step 1: switch u-blox from 9600 → 115200
  gpsSerial.begin(GPS_BAUD_INIT, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  delay(50);
  sendUBX(UBX_CFG_PRT_115200, sizeof(UBX_CFG_PRT_115200));
  delay(50);

  // Step 2: reopen at new baud rate
  gpsSerial.updateBaudRate(GPS_BAUD_FAST);
  delay(50);

  // Step 3: set 5Hz update rate
  sendUBX(UBX_CFG_RATE_5HZ, sizeof(UBX_CFG_RATE_5HZ));
  delay(50);

  Serial.println("INF: GPS configured — 115200 baud, 5Hz");
}

void gpsRead(SensorData &data) {
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isUpdated() && gps.location.isValid()) {
    firstFix = true;
    lastFixMs = millis();

    // Store as degE7 for CSV compatibility with AVR logs
    data.lat = (long)(gps.location.lat() * 1e7);
    data.lon = (long)(gps.location.lng() * 1e7);
    data.speed = gps.speed.mph();
    data.alt   = gps.altitude.feet();
    data.dir   = gps.course.deg();
    data.satellites = gps.satellites.value();

    // Build filename from GPS time
    if (gps.time.isValid() && gps.date.isValid()) {
      int localHour = (gps.time.hour() + UTC_OFFSET + 24) % 24;
      snprintf(dateBuf, sizeof(dateBuf), "%02d/%02d/%02d %02d:%02d:%02d",
        gps.date.day(), gps.date.month(), gps.date.year() % 100,
        localHour, gps.time.minute(), gps.time.second());
      snprintf(filenameBuf, sizeof(filenameBuf), "%02d%02d%02d",
        gps.date.day(), localHour, gps.time.minute());
    }

#ifdef GPS_DEBUG
    Serial.printf("GPS: %.7f, %.7f  %.1f mph  %d sats\n",
      gps.location.lat(), gps.location.lng(),
      gps.speed.mph(), gps.satellites.value());
#endif
  }
}

bool gpsHasFix() {
  return firstFix;
}

const char* gpsGetFilenameBase() {
  return filenameBuf;
}

const char* gpsGetDateString() {
  return dateBuf;
}

unsigned long gpsGetLastFixTime() {
  return lastFixMs;
}
