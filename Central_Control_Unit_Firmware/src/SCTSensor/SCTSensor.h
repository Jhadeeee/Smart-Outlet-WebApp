/*
 * SCTSensor.h
 * ------------
 * Reads the SCT-013 current transformer (100A:50mA) via ESP32 ADC.
 *
 * The SCT is clamped on the main breaker wire and outputs a small
 * AC current proportional to the load. A burden resistor converts
 * this to a voltage centered around a DC bias point (~1.65V).
 *
 * This class samples the ADC over one AC cycle, computes the RMS
 * value, and converts it to milliamps using the calibration factor.
 */

#ifndef SCT_SENSOR_H
#define SCT_SENSOR_H

#include <Arduino.h>
#include "../../Config.h"

class SCTSensor {
public:
    SCTSensor();

    // Initialize ADC pin and resolution
    void begin(uint8_t pin = SCT_ADC_PIN);

    // Read RMS current in milliamps (blocking — takes ~20ms for 1 cycle)
    int readCurrentRMS();

    // Get last reading without re-sampling
    int getLastReading() const;

private:
    uint8_t _pin;
    int     _lastReadingMA;
};

#endif // SCT_SENSOR_H
