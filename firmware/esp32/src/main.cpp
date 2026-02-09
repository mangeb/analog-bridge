/**
 *  Analog Bridge — ESP32-S3 Main Entry Point
 *
 *  1969 Chevrolet Nova, 454 BBC Datalogger
 *  Firmware v2.0.0 — ESP32-S3 with WiFi live monitoring
 *
 *  FreeRTOS dual-core architecture:
 *    Core 0: WiFi stack, WebSocket broadcast, serial commands
 *    Core 1: ISP2 drain, sensor reads (IMU+GPS), SD logging, LED/button
 *
 *  Data flow:
 *    Sensor tasks → SensorData (double-buffered) → WebSocket JSON + SD CSV
 *
 *  Repository: github.com/mangeb/analog-bridge
 */
#include <Arduino.h>
#include "config.h"
#include "sensor_data.h"

// Modules
#include "sensors/imu.h"
#include "sensors/isp2.h"
#include "sensors/gps.h"
#include "logging/sd_logger.h"
#include "ui/serial_cmd.h"
#include "ui/led.h"
#include "web/web_server.h"

//----------------------------------------------------------------
// Double-buffered SensorData for cross-core sharing
// Core 1 writes to backBuf, then swaps pointer.
// Core 0 reads from frontBuf. Spinlock protects only the swap.
//----------------------------------------------------------------
static SensorData bufA = {};
static SensorData bufB = {};
static volatile SensorData* frontBuf = &bufA;  // Core 0 reads this
static SensorData* backBuf = &bufB;             // Core 1 writes this
static portMUX_TYPE bufMux = portMUX_INITIALIZER_UNLOCKED;

// Swap buffers atomically (called by Core 1 after writing)
static void swapBuffers() {
  portENTER_CRITICAL(&bufMux);
  volatile SensorData* tmp = frontBuf;
  frontBuf = backBuf;
  backBuf = (SensorData*)tmp;
  portEXIT_CRITICAL(&bufMux);
}

// Get snapshot of current sensor data (called by Core 0)
static SensorData getSnapshot() {
  SensorData snap;
  portENTER_CRITICAL(&bufMux);
  snap = *(const SensorData*)frontBuf;
  portEXIT_CRITICAL(&bufMux);
  return snap;
}

//----------------------------------------------------------------
// Recording state (shared between cores via atomic/mutex)
//----------------------------------------------------------------
static volatile bool isRecording = false;
static volatile unsigned long startRecord = 0;
static volatile uint16_t keyframeCount = 0;
static volatile bool keyframePending = false;

//----------------------------------------------------------------
// Recording control functions (called from button/serial callbacks)
//----------------------------------------------------------------
static void startRecording() {
  if (isRecording) {
    Serial.println("INF: Already recording");
    return;
  }

  const char* base = gpsGetFilenameBase();
  const char* date = gpsGetDateString();

  if (!sdOpenLogFile(base, date)) {
    Serial.println("ERR: SD open failed, recording aborted");
    return;
  }

  isRecording = true;
  startRecord = millis();
  keyframeCount = 0;
  keyframePending = false;

  Serial.printf("INF: Recording -> %s\n", sdGetFilename());
}

static void stopRecording() {
  if (!isRecording) {
    Serial.println("INF: Not recording");
    return;
  }

  unsigned long duration = millis() - startRecord;
  isRecording = false;
  sdCloseLogFile();

  unsigned long sec = duration / 1000;
  Serial.printf("INF: Stopped — %lum %lus, %lu rows",
    sec / 60, sec % 60, sdGetRowCount());
  if (keyframeCount > 0) {
    Serial.printf(", %d keyframes", keyframeCount);
  }
  Serial.printf(" -> %s\n", sdGetFilename());
}

static void insertKeyframe() {
  if (!isRecording) return;
  keyframeCount++;
  keyframePending = true;
  Serial.printf("INF: Keyframe #%d\n", keyframeCount);
}

//----------------------------------------------------------------
// FreeRTOS Task: ISP2 Reader (Core 1, highest priority)
// Drains ISP2 UART buffer. Runs in tight loop with 1ms yield.
//----------------------------------------------------------------
static void taskISP2(void *pvParameters) {
  Serial.println("INF: taskISP2 started on core " + String(xPortGetCoreID()));
  for (;;) {
    isp2Read(*backBuf);
    vTaskDelay(pdMS_TO_TICKS(1));  // Yield briefly
  }
}

//----------------------------------------------------------------
// FreeRTOS Task: Sensor Read + GPS (Core 1, 12.5Hz)
// Reads IMU, processes GPS, updates backBuf, then swaps.
//----------------------------------------------------------------
static void taskSensors(void *pvParameters) {
  Serial.println("INF: taskSensors started on core " + String(xPortGetCoreID()));
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SAMPLE_INTERVAL));

    // Read sensors into back buffer
    imuRead(*backBuf);
    gpsRead(*backBuf);

    // GPS staleness check
    unsigned long lastFix = gpsGetLastFixTime();
    if (lastFix == 0 || (millis() - lastFix > GPS_STALE_MS)) {
      backBuf->gpsStale = true;
      backBuf->speed = 0.0f;
    } else {
      backBuf->gpsStale = false;
    }

    // GPS fix LED
    if (gpsHasFix()) {
      digitalWrite(BUTTON_LED_PIN, HIGH);
    }

    // Swap to make data available to Core 0
    swapBuffers();
  }
}

//----------------------------------------------------------------
// FreeRTOS Task: SD Card Logger (Core 1, 12.5Hz)
// Writes CSV rows from the front buffer.
//----------------------------------------------------------------
static void taskSDLog(void *pvParameters) {
  Serial.println("INF: taskSDLog started on core " + String(xPortGetCoreID()));
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(SAMPLE_INTERVAL));

    if (isRecording) {
      SensorData snap = getSnapshot();
      float elapsed = (float)(millis() - startRecord) / 1000.0f;
      bool kfPending = keyframePending;
      if (kfPending) keyframePending = false;

      if (!sdWriteRow(snap, elapsed, kfPending, keyframeCount)) {
        // SD error threshold exceeded
        stopRecording();
      }
    }
  }
}

//----------------------------------------------------------------
// FreeRTOS Task: WebSocket Broadcast (Core 0, 5Hz)
//----------------------------------------------------------------
static void taskWebSocket(void *pvParameters) {
  Serial.println("INF: taskWebSocket started on core " + String(xPortGetCoreID()));
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(WS_BROADCAST_MS));

    SensorData snap = getSnapshot();
    float duration = isRecording ? (float)(millis() - startRecord) / 1000.0f : 0.0f;
    webBroadcast(snap, isRecording, sdGetFilename(), sdGetRowCount(),
                 duration, keyframeCount);
    webCleanup();
  }
}

//----------------------------------------------------------------
// FreeRTOS Task: Serial Commands (Core 0, 100ms poll)
//----------------------------------------------------------------
static void taskSerialCmd(void *pvParameters) {
  Serial.println("INF: taskSerialCmd started on core " + String(xPortGetCoreID()));
  for (;;) {
    SensorData snap = getSnapshot();
    serialCmdProcess(snap, isRecording);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

//----------------------------------------------------------------
// FreeRTOS Task: LED + Button (Core 1, 100ms poll)
//----------------------------------------------------------------
static void taskLED(void *pvParameters) {
  Serial.println("INF: taskLED started on core " + String(xPortGetCoreID()));
  for (;;) {
    ledProcess(isRecording, gpsHasFix());
    ledProcessButtons(isRecording);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

//----------------------------------------------------------------
// Arduino setup() — runs on Core 1
//----------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);  // Let USB-CDC enumerate

  // Boot banner
  Serial.println();
  Serial.println("=========================================");
  Serial.println("  Analog Bridge  v" FW_VERSION);
  Serial.println("  1969 Nova 454 BBC Datalogger");
  Serial.println("  ESP32-S3 — Dual Core + WiFi");
  Serial.printf("  Built: %s %s\n", __DATE__, __TIME__);
  Serial.printf("  Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("=========================================");
  Serial.println("  Type '?' for commands");
  Serial.println();

  // Initialize subsystems
  gpsInit();
  isp2Init();
  sdInit();
  imuInit();    // Includes NVS cal load + gyro auto-zero (~2.5s)
  ledInit();
  webInit();

  // Set up callbacks
  serialCmdInit(startRecording, stopRecording, insertKeyframe);
  ledSetCallbacks(startRecording, stopRecording, insertKeyframe);

  Serial.println("INF: Boot complete");
  Serial.printf("INF: Free heap after init: %d bytes\n", ESP.getFreeHeap());
  Serial.println();

  // Launch FreeRTOS tasks
  // Core 1: time-critical sensor tasks
  xTaskCreatePinnedToCore(taskISP2,      "ISP2",    TASK_ISP2_STACK,
    NULL, TASK_ISP2_PRIORITY,    NULL, TASK_ISP2_CORE);
  xTaskCreatePinnedToCore(taskSensors,   "Sensors", TASK_SENSORS_STACK,
    NULL, TASK_SENSORS_PRIORITY, NULL, TASK_SENSORS_CORE);
  xTaskCreatePinnedToCore(taskSDLog,     "SDLog",   TASK_SDLOG_STACK,
    NULL, TASK_SDLOG_PRIORITY,   NULL, TASK_SDLOG_CORE);
  xTaskCreatePinnedToCore(taskLED,       "LED",     TASK_LED_STACK,
    NULL, TASK_LED_PRIORITY,     NULL, TASK_LED_CORE);

  // Core 0: WiFi + UI tasks
  xTaskCreatePinnedToCore(taskWebSocket, "WS",      TASK_WS_STACK,
    NULL, TASK_WS_PRIORITY,      NULL, TASK_WS_CORE);
  xTaskCreatePinnedToCore(taskSerialCmd, "Serial",  TASK_SERIAL_STACK,
    NULL, TASK_SERIAL_PRIORITY,  NULL, TASK_SERIAL_CORE);

  Serial.println("INF: All tasks launched");
}

//----------------------------------------------------------------
// Arduino loop() — empty, all work done in FreeRTOS tasks
//----------------------------------------------------------------
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));  // Idle task, yield to FreeRTOS
}
