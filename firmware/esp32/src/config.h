/**
 *  Analog Bridge — ESP32-S3 Configuration
 *
 *  All pin assignments, timing constants, and WiFi settings in one place.
 *  Change pins here when using a different ESP32-S3 devkit variant.
 *
 *  Avoid: GPIO35-37 (PSRAM on WROOM), GPIO19-20 (USB D-/D+)
 */
#ifndef AB_CONFIG_H
#define AB_CONFIG_H

#include "calibration_data.h"
#include "isp2_defs.h"

//----------------------------------------------------------------
// Firmware version
//----------------------------------------------------------------
#define FW_VERSION "2.0.0"

//----------------------------------------------------------------
// WiFi — Access Point mode
//----------------------------------------------------------------
#define WIFI_AP_SSID_PREFIX  "AnalogBridge"  // Full SSID: AnalogBridge-XXXX
#define WIFI_AP_PASSWORD     "nova454"       // Set empty "" for open network
#define WIFI_AP_CHANNEL      1
#define WIFI_AP_MAX_CLIENTS  3

//----------------------------------------------------------------
// UART Pin Assignments
// USB-CDC is used for debug (Serial), freeing all 3 hardware UARTs
//----------------------------------------------------------------

// GPS (u-blox) on UART1
#define GPS_UART_NUM     1
#define GPS_TX_PIN       17
#define GPS_RX_PIN       18
#define GPS_BAUD_INIT    9600
#define GPS_BAUD_FAST    115200

// ISP2 (Innovate Motorsports) on UART2
#define ISP2_UART_NUM    2
#define ISP2_TX_PIN      15    // Assigned but unused (ISP2 is receive-only)
#define ISP2_RX_PIN      16

//----------------------------------------------------------------
// I2C Pin Assignments (MPU9250)
//----------------------------------------------------------------
#define I2C_SDA_PIN      8
#define I2C_SCL_PIN      9
#define I2C_CLOCK_HZ     400000   // 400kHz fast mode

//----------------------------------------------------------------
// SPI Pin Assignments (SD Card) — using VSPI / SPI2
//----------------------------------------------------------------
#define SD_MOSI_PIN      11
#define SD_MISO_PIN      13
#define SD_CLK_PIN       12
#define SD_CS_PIN        10

//----------------------------------------------------------------
// Digital I/O
//----------------------------------------------------------------
#define BUTTON_PIN       4     // Momentary button (active HIGH, external pull-down)
#define BUTTON_LED_PIN   5     // LED indicator: solid=GPS fix, blink=recording
#define LED_BUILTIN_PIN  2     // On-board LED (varies by devkit; may be Neopixel on GPIO48)

//----------------------------------------------------------------
// Timing Constants
//----------------------------------------------------------------
#define UTC_OFFSET       -7      // PDT (UTC-7). Change to -8 for PST.
#define GPS_STALE_MS     2000    // Mark GPS stale after this many ms without fix
#define FLUSH_INTERVAL   1000    // SD card flush interval (ms)
#define BLINK_INTERVAL   1000    // Recording LED blink rate (ms)
#define DEBOUNCE_MS      200     // Button debounce time (ms)
#define KEYFRAME_HOLD_MS 1000    // Hold button this long for keyframe (ms)
#define NOFIX_MSG_MS     5000    // GPS no-fix message rate limit (ms)
#define SAMPLE_INTERVAL  80      // Main loop sample period (ms) = 12.5 Hz
#define SD_MAX_ERRORS    3       // Auto-stop recording after this many consecutive SD errors
#define WS_BROADCAST_MS  200     // WebSocket broadcast interval (ms) = 5 Hz

//----------------------------------------------------------------
// FreeRTOS Task Configuration
//----------------------------------------------------------------
#define TASK_ISP2_STACK      4096
#define TASK_ISP2_PRIORITY   5      // Highest user priority (drain UART)
#define TASK_ISP2_CORE       1

#define TASK_SENSORS_STACK   4096
#define TASK_SENSORS_PRIORITY 3
#define TASK_SENSORS_CORE    1

#define TASK_SDLOG_STACK     8192   // Larger for SD library buffers
#define TASK_SDLOG_PRIORITY  2
#define TASK_SDLOG_CORE      1

#define TASK_WS_STACK        8192   // JSON serialization + WebSocket
#define TASK_WS_PRIORITY     2
#define TASK_WS_CORE         0

#define TASK_SERIAL_STACK    4096
#define TASK_SERIAL_PRIORITY 1
#define TASK_SERIAL_CORE     0

#define TASK_LED_STACK       2048
#define TASK_LED_PRIORITY    1
#define TASK_LED_CORE        1

//----------------------------------------------------------------
// Debug flags (uncomment to enable at compile time)
//----------------------------------------------------------------
//#define TIMING_DEBUG 1
//#define GPS_DEBUG 1
//#define ISP2_DEBUG 1
//#define SERIAL_DEBUG 1    // Print every data row to Serial (high bandwidth)

// Runtime debug toggle (controlled by 'd' serial command)
#define LIVE_DEBUG_DEFAULT  true

#endif // AB_CONFIG_H
