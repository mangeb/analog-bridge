 /** 
 *  Carduino TaskScheduler Test
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
#include <NeoSWSerial.h>

/* SD card attached to SPI bus as follows:
* MOSI - pin 11
* MISO - pin 12
* CLK - pin 13
* CS - pin 10 Uno (53 on Mega)
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

// Innovate Serial Protocol 2
// 8 data bits, 1 stop bit, no parity, 19200 Baud
static int ISP2_INT = 80; // 81.92 ms = 12.20703125 ~ 12.1 hz

// Logging File variables
File logFile;
String logData = "";

// Timestamps from last readings and start of recording
unsigned long lastGPS;
unsigned long startRecord = -1;

// Data Values
unsigned long mTime = 0;
long mlat, mlon;
float mSpeed, mAlt, mDir, mAccx, mAccy, mAccz, mRotx, mRoty, mRotz;
float mAfr, mAfr1, mRpm, mMap, mOilp, mCoolant;

// TODO change to data[]; and use enums for access
static const String DELIM = ","; 

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
  startRecord = millis();
  DEBUG_PORT.println(F("Started Recording"));
}

static void stopRecording() {
  if(logFile) {
    logFile.close();
    DEBUG_PORT.println(F("Log file closed"));
  }
  startRecord = -1;
  DEBUG_PORT.println(F("Stopped Recording"));
}

char filenameBuf[8] = "carduino";
char datebuf[20];
int firstGPSFix = 0;
static void processGPSFix( const gps_fix & fix );
static void processGPSFix( const gps_fix & fix ) {
  if (fix.valid.location) {
    //DEBUG_PORT.println(F("GPS INF: Valid Fix"));
    if(firstGPSFix == 0) {
        digitalWrite(buttonLedPin, HIGH); 
        firstGPSFix == 1;
    }
    lastGPS = millis();
    
    mlat = fix.latitudeL();
    mlon = fix.longitudeL(); 
    mSpeed = fix.speed_mph(); 
    mAlt = fix.altitude_ft();  
    mDir = fix.heading();
    
    int timezone = -7; // SF (UTC -0800) > NMEA is UTC oriented
    
    sprintf(datebuf, "%02d/%02d/%02d %02d:%02d:%02d ",  fix.dateTime.date, fix.dateTime.month, fix.dateTime.year, fix.dateTime.hours+timezone, fix.dateTime.minutes, fix.dateTime.seconds); 
    sprintf(filenameBuf, "%02d%02d%02d",  fix.dateTime.month, fix.dateTime.date,fix.dateTime.hours+timezone); 

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

  mpu9250.readAccelXYZ(&mAccx, &mAccy, &mAccz);
  mpu9250.readGyroXYZ(&mRotx,&mRoty,&mRotx);
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

static void readISP2() {
#ifdef TIMING_DEBUG
  long tm0 = millis();
#endif

  mAfr = random(3)+11.0;
  mAfr1 = random(3)+11.0;
  mRpm = random(4000)+950; 
  mMap = random(12)-15; 
  mOilp = random(40)+20;
  mCoolant = random(23)+180;
  
#ifdef TIMING_DEBUG
   DEBUG_PORT.print(F("T readISP2 Start: "));
   DEBUG_PORT.print(tm0);
   DEBUG_PORT.print(F(" Dur: "));
   DEBUG_PORT.println(millis() - tm0);
#endif
}

const int SD_CS_PIN = 10;
static void setupSDCard() {
  pinMode(SD_CS_PIN, OUTPUT);
}

boolean sdcardReady;
static void openLogFile() {
  sdcardReady = false; 
  if (!SD.begin(SD_CS_PIN)) {
    DEBUG_PORT.println(F("SDCARD ERR: Card failed, or not present, recording will not work"));
    stopRecording();
    return; 
    // TODO use boolean for ready ? now it just tries until there is a card then starts logging, neat or ?
  }
  sdcardReady = true;
  
  int index = 0;
  String fname = "";
  fname = filenameBuf;
  fname += "_";
  fname += String(index);
  fname += ".csv";
  
  while(SD.exists(fname)) {
    index++;
    fname = filenameBuf;
    fname += "_";
    fname += String(index);
    fname += ".csv";
  }

  DEBUG_PORT.print(F("SDCARD INF: Trying to open "));
  DEBUG_PORT.println(fname);
  logFile = SD.open(fname, FILE_WRITE);
  if (logFile) {
    logFile.println(datebuf);
    logFile.println(F("time,lat,lon,speed,alt,dir,accx,accy,accz,rotx,roty,rotz,afr,afr1,rpm,map,oilp,coolant"));
    logFile.println(F("(s),(deg),(deg),(mph),(ft),(deg),(g),(g),(g),(deg/s),(deg/s),(deg/s),(afr),(afr),(rpm),(inHgVac),(psig),(f)")); 
    logFile.flush();  
  } 
}

static void writeToLog() {
#ifdef TIMING_DEBUG
  long tm0 = millis();
#endif 
  float now = (float)(millis() - startRecord)/1000.0;
  // GPS
  DEBUG_PORT.print(now, 3);
  DEBUG_PORT.print(DELIM);
  printL(DEBUG_PORT, mlat);
  DEBUG_PORT.print(DELIM);
  printL(DEBUG_PORT, mlon);

  logData += DELIM;
  logData += mSpeed;
  logData += DELIM;
  logData += mAlt;
  logData += DELIM;
  logData += mDir;
  logData += DELIM;

  // 9 Axis Acc
  logData += mAccx;
  logData += DELIM;
  logData += mAccy;
  logData += DELIM;
  logData += mAccz;
  logData += DELIM;

  // 9 Axis Rot
  logData += mRotx;
  logData += DELIM;
  logData += mRoty;
  logData += DELIM;
  logData += mRotz;
  logData += DELIM;

  // Innovate 
  logData += mAfr;
  logData += DELIM;
  logData += mAfr1;
  logData += DELIM;

  logData += mRpm;
  logData += DELIM;
  logData += mMap;
  logData += DELIM;

  logData += mOilp;
  logData += DELIM;
  logData += mCoolant;
  logData += "\r\n";

  DEBUG_PORT.print(logData);

  if(startRecord != -1) {
    if(!logFile) {
      DEBUG_PORT.println(F("LOGGING ERR: No log file open, trying to open"));
      openLogFile();
    }
    else {
      // TODO doesn't print the gps data
      logFile.print(now, 3);
      logFile.print(DELIM);
      printL(logFile, mlat);
      logFile.print(DELIM);
      printL(logFile, mlon);
      logFile.print(logData);
      logFile.flush();
    }
  }
  
  logData = "";
  
#ifdef TIMING_DEBUG
   DEBUG_PORT.print(F("T writeToLog Start: "));
   DEBUG_PORT.print(tm0);
   DEBUG_PORT.print(F(" Dur: "));
   DEBUG_PORT.println(millis() - tm0);
#endif
}

void updateRTC() {
 #ifdef TIMING_DEBUG
  long tm0 = millis();
  DEBUG_PORT.println(F("t5: updateRTC"));
#endif 
#ifdef TIMING_DEBUG
   DEBUG_PORT.print(F(" t5: Start: "));
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
        byte cfg_rate200ms[] = {0xb5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xc8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xde, 0x6a};
        gpsPort.write(cfg_rate200ms, sizeof(cfg_rate200ms)); 
        break;
      case 'B':
        byte cfg_baud115200[] = {0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00, 0x00, 0xC2, 0x01, 0x00, 0x07, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x7E};
        delay(10);
        gpsPort.begin(9600);
        delay(10);
        gpsPort.write(cfg_baud115200, sizeof(cfg_baud115200));
        delay(10);
        gpsPort.begin(115200); 
        break;
    }
  }
}

//TODO: Is it blinking?
long lastBlink;
boolean ledState = LOW;
void processLed() {
  if(startRecord != -1) {
    if(millis() - lastBlink > 1000) {
      DEBUG_PORT.println(F("BLINK"));
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
 
boolean buttonState = LOW;
boolean buttonReset = LOW;
long lastButton;
void processButtons() { 
 buttonState = digitalRead(buttonPin);
 
 if(buttonState == HIGH && startRecord == -1 && buttonReset == LOW) {
      DEBUG_PORT.println(F("START RECORDING PRESS"));
      startRecording();
      buttonReset = HIGH;
      lastButton = millis();
  }
 else if(buttonState == HIGH && startRecord != -1 && buttonReset == LOW) {
      DEBUG_PORT.println(F("STOP RECORDING PRESS"));
      stopRecording();
      buttonReset = HIGH;
      digitalWrite(buttonLedPin, HIGH);
  }
  else if(buttonState == LOW && buttonReset == HIGH && (millis() - lastBlink > 1000)) {
     buttonReset = LOW;
  }
}


long i;
void setup () {
  DEBUG_PORT.begin(115200);
  DEBUG_PORT.println(F("INF: Debug Port - Configured"));

  //gpsPort.begin(115200);
  gpsPort.begin(9600);
  DEBUG_PORT.println(F("INF: GPS Port - Configured"));

  setupSDCard();
  DEBUG_PORT.println(F("INF: SDCard - Configured"));
  
  // TODO only when recording start should we check and open file?
  
  if (mpu9250.begin()) {
    DEBUG_PORT.println(F("INF: 9Axis - Configured"));
  } else {
    DEBUG_PORT.println(F("ERR: 9Axis - Device error"));
    while(1);
  }
  
  pinMode(buttonLedPin, OUTPUT);
  //digitalWrite(ledRecodingPin, HIGH); // Turn on when GPS fix is valid
  
  pinMode(buttonPin, INPUT);
  
  i = millis();
}

void loop () {
  handleSerial();
  processLed();
  processButtons();
  
  if(millis() - i > 80) {
    i = millis();
    readISP2();
    readmpu9250();
    readGPS();
    writeToLog();
  }

  // TODO measure time from from start to finish and adjust next delay
}
