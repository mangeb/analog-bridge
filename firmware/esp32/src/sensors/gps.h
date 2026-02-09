/**
 *  Analog Bridge — GPS (u-blox) Module
 *
 *  Uses TinyGPS++ (replaces NeoGPS from AVR — better ESP32 compatibility).
 *  Auto-configures u-blox to 115200 baud + 5Hz at boot via UBX commands.
 */
#ifndef AB_GPS_H
#define AB_GPS_H

#include "sensor_data.h"

// Initialize UART1 for GPS, send UBX config (9600→115200, 5Hz).
void gpsInit();

// Read available NMEA sentences and update SensorData.
// Non-blocking — parses whatever bytes are in the serial buffer.
void gpsRead(SensorData &data);

// Reconfigure GPS (after u-blox power cycle without rebooting ESP32).
void gpsReconfigure();

// Returns true after first valid fix
bool gpsHasFix();

// Get the filename base built from GPS time (DDHHMM format)
const char* gpsGetFilenameBase();

// Get formatted date string
const char* gpsGetDateString();

// Get millis() of last valid fix
unsigned long gpsGetLastFixTime();

#endif // AB_GPS_H
