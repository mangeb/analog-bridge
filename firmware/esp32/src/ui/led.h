/**
 *  Analog Bridge — LED and Button Module
 *
 *  Button: short press (<1s) = start/stop recording
 *          long press  (>1s) = keyframe marker (triple-blink confirms)
 *  LED:    solid after GPS fix, blinks while recording
 */
#ifndef AB_LED_H
#define AB_LED_H

#include <stdint.h>

// Callback function types for button actions
typedef void (*ButtonStartCallback)();
typedef void (*ButtonStopCallback)();
typedef void (*ButtonKeyframeCallback)();

// Initialize button and LED pins.
void ledInit();

// Set callback functions for button events.
void ledSetCallbacks(ButtonStartCallback onStart,
                     ButtonStopCallback onStop,
                     ButtonKeyframeCallback onKeyframe);

// Process LED blink state. Call periodically (~100ms).
void ledProcess(bool isRecording, bool hasFix);

// Process button input. Call periodically (~100ms).
void ledProcessButtons(bool isRecording);

// Triple-blink to confirm keyframe.
void ledBlinkKeyframeConfirm();

#endif // AB_LED_H
