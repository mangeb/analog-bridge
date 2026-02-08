/**
 *  Analog Bridge - Datalogger for 1969 Chevrolet Nova 454 BBC
 *  Evolved from CarDuino v1.2
 */
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

// ISP2 Serial - Innovate Motorsports
//----------------------------------------------------------------
// Mega: Use hardware Serial2 (RX=17, TX=16) for better buffering
// Uno:  Use AltSoftSerial (RX=8, TX=9, claims PWM on pin 10)
#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
  #define isp2Serial Serial2
#else
  #include <AltSoftSerial.h>
  AltSoftSerial isp2Serial;
#endif

/* SD card attached to SPI bus as follows:
* Mega: MOSI-51, MISO-50, CLK-52, CS-53
* Uno:  MOSI-11, MISO-12, CLK-13, CS-10
*/

// 9 Axis 
//----------------------------------------------------------------
FaBo9Axis mpu9250;

// GPS
//----------------------------------------------------------------
static NMEAGPS  gps; // This parses the GPS characters



// Debug
//#define TIMING_DEBUG 1 // 140 bytes SRAM, 750 bytes FLASH
//#define GPS_DEBUG 1

// Logging File variables
File logFile;

// Timestamps from last readings and start of recording
unsigned long lastGPS;
bool isRecording = false;
unsigned long startRecord = 0;

// Data Values
unsigned long mTime = 0;
long mlat, mlon;
float mSpeed, mAlt, mDir, mAccx, mAccy, mAccz, mRotx, mRoty, mRotz;
float mAfr, mAfr1, mRpm, mMap, mOilp, mCoolant;
bool mpu9250Ready = false;

// ISP2 Protocol
//----------------------------------------------------------------
//#define ISP2_DEBUG 1
#define ISP2_H_SYNC_MASK  0xA2  // High sync byte: bits 7,5,1 set
#define ISP2_L_SYNC_MASK  0x80  // Low sync byte: bit 7 set
#define ISP2_LC1_FLAG     0x40  // Bit 14 in high byte: LC-1 sub-packet

#define ISP2_MAX_WORDS    16
static byte    isp2Header[2];
static byte    isp2Data[ISP2_MAX_WORDS * 2];
static bool    isp2HeaderFound = false;
static int     isp2PacketLen   = 0;
static bool    isp2IsData      = false;
static bool    isp2Ready       = false;
static uint8_t isp2AuxCount    = 0;  // aux channels detected last packet
static uint8_t isp2Lc1Count    = 0;  // LC-1 devices detected last packet

// Aux channel voltage-to-unit conversions (tune for your sensors)
#define AUX_COOLANT_F(v)   ((v) * 100.0)          // placeholder: needs thermistor table
#define AUX_OILP_PSI(v)    (((v) - 0.5) * 25.0)   // 0.5-4.5V = 0-100 PSI sender
#define AUX_MAP_INHG(v)    ((v) * 5.858 - 14.696)  // 1-bar MAP: 0V=full vac, 5V=~14.7psi
#define AUX_RPM(v)         ((v) * 2000.0)          // placeholder: depends on SSI-4 config

const int buttonPin = 2;
const int buttonLedPin = 3;

//----------------------------------------------------------------
//  Print the 32-bit integer degrees *as if* they were high-precision floats
static void printL( Print & outs, int32_t degE7 );
static void printL( Print & outs, int32_t degE7 )
{
  // Extract and print negative sign
  if (degE7 < 0) {
    degE7 = -degE7;
    outs.print( '-' );
  }

  // Whole degrees
  int32_t deg = degE7 / 10000000L;
  outs.print( deg );
  outs.print( '.' );

  // Get fractional degrees
  degE7 -= deg*10000000L;

  // Print leading zeroes, if needed
  int32_t factor = 1000000L;
  while ((degE7 < factor) && (factor > 1L)){
    outs.print( '0' );
    factor /= 10L;
  }
  
  // Print fractional degrees
  outs.print( degE7 );
}

static void startRecording() {
  isRecording = true;
  startRecord = millis();
  DEBUG_PORT.println(F("Started Recording"));
}

static void stopRecording() {
  isRecording = false;
  if(logFile) {
    logFile.close();
    DEBUG_PORT.println(F("Log file closed"));
  }
  DEBUG_PORT.println(F("Stopped Recording"));
}

char filenameBuf[16] = "CLOG";
char datebuf[20];
int firstGPSFix = 0;
static void processGPSFix( const gps_fix & fix );
static void processGPSFix( const gps_fix & fix ) {
  if (fix.valid.location) {
    //DEBUG_PORT.println(F("GPS INF: Valid Fix"));
    if(firstGPSFix == 0) {
        digitalWrite(buttonLedPin, HIGH); 
        firstGPSFix = 1;
    }
    lastGPS = millis();
    
    mlat = fix.latitudeL();
    mlon = fix.longitudeL(); 
    mSpeed = fix.speed_mph(); 
    mAlt = fix.altitude_ft();  
    mDir = fix.heading();
    
    int timezone = -7; // SF (UTC -0800) > NMEA is UTC oriented
    int localHour = (fix.dateTime.hours + timezone + 24) % 24;

    sprintf(datebuf, "%02d/%02d/%02d %02d:%02d:%02d ",  fix.dateTime.date, fix.dateTime.month, fix.dateTime.year, localHour, fix.dateTime.minutes, fix.dateTime.seconds);
    sprintf(filenameBuf, "%02d%02d%02d",  fix.dateTime.month, fix.dateTime.date, localHour); 

#ifdef GPS_DEBUG
    DEBUG_PORT.print(F("GPS Time: "));
    DEBUG_PORT.print(datebuf); 
    DEBUG_PORT.print( ',' );

    // DEBUG_PORT.print( fix.latitude(), 6 ); // floating-point display
    // DEBUG_PORT.print( fix.latitudeL() ); // integer display
    printL( DEBUG_PORT, fix.latitudeL() ); // prints int like a float

    DEBUG_PORT.print( ',' );
    // DEBUG_PORT.print( fix.longitude(), 6 ); // floating-point display
    // DEBUG_PORT.print( fix.longitudeL() );  // integer display
    printL( DEBUG_PORT, fix.longitudeL() ); // prints int like a float

    DEBUG_PORT.print( ',' );
    if (fix.valid.satellites)
      DEBUG_PORT.print( fix.satellites );

    DEBUG_PORT.print( ',' );
    DEBUG_PORT.print( fix.speed_mph(), 6 );
    DEBUG_PORT.print( F(" mph") );
    DEBUG_PORT.println();
#endif
  } 
  else {
    // No valid location data yet!
    DEBUG_PORT.println(F("?"));
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

static void readmpu9250() {
#ifdef TIMING_DEBUG
  long tm0 = millis();
#endif

  if(!mpu9250Ready) return;
  mpu9250.readAccelXYZ(&mAccx, &mAccy, &mAccz);
  mpu9250.readGyroXYZ(&mRotx,&mRoty,&mRotz);
  //mpu9250.readMagnetXYZ(&mx,&my,&mz);
  //mpu9250.readTemperature(&temp);
  /*
  float Pi = 3.14159;
  // Calculate the angle of the vector y,x
  float heading = (atan2(event.magnetic.y,event.magnetic.x) * 180) / Pi;
 
  // Normalize to 0-360
  if (heading < 0)
  {
    heading = 360 + heading;
  }
  */
#ifdef TIMING_DEBUG
   DEBUG_PORT.print(F("T mpu9250 Start: "));
   DEBUG_PORT.print(tm0);
   DEBUG_PORT.print(F(" Dur: "));
   DEBUG_PORT.println(millis() - tm0);
#endif
}

// Process a complete ISP2 data packet into global variables.
// Walks word-by-word detecting LC-1 (bit 14 set) vs aux (bits 15-14 = 00).
// LC-1 sub-packets are 2 words: header + lambda.
// Aux sub-packets are 1 word: 10-bit sensor value.
// Channel order follows the physical ISP2 daisy-chain:
//   SSI-4#1: aux0=coolant, aux1=oilp
//   LC-1#1:  AFR bank 1
//   LC-1#2:  AFR bank 2
//   SSI-4#2: aux2=MAP, aux3=RPM
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
    }
    else {
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

  // Map aux channels → global variables (daisy-chain order)
  if (auxIdx >= 1) mCoolant = AUX_COOLANT_F(auxV[0]);
  if (auxIdx >= 2) mOilp    = AUX_OILP_PSI(auxV[1]);
  if (auxIdx >= 3) mMap     = AUX_MAP_INHG(auxV[2]);
  if (auxIdx >= 4) mRpm     = AUX_RPM(auxV[3]);

  // Map LC-1 devices → AFR variables
  if (lc1Idx >= 1) mAfr  = afrVal[0];
  if (lc1Idx >= 2) mAfr1 = afrVal[1];

#ifdef ISP2_DEBUG
  DEBUG_PORT.print(F("ISP2: "));
  DEBUG_PORT.print(lc1Idx); DEBUG_PORT.print(F("xLC1 "));
  DEBUG_PORT.print(auxIdx); DEBUG_PORT.print(F("xAUX | "));
  DEBUG_PORT.print(F("AFR=")); DEBUG_PORT.print(mAfr, 1);
  DEBUG_PORT.print(F(" AFR1=")); DEBUG_PORT.print(mAfr1, 1);
  DEBUG_PORT.print(F(" RPM=")); DEBUG_PORT.print(mRpm, 0);
  DEBUG_PORT.print(F(" MAP=")); DEBUG_PORT.print(mMap, 1);
  DEBUG_PORT.print(F(" OIL=")); DEBUG_PORT.print(mOilp, 0);
  DEBUG_PORT.print(F(" CLT=")); DEBUG_PORT.println(mCoolant, 0);
#endif
}

// Read and sync ISP2 serial stream.
// Header sync: find high byte matching 0xA2 mask, then low byte matching 0x80.
// Once synced, bulk-read payload with readBytes() and flag for processing.
static void readISP2() {
#ifdef TIMING_DEBUG
  long tm0 = millis();
#endif

  while (isp2Serial.available() > 0) {
    byte b = isp2Serial.read();

    if (!isp2HeaderFound && ((b & ISP2_H_SYNC_MASK) == ISP2_H_SYNC_MASK)) {
      // Potential header high byte
      isp2Header[0] = b;
      isp2HeaderFound = true;
    }
    else if (isp2HeaderFound && ((b & ISP2_L_SYNC_MASK) == ISP2_L_SYNC_MASK)) {
      // Header complete
      isp2Header[1] = b;
      isp2IsData = bitRead(isp2Header[0], 4);
      isp2PacketLen = ((isp2Header[0] & 0x01) << 7) | (isp2Header[1] & 0x7F);

      if (isp2PacketLen > 0 && isp2PacketLen <= ISP2_MAX_WORDS) {
        int got = isp2Serial.readBytes(isp2Data, isp2PacketLen * 2);
        if (got == isp2PacketLen * 2) {
          isp2Ready = true;
        }
      }
      isp2HeaderFound = false;
    }
    else if (isp2HeaderFound) {
      // Not a valid low sync — maybe a new high sync?
      if ((b & ISP2_H_SYNC_MASK) == ISP2_H_SYNC_MASK) {
        isp2Header[0] = b;
      } else {
        isp2HeaderFound = false;
      }
    }
  }

  // Process completed packet
  if (isp2Ready) {
    if (isp2IsData) {
      processISP2Data();
    }
    isp2Ready = false;
  }

#ifdef TIMING_DEBUG
   DEBUG_PORT.print(F("T readISP2 Start: "));
   DEBUG_PORT.print(tm0);
   DEBUG_PORT.print(F(" Dur: "));
   DEBUG_PORT.println(millis() - tm0);
#endif
}

#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
  const int SD_CS_PIN = 53;
#else
  const int SD_CS_PIN = 10;
#endif
static void setupSDCard() {
  pinMode(SD_CS_PIN, OUTPUT);
}

bool sdcardReady = false;
static unsigned long lastFlush = 0;
static void openLogFile() {
  sdcardReady = false;
  if (!SD.begin(SD_CS_PIN)) {
    DEBUG_PORT.println(F("SDCARD ERR: Card failed, or not present, recording will not work"));
    stopRecording();
    return;
  }
  sdcardReady = true;

  // Build filename using char buffer instead of String
  char fname[24];
  int index = 0;
  snprintf(fname, sizeof(fname), "%s_%d.csv", filenameBuf, index);

  while(SD.exists(fname)) {
    index++;
    snprintf(fname, sizeof(fname), "%s_%d.csv", filenameBuf, index);
  }

  DEBUG_PORT.print(F("SDCARD INF: Trying to open "));
  DEBUG_PORT.println(fname);
  logFile = SD.open(fname, FILE_WRITE);
  if (logFile) {
    logFile.println(datebuf);
    logFile.println(F("time,lat,lon,speed,alt,dir,accx,accy,accz,rotx,roty,rotz,afr,afr1,rpm,map,oilp,coolant"));
    logFile.println(F("(s),(deg),(deg),(mph),(ft),(deg),(g),(g),(g),(deg/s),(deg/s),(deg/s),(afr),(afr),(rpm),(inHgVac),(psig),(f)"));
    logFile.flush();
    lastFlush = millis();
  }
}

// Helper: write a single data row to a Print target (Serial or File)
static void printRow(Print &out, float now) {
  out.print(now, 3);
  out.print(',');
  printL(out, mlat);
  out.print(',');
  printL(out, mlon);
  out.print(',');
  out.print(mSpeed);
  out.print(',');
  out.print(mAlt);
  out.print(',');
  out.print(mDir);
  out.print(',');
  out.print(mAccx);
  out.print(',');
  out.print(mAccy);
  out.print(',');
  out.print(mAccz);
  out.print(',');
  out.print(mRotx);
  out.print(',');
  out.print(mRoty);
  out.print(',');
  out.print(mRotz);
  out.print(',');
  out.print(mAfr);
  out.print(',');
  out.print(mAfr1);
  out.print(',');
  out.print(mRpm);
  out.print(',');
  out.print(mMap);
  out.print(',');
  out.print(mOilp);
  out.print(',');
  out.println(mCoolant);
}

static void writeToLog() {
#ifdef TIMING_DEBUG
  long tm0 = millis();
#endif
  float now = (float)(millis() - startRecord)/1000.0;

  // Always print to serial debug
  printRow(DEBUG_PORT, now);

  if(isRecording) {
    if(!logFile) {
      DEBUG_PORT.println(F("LOGGING ERR: No log file open, trying to open"));
      openLogFile();
    }
    else {
      printRow(logFile, now);
      // Flush every 1 second instead of every row
      if(millis() - lastFlush > 1000) {
        logFile.flush();
        lastFlush = millis();
      }
    }
  }

#ifdef TIMING_DEBUG
   DEBUG_PORT.print(F("T writeToLog Start: "));
   DEBUG_PORT.print(tm0);
   DEBUG_PORT.print(F(" Dur: "));
   DEBUG_PORT.println(millis() - tm0);
#endif
}

void handleSerial() {
  while (DEBUG_PORT.available() > 0) {
    char c = DEBUG_PORT.read();
    switch (c) {
      case 'r':
        startRecording();
        break;
      case 's':
        stopRecording();
        break;
      case '2':
        {
          byte cfg_rate200ms[] = {0xb5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xc8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xde, 0x6a};
          gpsPort.write(cfg_rate200ms, sizeof(cfg_rate200ms));
        }
        break;
      case 'B':
        {
          byte cfg_baud115200[] = {0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00, 0x00, 0xC2, 0x01, 0x00, 0x07, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x7E};
          delay(10);
          gpsPort.begin(9600);
          delay(10);
          gpsPort.write(cfg_baud115200, sizeof(cfg_baud115200));
          delay(10);
          gpsPort.begin(115200);
        }
        break;
    }
  }
}

unsigned long lastBlink = 0;
bool ledState = LOW;
void processLed() {
  if(isRecording) {
    if(millis() - lastBlink > 1000) {
      digitalWrite(buttonLedPin, ledState);
      ledState = !ledState;
      lastBlink = millis();
    }
  }
  else {
    ledState = HIGH;
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

bool buttonReset = false;
unsigned long lastButton = 0;
void processButtons() {
  bool buttonState = digitalRead(buttonPin);

  if(buttonState == HIGH && !isRecording && !buttonReset) {
    DEBUG_PORT.println(F("START RECORDING PRESS"));
    startRecording();
    buttonReset = true;
    lastButton = millis();
  }
  else if(buttonState == HIGH && isRecording && !buttonReset) {
    DEBUG_PORT.println(F("STOP RECORDING PRESS"));
    stopRecording();
    buttonReset = true;
    lastButton = millis();
    digitalWrite(buttonLedPin, HIGH);
  }
  else if(buttonState == LOW && buttonReset && (millis() - lastButton > 500)) {
    buttonReset = false;
  }
}


unsigned long nextSample = 0;
void setup () {
  DEBUG_PORT.begin(115200);
  DEBUG_PORT.println(F("INF: Debug Port - Configured"));

  //gpsPort.begin(115200);
  gpsPort.begin(9600);
  DEBUG_PORT.println(F("INF: GPS Port - Configured"));

  isp2Serial.begin(19200);
  isp2Serial.setTimeout(20);  // Don't block >20ms on readBytes
  DEBUG_PORT.println(F("INF: ISP2 Port - Configured (19200 baud)"));

  setupSDCard();
  DEBUG_PORT.println(F("INF: SDCard - Configured"));
  
  // TODO only when recording start should we check and open file?
  
  if (mpu9250.begin()) {
    mpu9250Ready = true;
    DEBUG_PORT.println(F("INF: 9Axis - Configured"));
  } else {
    mpu9250Ready = false;
    DEBUG_PORT.println(F("ERR: 9Axis - Device error, continuing without IMU"));
  }
  
  pinMode(buttonLedPin, OUTPUT);
  //digitalWrite(ledRecodingPin, HIGH); // Turn on when GPS fix is valid
  
  pinMode(buttonPin, INPUT);
  
  nextSample = millis();
}

void loop () {
  handleSerial();
  processLed();
  processButtons();

  if(millis() >= nextSample) {
    nextSample += 80;  // Fixed 80ms interval, no drift
    readISP2();
    readmpu9250();
    readGPS();
    writeToLog();
  }
}
