/*
 * StatusLED.cpp
 * --------------
 * Implementation of non-blocking LED status patterns.
 */

#include "StatusLED.h"

StatusLED::StatusLED(uint8_t pin)
    : _pin(pin),
      _pattern(LEDPattern::OFF),
      _ledState(false),
      _lastToggle(0),
      _pulseCount(0) {}

void StatusLED::begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    Serial.println("[StatusLED] Initialized on pin " + String(_pin));
}

void StatusLED::setPattern(LEDPattern pattern) {
    if (_pattern != pattern) {
        _pattern = pattern;
        _pulseCount = 0;
        _lastToggle = millis();
        Serial.println("[StatusLED] Pattern → " + getPatternName());
    }
}

void StatusLED::update() {
    unsigned long now = millis();
    unsigned long interval = 0;

    switch (_pattern) {
        case LEDPattern::OFF:
            digitalWrite(_pin, LOW);
            _ledState = false;
            return;

        case LEDPattern::SOLID:
            digitalWrite(_pin, HIGH);
            _ledState = true;
            return;

        case LEDPattern::SLOW_BLINK:
            interval = 1000;  // 1 second per toggle
            break;

        case LEDPattern::FAST_BLINK:
            interval = 150;   // 150ms per toggle
            break;

        case LEDPattern::PULSE:
            // 3 quick blinks then pause
            if (_pulseCount < 6) {  // 3 blinks = 6 toggles
                interval = 120;
            } else {
                interval = 800;     // Pause
                if (now - _lastToggle >= interval) {
                    _pulseCount = 0;
                    _lastToggle = now;
                    digitalWrite(_pin, LOW);
                    _ledState = false;
                }
                return;
            }
            break;
    }

    // Toggle LED at the specified interval
    if (now - _lastToggle >= interval) {
        _ledState = !_ledState;
        digitalWrite(_pin, _ledState ? HIGH : LOW);
        _lastToggle = now;

        if (_pattern == LEDPattern::PULSE) {
            _pulseCount++;
        }
    }
}

String StatusLED::getPatternName() const {
    switch (_pattern) {
        case LEDPattern::OFF:         return "OFF";
        case LEDPattern::SOLID:       return "SOLID";
        case LEDPattern::SLOW_BLINK:  return "SLOW_BLINK (AP Mode)";
        case LEDPattern::FAST_BLINK:  return "FAST_BLINK (Connecting)";
        case LEDPattern::PULSE:       return "PULSE (Sending Data)";
        default:                      return "UNKNOWN";
    }
}
