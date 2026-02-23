/*
 * SCTSensor.cpp
 * --------------
 * Implementation of the SCT-013 current sensor reader.
 *
 * How it works:
 *   1. The SCT-013 (100A:50mA) outputs AC current proportional to load.
 *   2. A burden resistor (e.g., 33Ω) converts current to voltage.
 *   3. A voltage divider biases the signal to ~1.65V (mid-rail for 3.3V).
 *   4. The ESP32 ADC reads the biased AC waveform.
 *   5. We sample many points, subtract the DC offset, compute RMS,
 *      and scale by the calibration factor to get real current in mA.
 */

#include "SCTSensor.h"

SCTSensor::SCTSensor()
    : _pin(SCT_ADC_PIN),
      _lastReadingMA(0) {}

void SCTSensor::begin(uint8_t pin) {
    _pin = pin;
    analogReadResolution(12);       // 12-bit: 0–4095
    analogSetAttenuation(ADC_11db); // Full 0–3.3V range
    pinMode(_pin, INPUT);

    Serial.println("[SCTSensor] Initialized on GPIO " + String(_pin));
    Serial.println("[SCTSensor] Calibration: " + String(SCT_CALIBRATION, 1) +
                   " | Samples: " + String(SCT_SAMPLES));
}

int SCTSensor::readCurrentRMS() {
    double sumSquared = 0.0;

    for (int i = 0; i < SCT_SAMPLES; i++) {
        int raw = analogRead(_pin);

        // Subtract DC bias (midpoint) to get the AC component
        double centered = (double)raw - SCT_ADC_MIDPOINT;

        // Accumulate squared values for RMS
        sumSquared += centered * centered;
    }

    // RMS of the ADC readings
    double meanSquared = sumSquared / SCT_SAMPLES;
    double rmsADC = sqrt(meanSquared);

    // Convert ADC value to voltage, then to current using calibration
    // Voltage = (rmsADC / ADC_RESOLUTION) * VREF
    // Current_mA = Voltage * calibration * 1000
    double rmsVoltage = (rmsADC / SCT_ADC_RESOLUTION) * SCT_VREF;
    double rmsCurrentA = rmsVoltage * SCT_CALIBRATION;
    int rmsCurrentMA = (int)(rmsCurrentA * 1000.0);

    // Noise floor: readings below ~50mA are likely noise
    if (rmsCurrentMA < 50) {
        rmsCurrentMA = 0;
    }

    _lastReadingMA = rmsCurrentMA;
    return _lastReadingMA;
}

int SCTSensor::getLastReading() const {
    return _lastReadingMA;
}
