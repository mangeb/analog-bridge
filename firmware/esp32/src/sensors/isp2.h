/**
 *  Analog Bridge — ISP2 (Innovate Motorsports) Module
 *
 *  Non-blocking state machine parser for the ISP2 serial protocol.
 *  Decodes LC-1 wideband AFR and SSI-4 aux sensor data.
 *
 *  On ESP32: runs as a high-priority FreeRTOS task driven by UART events.
 */
#ifndef AB_ISP2_H
#define AB_ISP2_H

#include "sensor_data.h"
#include <HardwareSerial.h>

// Initialize UART2 for ISP2 at 19200 baud.
void isp2Init();

// Process available bytes from the ISP2 serial buffer.
// Called from the ISP2 FreeRTOS task. Non-blocking.
void isp2Read(SensorData &data);

// Get diagnostic info
uint8_t isp2GetAuxCount();
uint8_t isp2GetLc1Count();
int     isp2GetState();

// Access the serial port (for task setup)
HardwareSerial& isp2GetSerial();

#endif // AB_ISP2_H
