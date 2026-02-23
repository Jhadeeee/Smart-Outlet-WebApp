/*
 * StatusLED.h
 * ------------
 * Non-blocking LED indicator for showing device state.
 * Uses millis()-based patterns instead of delay().
 */

#ifndef STATUS_LED_H
#define STATUS_LED_H

#include <Arduino.h>
#include "../../Config.h"

enum class LEDPattern {
    OFF,
    SOLID,          // Solid ON — connected
    SLOW_BLINK,     // 1s on / 1s off — AP mode (waiting for setup)
    FAST_BLINK,     // 200ms on / 200ms off — connecting to WiFi
    PULSE           // 3 quick blinks, pause — sending data
};

class StatusLED {
public:
    StatusLED(uint8_t pin = LED_PIN);

    // Initialize the LED pin
    void begin();

    // Set the current blink pattern
    void setPattern(LEDPattern pattern);

    // Must be called in loop() for non-blocking blink
    void update();

    // Get current pattern name (for debug)
    String getPatternName() const;

private:
    uint8_t    _pin;
    LEDPattern _pattern;
    bool       _ledState;
    unsigned long _lastToggle;
    uint8_t    _pulseCount;
};

#endif // STATUS_LED_H
