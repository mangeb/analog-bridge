/**
 *  Analog Bridge — LED and Button Implementation
 *
 *  Ported from AVR analog-bridge.ino lines 1309-1392.
 *  Changes from AVR:
 *    - Pin numbers from config.h
 *    - Callback pattern for button events (decoupled from recording logic)
 *    - No F() macros
 */
#include "led.h"
#include "config.h"
#include <Arduino.h>

static ButtonStartCallback    cbStart    = nullptr;
static ButtonStopCallback     cbStop     = nullptr;
static ButtonKeyframeCallback cbKeyframe = nullptr;

// LED state
static unsigned long lastBlink = 0;
static bool ledState = LOW;
static bool ledInitialized = false;

// Button state
static bool     buttonDown    = false;
static unsigned long buttonDownAt = 0;
static unsigned long lastRelease  = 0;

//----------------------------------------------------------------
// Public API
//----------------------------------------------------------------

void ledInit() {
  pinMode(BUTTON_LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_BUILTIN_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN_PIN, HIGH);  // system alive
}

void ledSetCallbacks(ButtonStartCallback onStart,
                     ButtonStopCallback onStop,
                     ButtonKeyframeCallback onKeyframe) {
  cbStart    = onStart;
  cbStop     = onStop;
  cbKeyframe = onKeyframe;
}

void ledProcess(bool isRecording, bool hasFix) {
  if (isRecording) {
    ledInitialized = false;
    if (millis() - lastBlink > BLINK_INTERVAL) {
      ledState = !ledState;
      digitalWrite(BUTTON_LED_PIN, ledState);
      lastBlink = millis();
    }
  } else {
    if (!ledInitialized) {
      if (hasFix) {
        digitalWrite(BUTTON_LED_PIN, HIGH);
      }
      ledState = HIGH;
      ledInitialized = true;
    }
  }
}

void ledProcessButtons(bool isRecording) {
  bool pressed = (digitalRead(BUTTON_PIN) == HIGH);

  // Rising edge
  if (pressed && !buttonDown) {
    if (millis() - lastRelease < DEBOUNCE_MS) return;
    buttonDown = true;
    buttonDownAt = millis();
  }

  // Falling edge
  if (!pressed && buttonDown) {
    buttonDown = false;
    lastRelease = millis();
    unsigned long held = millis() - buttonDownAt;

    if (held >= KEYFRAME_HOLD_MS && isRecording) {
      if (cbKeyframe) cbKeyframe();
      ledBlinkKeyframeConfirm();
    } else if (held < KEYFRAME_HOLD_MS) {
      if (!isRecording) {
        if (cbStart) cbStart();
      } else {
        if (cbStop) cbStop();
        digitalWrite(BUTTON_LED_PIN, HIGH);
      }
    }
  }
}

void ledBlinkKeyframeConfirm() {
  for (uint8_t i = 0; i < 3; i++) {
    digitalWrite(BUTTON_LED_PIN, LOW);
    delay(60);
    digitalWrite(BUTTON_LED_PIN, HIGH);
    delay(60);
  }
}
