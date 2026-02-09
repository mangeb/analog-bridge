/**
 *  Analog Bridge — Automotive Datalogger
 *  1969 Chevrolet Nova, 454 BBC
 *
 *  Logs GPS, 9-axis IMU, and engine data to SD card at 12.5 Hz.
 *  Designed for Arduino Mega 2560 (also compiles for Uno with reduced I/O).
 *
 *  Hardware:
 *    - u-blox GPS module on Serial1 (9600 default, configurable to 115200/5Hz)
 *    - MPU9250 9-axis IMU on I2C (accelerometer, gyroscope, magnetometer, temp)
 *    - Innovate Motorsports ISP2 daisy-chain on Serial2 (19200 baud):
 *        SSI-4 #1: coolant temp, oil pressure
 *        LC-1  #1: wideband AFR bank 1
 *        LC-1  #2: wideband AFR bank 2
 *        SSI-4 #2: MAP, VSS (vehicle speed from reluctor)
 *    - SD card on SPI (CS pin 53 Mega / 10 Uno)
 *    - Momentary button (pin 2) to start/stop recording
 *    - LED indicator (pin 3) — solid after GPS fix, blinks while recording
 *
 *  Serial commands (115200 baud, type '?' for help):
 *    r/s  start/stop recording    d  toggle live debug
 *    p    sensor snapshot          v  system status
 *    i    ISP2 diagnostics         g  GPS 5Hz    b  GPS 115200 baud
 *
 *  CSV output (24 columns):
 *    time, lat, lon, speed, alt, dir, sats,
 *    accx/y/z, rotx/y/z, magx/y/z, imuTemp,
 *    afr, afr1, vss, map, oilp, coolant, gpsStale
 *
 *  Repository: github.com/mangeb/analog-bridge
 */
#define FW_VERSION "0.9.0"

#include <SPI.h>
#include <SD.h>
#include <NMEAGPS.h>
#include <GPSport.h>

//------------------------------------------------------------
// Check that the config files are set up properly

#if !defined( NMEAGPS_PARSE_RMC )
  #error You must uncomment NMEAGPS_PARSE_RMC in NMEAGPS_cfg.h!
#endif

#if !defined( GPS_FIX_TIME )
  #error You must uncomment GPS_FIX_TIME in GPSfix_cfg.h!
#endif

#if !defined( GPS_FIX_LOCATION )
  #error You must uncomment GPS_FIX_LOCATION in GPSfix_cfg.h!
#endif

#if !defined( GPS_FIX_SPEED )
  #error You must uncomment GPS_FIX_SPEED in GPSfix_cfg.h!
#endif

#if !defined( GPS_FIX_SATELLITES )
  #error You must uncomment GPS_FIX_SATELLITES in GPSfix_cfg.h!
#endif

#ifdef NMEAGPS_INTERRUPT_PROCESSING
  #error You must *NOT* define NMEAGPS_INTERRUPT_PROCESSING in NMEAGPS_cfg.h!
#endif

#include <Wire.h>
#include <FaBo9Axis_MPU9250.h>

//----------------------------------------------------------------
// ISP2 Serial port
// Mega: hardware Serial2 (RX=17, TX=16)
// Uno:  AltSoftSerial (RX=8, TX=9, claims PWM on pin 10)
//----------------------------------------------------------------
#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
  #define isp2Serial Serial2
#else
  #include <AltSoftSerial.h>
  AltSoftSerial isp2Serial;
#endif

//----------------------------------------------------------------
// SD card SPI chip select
// Mega: MOSI-51, MISO-50, CLK-52, CS-53
// Uno:  MOSI-11, MISO-12, CLK-13, CS-10
//----------------------------------------------------------------

#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
  const int SD_CS_PIN = 53;
#else
  const int SD_CS_PIN = 10;
#endif

//----------------------------------------------------------------
// IMU (MPU9250 9-axis + magnetometer)
//----------------------------------------------------------------
FaBo9Axis mpu9250;

//----------------------------------------------------------------
// GPS (u-blox via NeoGPS)
//----------------------------------------------------------------
static NMEAGPS gps;

//----------------------------------------------------------------
// Debug flags (uncomment to enable at compile time)
//----------------------------------------------------------------
//#define TIMING_DEBUG 1  // +140 bytes SRAM, +750 bytes FLASH
//#define GPS_DEBUG 1
//#define ISP2_DEBUG 1
//#define SERIAL_DEBUG 1  // Print every data row to Serial (high bandwidth)

// Runtime debug toggle (controlled by 'd' serial command)
static bool liveDebug = false;

//----------------------------------------------------------------
// Sensor data struct — single source of truth for all readings
//----------------------------------------------------------------
struct SensorData {
  // GPS
  long lat, lon;
  float speed, alt, dir;
  uint8_t satellites;
  bool gpsStale;
  // IMU — accelerometer, gyroscope, magnetometer, temperature
  float accx, accy, accz;
  float rotx, roty, rotz;
  float magx, magy, magz;
  float imuTemp;
  // ISP2 / Engine
  float afr, afr1;
  float vss, map, oilp, coolant;  // vss = vehicle speed (MPH) from VSS reluctor
};
static SensorData data = {};

// Logging
static File logFile;
static uint8_t sdErrorCount = 0;        // consecutive flush errors
#define SD_MAX_ERRORS  3                 // auto-stop after this many

// Timestamps
static unsigned long lastGPS = 0;
static bool isRecording = false;
static unsigned long startRecord = 0;

static bool mpu9250Ready = false;

//----------------------------------------------------------------
// ISP2 Protocol (Innovate Motorsports serial)
//----------------------------------------------------------------
#define ISP2_H_SYNC_MASK  0xA2  // High sync byte: bits 7,5,1 set
#define ISP2_L_SYNC_MASK  0x80  // Low sync byte: bit 7 set
#define ISP2_LC1_FLAG     0x40  // Bit 14 in high byte: LC-1 sub-packet

#define ISP2_MAX_WORDS    16
static byte    isp2Header[2];
static byte    isp2Data[ISP2_MAX_WORDS * 2];
static int     isp2PacketLen   = 0;
static bool    isp2IsData      = false;
static uint8_t isp2AuxCount    = 0;
static uint8_t isp2Lc1Count    = 0;

// ISP2 non-blocking state machine
enum ISP2State { ISP2_SYNC_HIGH, ISP2_SYNC_LOW, ISP2_READING_PAYLOAD };
static ISP2State isp2State      = ISP2_SYNC_HIGH;
static int       isp2BytesRead    = 0;
static int       isp2BytesExpected = 0;
static unsigned long isp2LastByte = 0;     // timestamp of last byte received
#define ISP2_TIMEOUT_MS  200               // resync if payload stalls this long

// Aux channel voltage-to-unit conversions (tune for your sensors)
// TODO: Replace with actual thermistor curve (Steinhart-Hart or lookup table)
#define AUX_COOLANT_F(v)   ((v) * 100.0)
// TODO: Verify sender range — assumes 0.5-4.5V = 0-100 PSI linear
#define AUX_OILP_PSI(v)    (((v) - 0.5) * 25.0)
// TODO: Verify MAP sensor model — assumes 1-bar (0-5V = -14.7 to +14.7)
#define AUX_MAP_INHG(v)    ((v) * 5.858 - 14.696)

// VSS (Vehicle Speed Sensor) calibration
// Derived from: 235/70R15 tire, 3.73 final drive, 17-tooth reluctor
//   Wheel circumference: 2.23 m (710 mm dia)
//   Driveshaft revs/h @ 100 mph: 269,153
//   VSS frequency @ 100 mph: 269,153 × 17 / 3600 = 1271 Hz
//   Conversion factor: 12.71 Hz per MPH
// SSI-4 frequency mode: 0-5V maps linearly to 0..SSI4_VSS_FREQ_MAX Hz
// Adjust SSI4_VSS_FREQ_MAX to match your LM Programmer frequency range setting
#define SSI4_VSS_FREQ_MAX  1500.0   // Hz — SSI-4 configured max (adjust to match)
#define VSS_HZ_PER_MPH     12.71    // Hz per MPH for this drivetrain
#define AUX_VSS_MPH(v)     (((v) / 5.0 * SSI4_VSS_FREQ_MAX) / VSS_HZ_PER_MPH)

//----------------------------------------------------------------
// Configuration constants
//----------------------------------------------------------------
#define UTC_OFFSET       -7     // PDT (UTC-7). Change to -8 for PST.
#define GPS_STALE_MS     2000   // mark GPS stale after this many ms without fix
#define FLUSH_INTERVAL   1000   // SD card flush interval (ms)
#define BLINK_INTERVAL   1000   // recording LED blink rate (ms)
#define DEBOUNCE_MS      500    // button debounce time (ms)
#define NOFIX_MSG_MS     5000   // GPS no-fix message rate limit (ms)
#define SAMPLE_INTERVAL  80     // main loop sample period (ms) = 12.5 Hz

//----------------------------------------------------------------
// UI — button and LED pins
//----------------------------------------------------------------
const int buttonPin = 2;
const int buttonLedPin = 3;

// UBX GPS configuration commands (stored in PROGMEM to save SRAM)
//----------------------------------------------------------------
// Set navigation measurement rate to 200ms (5Hz update rate)
// UBX-CFG-RATE: measRate=200ms, navRate=1 cycle, timeRef=UTC
static const byte UBX_CFG_RATE_5HZ[] PROGMEM = {
  0xB5, 0x62,             // UBX sync chars
  0x06, 0x08,             // Class: CFG, ID: RATE
  0x06, 0x00,             // Payload length: 6 bytes
  0xC8, 0x00,             // measRate = 200ms (0x00C8)
  0x01, 0x00,             // navRate  = 1 cycle
  0x01, 0x00,             // timeRef  = UTC (1)
  0xDE, 0x6A              // Checksum (CK_A, CK_B)
};

// Set UART1 baud rate to 115200 for higher GPS throughput
// UBX-CFG-PRT: portID=UART1, mode=8N1, baud=115200, in/out=UBX+NMEA
static const byte UBX_CFG_PRT_115200[] PROGMEM = {
  0xB5, 0x62,             // UBX sync chars
  0x06, 0x00,             // Class: CFG, ID: PRT
  0x14, 0x00,             // Payload length: 20 bytes
  0x01,                   // portID = UART1
  0x00,                   // reserved
  0x00, 0x00,             // txReady (disabled)
  0xD0, 0x08, 0x00, 0x00, // mode: 8 data bits, no parity, 1 stop bit
  0x00, 0xC2, 0x01, 0x00, // baudRate = 115200
  0x07, 0x00,             // inProtoMask: UBX + NMEA + RTCM
  0x03, 0x00,             // outProtoMask: UBX + NMEA
  0x00, 0x00,             // flags
  0x00, 0x00,             // reserved
  0xC0, 0x7E              // Checksum (CK_A, CK_B)
};

// Send a PROGMEM byte array to the GPS port
static void sendUBX(const byte *cmd, size_t len) {
  for (size_t i = 0; i < len; i++) {
    gpsPort.write(pgm_read_byte(cmd + i));
  }
}

//----------------------------------------------------------------
//  Print a NeoGPS degE7 (int32 scaled by 1e7) as a decimal degree string
static void printDegE7(Print &outs, int32_t degE7) {
  if (degE7 < 0) {
    degE7 = -degE7;
    outs.print('-');
  }

  int32_t deg = degE7 / 10000000L;
  outs.print(deg);
  outs.print('.');

  degE7 -= deg * 10000000L;

  int32_t factor = 1000000L;
  while ((degE7 < factor) && (factor > 1L)) {
    outs.print('0');
    factor /= 10L;
  }
  outs.print(degE7);
}

//----------------------------------------------------------------
// Recording control
//----------------------------------------------------------------
static void openLogFile();  // forward declaration

static void startRecording() {
  isRecording = true;
  startRecord = millis();
  sdErrorCount = 0;
  openLogFile();
  if (!logFile) {
    DEBUG_PORT.println(F("ERR: SD open failed, recording aborted"));
    isRecording = false;
    return;
  }
  DEBUG_PORT.println(F("INF: Recording started"));
}

static void stopRecording() {
  isRecording = false;
  if (logFile) {
    logFile.close();
    DEBUG_PORT.println(F("INF: Log file closed"));
  }
  DEBUG_PORT.println(F("INF: Recording stopped"));
}

//----------------------------------------------------------------
// GPS processing
//----------------------------------------------------------------
static char filenameBuf[16] = "CLOG";
static char datebuf[24];
static bool firstGPSFix = false;

static void processGPSFix(const gps_fix &fix) {
  if (fix.valid.location) {
    if (!firstGPSFix) {
      digitalWrite(buttonLedPin, HIGH);
      firstGPSFix = true;
    }
    lastGPS = millis();

    data.lat   = fix.latitudeL();
    data.lon   = fix.longitudeL();
    data.speed = fix.speed_mph();
    data.alt   = fix.altitude_ft();
    data.dir   = fix.heading();
    if (fix.valid.satellites)
      data.satellites = fix.satellites;

    int localHour = (fix.dateTime.hours + UTC_OFFSET + 24) % 24;

    sprintf(datebuf, "%02d/%02d/%02d %02d:%02d:%02d ",
            fix.dateTime.date, fix.dateTime.month, fix.dateTime.year,
            localHour, fix.dateTime.minutes, fix.dateTime.seconds);
    // Filename: DDHHMM — day(2), hour(2), minute(2) = 6 chars
    // With "_N.csv" suffix: "DDHHMM_N.csv" = 8.3 FAT compliant
    // Adding minutes eliminates same-hour filename collisions
    sprintf(filenameBuf, "%02d%02d%02d",
            fix.dateTime.date, localHour, fix.dateTime.minutes);

#ifdef GPS_DEBUG
    DEBUG_PORT.print(F("GPS Time: "));
    DEBUG_PORT.print(datebuf);
    DEBUG_PORT.print(',');
    printDegE7(DEBUG_PORT, fix.latitudeL());
    DEBUG_PORT.print(',');
    printDegE7(DEBUG_PORT, fix.longitudeL());
    DEBUG_PORT.print(',');
    if (fix.valid.satellites)
      DEBUG_PORT.print(fix.satellites);
    DEBUG_PORT.print(',');
    DEBUG_PORT.print(fix.speed_mph(), 6);
    DEBUG_PORT.print(F(" mph"));
    DEBUG_PORT.println();
#endif
  } else {
    // No valid location — rate-limit to once per 5 seconds
    static unsigned long lastNoFix = 0;
    if (millis() - lastNoFix > NOFIX_MSG_MS) {
      DEBUG_PORT.println(F("WRN: GPS waiting for fix..."));
      lastNoFix = millis();
    }
  }
}

static void readGPS() {
#ifdef TIMING_DEBUG
  long tm0 = millis();
#endif

  while (gps.available(gpsPort))
    processGPSFix(gps.read());

#ifdef TIMING_DEBUG
  DEBUG_PORT.print(F("T readGPS Start: "));
  DEBUG_PORT.print(tm0);
  DEBUG_PORT.print(F(" Dur: "));
  DEBUG_PORT.println(millis() - tm0);
#endif
}

//----------------------------------------------------------------
// IMU — MPU9250 burst read (accel + temp + gyro) + magnetometer
//----------------------------------------------------------------
static void readMPU9250() {
#ifdef TIMING_DEBUG
  long tm0 = millis();
#endif

  if (!mpu9250Ready) return;

  // Burst read: accel(6) + temp(2) + gyro(6) = 14 bytes from 0x3B
  // Registers are contiguous: ACCEL_XOUT_H(0x3B) through GYRO_ZOUT_L(0x48)
  uint8_t buf[14];
  Wire.beginTransmission(MPU9250_SLAVE_ADDRESS);
  Wire.write(MPU9250_ACCEL_XOUT_H);
  Wire.endTransmission(false);  // repeated start
  Wire.requestFrom((uint8_t)MPU9250_SLAVE_ADDRESS, (uint8_t)14);
  for (uint8_t i = 0; i < 14 && Wire.available(); i++) {
    buf[i] = Wire.read();
  }

  // Accelerometer (bytes 0-5) — 2g full scale: raw / 16384.0 = g
  int16_t rawAx = ((int16_t)buf[0]  << 8) | buf[1];
  int16_t rawAy = ((int16_t)buf[2]  << 8) | buf[3];
  int16_t rawAz = ((int16_t)buf[4]  << 8) | buf[5];
  data.accx = (float)rawAx / 16384.0;
  data.accy = (float)rawAy / 16384.0;
  data.accz = (float)rawAz / 16384.0;

  // Temperature (bytes 6-7) — degrees Celsius
  int16_t rawTemp = ((int16_t)buf[6] << 8) | buf[7];
  data.imuTemp = (float)rawTemp / 333.87 + 21.0;

  // Gyroscope (bytes 8-13) — 250dps full scale: raw / 131.0 = deg/s
  int16_t rawGx = ((int16_t)buf[8]  << 8) | buf[9];
  int16_t rawGy = ((int16_t)buf[10] << 8) | buf[11];
  int16_t rawGz = ((int16_t)buf[12] << 8) | buf[13];
  data.rotx = (float)rawGx / 131.0;
  data.roty = (float)rawGy / 131.0;
  data.rotz = (float)rawGz / 131.0;

  // Magnetometer — separate AK8963 I2C device (0x0C)
  // Uses library call for data-ready check, overflow detection,
  // and factory calibration coefficient application
  mpu9250.readMagnetXYZ(&data.magx, &data.magy, &data.magz);

#ifdef TIMING_DEBUG
  DEBUG_PORT.print(F("T mpu9250 Start: "));
  DEBUG_PORT.print(tm0);
  DEBUG_PORT.print(F(" Dur: "));
  DEBUG_PORT.println(millis() - tm0);
#endif
}

//----------------------------------------------------------------
// ISP2 — Innovate Serial Protocol 2
//----------------------------------------------------------------

// Process a complete ISP2 data packet into the SensorData struct.
// Walks word-by-word detecting LC-1 (bit 14 set) vs aux (bits 15-14 = 00).
// LC-1 sub-packets are 2 words: header + lambda.
// Aux sub-packets are 1 word: 10-bit sensor value.
// Channel order follows the physical ISP2 daisy-chain:
//   SSI-4#1: aux0=coolant, aux1=oilp
//   LC-1#1:  AFR bank 1
//   LC-1#2:  AFR bank 2
//   SSI-4#2: aux2=MAP, aux3=VSS (vehicle speed, frequency mode)
static void processISP2Data() {
  uint8_t auxIdx = 0;
  uint8_t lc1Idx = 0;
  float auxV[8];
  float afrVal[4];

  int w = 0;
  while (w < isp2PacketLen) {
    byte hi = isp2Data[w * 2];
    byte lo = isp2Data[w * 2 + 1];

    if (hi & ISP2_LC1_FLAG) {
      // LC-1 header word: func(bits 12-10), afrMult(bits 8,6-0)
      int func    = (hi >> 2) & 0x07;
      int afrMult = ((hi & 0x01) << 7) | (lo & 0x7F);

      // Next word is lambda
      w++;
      if (w >= isp2PacketLen) break;
      hi = isp2Data[w * 2];
      lo = isp2Data[w * 2 + 1];

      int lambda = ((hi & 0x3F) << 7) | (lo & 0x7F);

      if (lc1Idx < 4) {
        if (func <= 1) {
          // func 0: lambda valid, func 1: O2 level
          afrVal[lc1Idx] = ((float)(lambda + 500) * (float)afrMult) / 10000.0;
        } else {
          afrVal[lc1Idx] = 0.0; // calibrating/warming/error
        }
      }
      lc1Idx++;
    } else {
      // Aux sensor word: 10-bit value in bits 9-0
      int raw = ((hi & 0x07) << 7) | (lo & 0x7F);
      if (auxIdx < 8) {
        auxV[auxIdx] = (float)raw / 1023.0 * 5.0;
      }
      auxIdx++;
    }
    w++;
  }

  isp2AuxCount = auxIdx;
  isp2Lc1Count = lc1Idx;

  // Map aux channels to SensorData (daisy-chain order)
  if (auxIdx >= 1) data.coolant = AUX_COOLANT_F(auxV[0]);
  if (auxIdx >= 2) data.oilp    = AUX_OILP_PSI(auxV[1]);
  if (auxIdx >= 3) data.map     = AUX_MAP_INHG(auxV[2]);
  if (auxIdx >= 4) data.vss     = AUX_VSS_MPH(auxV[3]);

  // Map LC-1 devices to AFR
  if (lc1Idx >= 1) data.afr  = afrVal[0];
  if (lc1Idx >= 2) data.afr1 = afrVal[1];

#ifdef ISP2_DEBUG
  DEBUG_PORT.print(F("ISP2: "));
  DEBUG_PORT.print(lc1Idx); DEBUG_PORT.print(F("xLC1 "));
  DEBUG_PORT.print(auxIdx); DEBUG_PORT.print(F("xAUX | "));
  DEBUG_PORT.print(F("AFR=")); DEBUG_PORT.print(data.afr, 1);
  DEBUG_PORT.print(F(" AFR1=")); DEBUG_PORT.print(data.afr1, 1);
  DEBUG_PORT.print(F(" VSS=")); DEBUG_PORT.print(data.vss, 1);
  DEBUG_PORT.print(F("mph MAP=")); DEBUG_PORT.print(data.map, 1);
  DEBUG_PORT.print(F(" OIL=")); DEBUG_PORT.print(data.oilp, 0);
  DEBUG_PORT.print(F(" CLT=")); DEBUG_PORT.println(data.coolant, 0);
#endif
}

// Non-blocking ISP2 serial read.
// State machine: SYNC_HIGH → SYNC_LOW → READING_PAYLOAD → process.
// Reads only bytes already in the hardware serial buffer — never blocks.
static void readISP2() {
#ifdef TIMING_DEBUG
  long tm0 = millis();
#endif

  // Watchdog: if stuck mid-payload for too long, resync
  // This catches ISP2 cable disconnects or corrupted streams
  if (isp2State == ISP2_READING_PAYLOAD &&
      (millis() - isp2LastByte > ISP2_TIMEOUT_MS)) {
    isp2State = ISP2_SYNC_HIGH;
  }

  while (isp2Serial.available() > 0) {
    byte b = isp2Serial.read();
    isp2LastByte = millis();

    switch (isp2State) {
      case ISP2_SYNC_HIGH:
        if ((b & ISP2_H_SYNC_MASK) == ISP2_H_SYNC_MASK) {
          isp2Header[0] = b;
          isp2State = ISP2_SYNC_LOW;
        }
        break;

      case ISP2_SYNC_LOW:
        if ((b & ISP2_L_SYNC_MASK) == ISP2_L_SYNC_MASK) {
          // Header complete — extract packet info
          isp2Header[1] = b;
          isp2IsData = bitRead(isp2Header[0], 4);
          isp2PacketLen = ((isp2Header[0] & 0x01) << 7) | (isp2Header[1] & 0x7F);

          if (isp2PacketLen > 0 && isp2PacketLen <= ISP2_MAX_WORDS) {
            isp2BytesExpected = isp2PacketLen * 2;
            isp2BytesRead = 0;
            isp2State = ISP2_READING_PAYLOAD;
          } else {
            isp2State = ISP2_SYNC_HIGH;  // invalid length, resync
          }
        } else if ((b & ISP2_H_SYNC_MASK) == ISP2_H_SYNC_MASK) {
          // Not a valid low byte, but could be a new high sync
          isp2Header[0] = b;
          // stay in ISP2_SYNC_LOW
        } else {
          isp2State = ISP2_SYNC_HIGH;
        }
        break;

      case ISP2_READING_PAYLOAD:
        isp2Data[isp2BytesRead++] = b;
        if (isp2BytesRead >= isp2BytesExpected) {
          // Packet complete — process and reset
          if (isp2IsData) {
            processISP2Data();
          }
          isp2State = ISP2_SYNC_HIGH;
        }
        break;
    }
  }

#ifdef TIMING_DEBUG
  DEBUG_PORT.print(F("T readISP2 Start: "));
  DEBUG_PORT.print(tm0);
  DEBUG_PORT.print(F(" Dur: "));
  DEBUG_PORT.println(millis() - tm0);
#endif
}

//----------------------------------------------------------------
// SD Card / Logging
//----------------------------------------------------------------
static void setupSDCard() {
  pinMode(SD_CS_PIN, OUTPUT);
}

static unsigned long lastFlush = 0;

static void openLogFile() {
  if (!SD.begin(SD_CS_PIN)) {
    DEBUG_PORT.println(F("ERR: SD card failed or not present"));
    return;
  }

  // Build filename using char buffer instead of String
  char fname[24];
  int index = 0;
  snprintf(fname, sizeof(fname), "%s_%d.csv", filenameBuf, index);

  while (SD.exists(fname)) {
    index++;
    snprintf(fname, sizeof(fname), "%s_%d.csv", filenameBuf, index);
  }

  DEBUG_PORT.print(F("INF: Opening log "));
  DEBUG_PORT.println(fname);
  logFile = SD.open(fname, FILE_WRITE);
  if (logFile) {
    logFile.println(datebuf);
    logFile.println(F("time,lat,lon,speed,alt,dir,sats,accx,accy,accz,rotx,roty,rotz,magx,magy,magz,imuTemp,afr,afr1,vss,map,oilp,coolant,gpsStale"));
    logFile.println(F("(s),(deg),(deg),(mph),(ft),(deg),(#),(g),(g),(g),(dps),(dps),(dps),(uT),(uT),(uT),(C),(afr),(afr),(mph),(inHgVac),(psig),(F),(flag)"));
    logFile.flush();
    lastFlush = millis();
  }
}

// Write a single data row to a Print target (Serial or File)
static void printRow(Print &out, float now) {
  out.print(now, 3);          out.print(',');
  printDegE7(out, data.lat);      out.print(',');
  printDegE7(out, data.lon);      out.print(',');
  out.print(data.speed);      out.print(',');
  out.print(data.alt);        out.print(',');
  out.print(data.dir);        out.print(',');
  out.print(data.satellites);  out.print(',');
  out.print(data.accx);       out.print(',');
  out.print(data.accy);       out.print(',');
  out.print(data.accz);       out.print(',');
  out.print(data.rotx);       out.print(',');
  out.print(data.roty);       out.print(',');
  out.print(data.rotz);       out.print(',');
  out.print(data.magx);       out.print(',');
  out.print(data.magy);       out.print(',');
  out.print(data.magz);       out.print(',');
  out.print(data.imuTemp, 1); out.print(',');
  out.print(data.afr);        out.print(',');
  out.print(data.afr1);       out.print(',');
  out.print(data.vss);        out.print(',');
  out.print(data.map);        out.print(',');
  out.print(data.oilp);       out.print(',');
  out.print(data.coolant);    out.print(',');
  out.println(data.gpsStale ? 1 : 0);
}

// Compact human-readable live debug output, rate-limited to 2Hz.
// Format: single line with fixed-width fields so terminal doesn't jump.
// Shows the most important values at a glance while tuning.
static unsigned long lastLiveDebug = 0;
static void printLiveDebug(float now) {
  // 2Hz update rate — fast enough to see changes, slow enough to read
  if (millis() - lastLiveDebug < 500) return;
  lastLiveDebug = millis();

  // Time and recording indicator
  DEBUG_PORT.print(now, 1);
  DEBUG_PORT.print(isRecording ? F("s [REC] ") : F("s       "));

  // Speed and GPS quality
  char buf[10];
  dtostrf(data.speed, 5, 1, buf);
  DEBUG_PORT.print(buf);
  DEBUG_PORT.print(F("mph "));
  DEBUG_PORT.print(data.satellites);
  DEBUG_PORT.print(F("sat"));
  DEBUG_PORT.print(data.gpsStale ? F("! ") : F("  "));

  // AFR (the most watched value on a wideband)
  DEBUG_PORT.print(F("AFR "));
  dtostrf(data.afr, 4, 1, buf);
  DEBUG_PORT.print(buf);
  DEBUG_PORT.print('/');
  dtostrf(data.afr1, 4, 1, buf);
  DEBUG_PORT.print(buf);

  // VSS and MAP
  DEBUG_PORT.print(F("  "));
  dtostrf(data.vss, 5, 1, buf);
  DEBUG_PORT.print(buf);
  DEBUG_PORT.print(F("mph "));
  dtostrf(data.map, 5, 1, buf);
  DEBUG_PORT.print(buf);
  DEBUG_PORT.print(F("\"Hg"));

  // Oil pressure and coolant — the "is the engine happy" gauges
  DEBUG_PORT.print(F("  OIL"));
  dtostrf(data.oilp, 3, 0, buf);
  DEBUG_PORT.print(buf);
  DEBUG_PORT.print(F(" CLT"));
  dtostrf(data.coolant, 4, 0, buf);
  DEBUG_PORT.print(buf);

  // G-force (lateral is most interesting for driving)
  DEBUG_PORT.print(F("  G "));
  dtostrf(data.accy, 5, 2, buf);
  DEBUG_PORT.print(buf);

  DEBUG_PORT.println();
}

static void writeToLog() {
#ifdef TIMING_DEBUG
  long tm0 = millis();
#endif
  float now = (float)(millis() - startRecord) / 1000.0;

  // GPS staleness check
  if (lastGPS == 0 || (millis() - lastGPS > GPS_STALE_MS)) {
    data.gpsStale = true;
    data.speed = 0.0;  // can't confirm movement without fresh GPS
    // Keep lat/lon as last-known position for analysis
  } else {
    data.gpsStale = false;
  }

  // Serial debug output (compile-time or runtime toggle)
#ifdef SERIAL_DEBUG
  printRow(DEBUG_PORT, now);
#else
  if (liveDebug) printLiveDebug(now);
#endif

  if (isRecording) {
    if (!logFile) {
      // No file open — skip this sample (file is opened in startRecording)
      return;
    }
    printRow(logFile, now);
    // Flush every 1 second instead of every row
    if (millis() - lastFlush > FLUSH_INTERVAL) {
      logFile.flush();
      // SD library sets write error flag on failure
      if (logFile.getWriteError()) {
        sdErrorCount++;
        logFile.clearWriteError();
        DEBUG_PORT.print(F("ERR: SD write fail #"));
        DEBUG_PORT.println(sdErrorCount);
        if (sdErrorCount >= SD_MAX_ERRORS) {
          DEBUG_PORT.println(F("ERR: SD card failed, stopping recording"));
          stopRecording();
          return;
        }
      } else {
        sdErrorCount = 0;  // reset on successful flush
      }
      lastFlush = millis();
    }
  }

#ifdef TIMING_DEBUG
  DEBUG_PORT.print(F("T writeToLog Start: "));
  DEBUG_PORT.print(tm0);
  DEBUG_PORT.print(F(" Dur: "));
  DEBUG_PORT.println(millis() - tm0);
#endif
}

//----------------------------------------------------------------
// Serial command handler
//----------------------------------------------------------------
static void printStatus() {
  unsigned long uptime = millis() / 1000;
  DEBUG_PORT.println(F("--- Analog Bridge Status ---"));
  DEBUG_PORT.print(F("Uptime: ")); DEBUG_PORT.print(uptime); DEBUG_PORT.println(F("s"));
  DEBUG_PORT.print(F("Recording: ")); DEBUG_PORT.println(isRecording ? F("YES") : F("NO"));
  DEBUG_PORT.print(F("GPS fix: ")); DEBUG_PORT.println(data.gpsStale ? F("STALE") : F("OK"));
  DEBUG_PORT.print(F("GPS sats: ")); DEBUG_PORT.println(data.satellites);
  DEBUG_PORT.print(F("IMU: ")); DEBUG_PORT.println(mpu9250Ready ? F("OK") : F("FAIL"));
  DEBUG_PORT.print(F("ISP2 LC1: ")); DEBUG_PORT.print(isp2Lc1Count);
  DEBUG_PORT.print(F("  Aux: ")); DEBUG_PORT.println(isp2AuxCount);
  DEBUG_PORT.print(F("Live debug: ")); DEBUG_PORT.println(liveDebug ? F("ON") : F("OFF"));
  DEBUG_PORT.print(F("Free SRAM: ")); DEBUG_PORT.println(freeMemory());
}

// Returns approximate free SRAM (bytes between heap and stack)
static int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

static void handleSerial() {
  while (DEBUG_PORT.available() > 0) {
    char c = DEBUG_PORT.read();
    switch (c) {
      case '?':  // Print help
        DEBUG_PORT.println(F("--- Analog Bridge Commands ---"));
        DEBUG_PORT.println(F("  r  Start recording"));
        DEBUG_PORT.println(F("  s  Stop recording"));
        DEBUG_PORT.println(F("  d  Toggle live debug output"));
        DEBUG_PORT.println(F("  p  Print current sensor snapshot"));
        DEBUG_PORT.println(F("  v  Print system status"));
        DEBUG_PORT.println(F("  i  Print ISP2 diagnostics"));
        DEBUG_PORT.println(F("  g  Set GPS rate to 5Hz"));
        DEBUG_PORT.println(F("  b  Set GPS baud to 115200"));
        DEBUG_PORT.println(F("  ?  This help"));
        break;
      case 'r':  // Start recording to SD card
        startRecording();
        break;
      case 's':  // Stop recording
        stopRecording();
        break;
      case 'p':  // Print current sensor snapshot
        DEBUG_PORT.println(F("--- Sensor Snapshot ---"));
        DEBUG_PORT.print(F("GPS: "));
        printDegE7(DEBUG_PORT, data.lat); DEBUG_PORT.print(F(", "));
        printDegE7(DEBUG_PORT, data.lon);
        DEBUG_PORT.print(F("  ")); DEBUG_PORT.print(data.speed); DEBUG_PORT.print(F(" mph"));
        DEBUG_PORT.print(F("  sats=")); DEBUG_PORT.print(data.satellites);
        DEBUG_PORT.print(data.gpsStale ? F(" [STALE]") : F(""));
        DEBUG_PORT.println();
        DEBUG_PORT.print(F("IMU: acc="));
        DEBUG_PORT.print(data.accx); DEBUG_PORT.print(',');
        DEBUG_PORT.print(data.accy); DEBUG_PORT.print(',');
        DEBUG_PORT.print(data.accz);
        DEBUG_PORT.print(F("  gyro="));
        DEBUG_PORT.print(data.rotx); DEBUG_PORT.print(',');
        DEBUG_PORT.print(data.roty); DEBUG_PORT.print(',');
        DEBUG_PORT.print(data.rotz);
        DEBUG_PORT.print(F("  temp=")); DEBUG_PORT.print(data.imuTemp, 1); DEBUG_PORT.println(F("C"));
        DEBUG_PORT.print(F("MAG: "));
        DEBUG_PORT.print(data.magx); DEBUG_PORT.print(',');
        DEBUG_PORT.print(data.magy); DEBUG_PORT.print(',');
        DEBUG_PORT.print(data.magz); DEBUG_PORT.println(F(" uT"));
        DEBUG_PORT.print(F("ENG: AFR="));
        DEBUG_PORT.print(data.afr, 1); DEBUG_PORT.print('/');
        DEBUG_PORT.print(data.afr1, 1);
        DEBUG_PORT.print(F("  VSS=")); DEBUG_PORT.print(data.vss, 1); DEBUG_PORT.print(F("mph"));
        DEBUG_PORT.print(F("  MAP=")); DEBUG_PORT.print(data.map, 1);
        DEBUG_PORT.print(F("  OIL=")); DEBUG_PORT.print(data.oilp, 0);
        DEBUG_PORT.print(F("  CLT=")); DEBUG_PORT.println(data.coolant, 0);
        break;
      case 'v':  // Print system status
        printStatus();
        break;
      case 'g':  // Set GPS update rate to 5Hz (200ms)
        sendUBX(UBX_CFG_RATE_5HZ, sizeof(UBX_CFG_RATE_5HZ));
        DEBUG_PORT.println(F("INF: GPS rate set to 5Hz"));
        break;
      case 'b':  // Set GPS baud to 115200
        delay(10);
        gpsPort.begin(9600);
        delay(10);
        sendUBX(UBX_CFG_PRT_115200, sizeof(UBX_CFG_PRT_115200));
        delay(10);
        gpsPort.begin(115200);
        DEBUG_PORT.println(F("INF: GPS baud set to 115200"));
        break;
      case 'd':  // Toggle live debug output
        liveDebug = !liveDebug;
        DEBUG_PORT.print(F("INF: Live debug "));
        DEBUG_PORT.println(liveDebug ? F("ON") : F("OFF"));
        break;
      case 'i':  // Print ISP2 diagnostics
        DEBUG_PORT.print(F("ISP2 state: "));
        DEBUG_PORT.println(isp2State);
        DEBUG_PORT.print(F("LC1 devices: "));
        DEBUG_PORT.println(isp2Lc1Count);
        DEBUG_PORT.print(F("Aux channels: "));
        DEBUG_PORT.println(isp2AuxCount);
        DEBUG_PORT.print(F("AFR: ")); DEBUG_PORT.print(data.afr, 1);
        DEBUG_PORT.print(F(" AFR1: ")); DEBUG_PORT.println(data.afr1, 1);
        DEBUG_PORT.print(F("VSS: ")); DEBUG_PORT.print(data.vss, 1); DEBUG_PORT.print(F("mph"));
        DEBUG_PORT.print(F(" MAP: ")); DEBUG_PORT.print(data.map, 1);
        DEBUG_PORT.print(F(" OIL: ")); DEBUG_PORT.print(data.oilp, 0);
        DEBUG_PORT.print(F(" CLT: ")); DEBUG_PORT.println(data.coolant, 0);
        break;
    }
  }
}

//----------------------------------------------------------------
// LED indicator
// Button LED (pin 3): solid ON after GPS fix, blinks while recording
// On-board LED (pin 13): solid ON = system running
//----------------------------------------------------------------
static unsigned long lastBlink = 0;
static bool ledState = LOW;
static bool ledInitialized = false;

static void processLed() {
  if (isRecording) {
    ledInitialized = false;  // reset so we re-init on stop
    if (millis() - lastBlink > BLINK_INTERVAL) {
      ledState = !ledState;
      digitalWrite(buttonLedPin, ledState);
      lastBlink = millis();
    }
  } else {
    if (!ledInitialized) {
      digitalWrite(LED_BUILTIN, HIGH);  // on-board LED: system alive
      // buttonLedPin is managed by processGPSFix() — solid ON after first fix
      ledState = HIGH;
      ledInitialized = true;
    }
  }
}

//----------------------------------------------------------------
// Button handling
//----------------------------------------------------------------
static bool buttonReset = false;
static unsigned long lastButton = 0;

static void processButtons() {
  bool buttonState = digitalRead(buttonPin);

  if (buttonState == HIGH && !isRecording && !buttonReset) {
    DEBUG_PORT.println(F("INF: Button press — start"));
    startRecording();
    buttonReset = true;
    lastButton = millis();
  } else if (buttonState == HIGH && isRecording && !buttonReset) {
    DEBUG_PORT.println(F("INF: Button press — stop"));
    stopRecording();
    buttonReset = true;
    lastButton = millis();
    digitalWrite(buttonLedPin, HIGH);
  } else if (buttonState == LOW && buttonReset && (millis() - lastButton > DEBOUNCE_MS)) {
    buttonReset = false;
  }
}

//----------------------------------------------------------------
// Setup
//----------------------------------------------------------------
static unsigned long nextSample = 0;

void setup() {
  DEBUG_PORT.begin(115200);

  // Boot banner — immediately identifies firmware on serial connect
  DEBUG_PORT.println();
  DEBUG_PORT.println(F("========================================="));
  DEBUG_PORT.println(F("  Analog Bridge  v" FW_VERSION));
  DEBUG_PORT.println(F("  1969 Nova 454 BBC Datalogger"));
  DEBUG_PORT.print(F("  Built: ")); DEBUG_PORT.print(F(__DATE__));
  DEBUG_PORT.print(' ');           DEBUG_PORT.println(F(__TIME__));
#if defined(__AVR_ATmega2560__)
  DEBUG_PORT.println(F("  Board: Mega 2560"));
#elif defined(__AVR_ATmega1280__)
  DEBUG_PORT.println(F("  Board: Mega 1280"));
#else
  DEBUG_PORT.println(F("  Board: Uno/Other"));
#endif
  DEBUG_PORT.print(F("  Free SRAM: ")); DEBUG_PORT.print(freeMemory()); DEBUG_PORT.println(F(" bytes"));
  DEBUG_PORT.println(F("========================================="));
  DEBUG_PORT.println(F("  Type '?' for commands"));
  DEBUG_PORT.println();

  gpsPort.begin(9600);
  DEBUG_PORT.println(F("INF: GPS on " GPS_PORT_NAME " @ 9600"));

  isp2Serial.begin(19200);
  DEBUG_PORT.println(F("INF: ISP2 @ 19200"));

  setupSDCard();
  DEBUG_PORT.println(F("INF: SD card ready"));

  if (mpu9250.begin()) {
    mpu9250Ready = true;
    DEBUG_PORT.println(F("INF: MPU9250 OK"));
  } else {
    mpu9250Ready = false;
    DEBUG_PORT.println(F("ERR: MPU9250 not found, continuing without IMU"));
  }

  pinMode(buttonLedPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  DEBUG_PORT.println(F("INF: Boot complete"));
  DEBUG_PORT.println();

  nextSample = millis();
}

//----------------------------------------------------------------
// Main loop
//----------------------------------------------------------------
void loop() {
  handleSerial();
  processLed();
  processButtons();

  // Drain ISP2 serial buffer every iteration to prevent overflow.
  // At 19200 baud the 64-byte hardware buffer fills in ~267ms;
  // calling only at 80ms intervals risks drops during busy loops.
  readISP2();

  if ((long)(millis() - nextSample) >= 0) {
    nextSample += SAMPLE_INTERVAL;  // Fixed interval, no drift
    readMPU9250();
    readGPS();
    writeToLog();
  }
}
