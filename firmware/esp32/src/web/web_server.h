/**
 *  Analog Bridge — Web Server Module
 *
 *  WiFi AP + ESPAsyncWebServer + WebSocket for live monitoring.
 *  Serves gzipped dashboard HTML from PROGMEM, broadcasts sensor
 *  data as JSON over WebSocket at 5Hz.
 */
#ifndef AB_WEB_SERVER_H
#define AB_WEB_SERVER_H

#include "sensor_data.h"

// Initialize WiFi AP and start the web server.
void webInit();

// Broadcast sensor data to all connected WebSocket clients.
// Call at WS_BROADCAST_MS interval (5Hz).
void webBroadcast(const SensorData &data, bool isRecording,
                  const char* filename, unsigned long rowCount,
                  float duration, uint16_t keyframeCount);

// Get number of connected WebSocket clients.
int webGetClientCount();

// Cleanup disconnected clients (call periodically).
void webCleanup();

#endif // AB_WEB_SERVER_H
