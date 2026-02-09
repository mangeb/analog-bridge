/**
 *  Analog Bridge — Serial Command Handler
 *
 *  Single-character commands over USB-CDC serial (115200 baud).
 *  Same command set as AVR plus 'w' for WiFi status.
 */
#ifndef AB_SERIAL_CMD_H
#define AB_SERIAL_CMD_H

#include "sensor_data.h"

// Forward declarations for callbacks
typedef void (*CmdStartCallback)();
typedef void (*CmdStopCallback)();
typedef void (*CmdKeyframeCallback)();

// Initialize serial command handler with callbacks.
void serialCmdInit(CmdStartCallback onStart,
                   CmdStopCallback onStop,
                   CmdKeyframeCallback onKeyframe);

// Process available serial input. Non-blocking.
void serialCmdProcess(const SensorData &data, bool isRecording);

// Print system status (for 'v' command, also used by WebSocket)
void serialCmdPrintStatus(const SensorData &data, bool isRecording);

#endif // AB_SERIAL_CMD_H
