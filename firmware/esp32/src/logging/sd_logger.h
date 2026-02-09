/**
 *  Analog Bridge — SD Card Logger Module
 *
 *  CSV logging to SD card with periodic flush and error recovery.
 *  Same 25-column format as AVR for analysis tool compatibility.
 */
#ifndef AB_SD_LOGGER_H
#define AB_SD_LOGGER_H

#include "sensor_data.h"

// Initialize SPI and SD card hardware.
void sdInit();

// Open a new log file. Returns true if successful.
bool sdOpenLogFile(const char* filenameBase, const char* dateStr);

// Write one CSV row. Handles flush timing and error recovery.
// Returns false if recording should be stopped (SD_MAX_ERRORS exceeded).
bool sdWriteRow(const SensorData &data, float elapsedSec,
                bool keyframePending, uint16_t keyframeCount);

// Close the current log file and flush.
void sdCloseLogFile();

// Get current log info
const char* sdGetFilename();
unsigned long sdGetRowCount();

#endif // AB_SD_LOGGER_H
