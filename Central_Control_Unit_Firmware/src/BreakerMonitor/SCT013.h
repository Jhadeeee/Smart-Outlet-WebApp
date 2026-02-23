#ifndef SCT013_H
#define SCT013_H

#include "Arduino.h"

class SCT013 {
public:
    /**
     * @brief Constructor
     * @param pin The analog input pin.
     * @param voltageReference System logic voltage (e.g., 5.0 for Arduino Uno, 3.3 for ESP32).
     * @param adcResolution ADC bit resolution (e.g., 10 for Arduino, 12 for ESP32).
     */
    /**
     * @brief Constructor (Manual Configuration)
     * @param pin The analog input pin.
     * @param voltageReference System logic voltage (e.g., 5.0 or 3.3).
     * @param adcResolution ADC bit resolution (e.g., 10 or 12).
     */
    SCT013(int pin, float voltageReference, int adcResolution);

    /**
     * @brief Constructor (Auto Configuration)
     * Automatically detects if running on ESP32 or AVR (Arduino) and sets defaults.
     * ESP32: 3.3V, 12-bit
     * AVR: 5.0V, 10-bit
     * @param pin The analog input pin.
     */
    SCT013(int pin);

    /**
     * @brief Initialize the library and set calibration.
     * @param turnsRatio Number of turns (default 2000 for standard SCT013).
     * @param burdenResistor Burden resistor value in Ohms (default 18.0).
     */
    void begin(float turnsRatio = 2000.0, float burdenResistor = 18.0);

    /**
     * @brief Set calibration parameters manually.
     * Calibration Factor = TurnsRatio / BurdenResistor.
     * @param turnsRatio The number of turns in the CT (e.g., 2000 for SCT013-000).
     * @param burdenResistor The value of the burden resistor in Ohms.
     */
    void setCalibration(float turnsRatio, float burdenResistor);

    /**
     * @brief Configure the line frequency (50Hz or 60Hz) for RMS timing.
     * @param hz Frequency in Hertz (default 50).
     */
    void setFrequency(int hz);
    
    // Calibration Helpers
    double getCalibrationFactor(); 
    void setCalibrationFactor(double factor);

    // Tare / Zeroing (Non-blocking)
    void tareNoDelay();
    bool getTareStatus();

    // Utility
    double smooth(double newVal, double oldVal, double weight);

    /**
     * @brief Read the RMS current.
     * Use setFrequency() to adjust timing (samples for 10 line cycles).
     * @return RMS Current in Amperes.
     */
    double readAmps();

    /**
     * @brief Read the RMS current (Manual Mode).
     * @param samples Number of samples to take.
     * @return RMS Current in Amperes.
     */
    double readAmps(int samples);

    /**
     * @brief Get the internal DC offset value (Digital Low Pass Filter output).
     * Useful for checking if the hardware bias is correct (should be ~1/2 ADC max).
     * @return The filtered DC offset value.
     */
    float getDCoffset();

    /**
     * @brief Non-blocking update function. Call this in your loop() as fast as possible.
     * @return true if a new RMS calculation is ready, false otherwise.
     */
    bool update();

    /**
     * @brief Get the last calculated RMS current from the non-blocking update.
     * @return RMS Current in Amperes.
     */
    double getLastAmps();

private:
    int _pin;
    float _voltageReference;
    int _adcResolution;
    double _calibration;
    int _frequency;
    
    // Internal variables for Digital Low Pass Filter
    double _offsetI;
    double _filteredI;

    // Non-blocking state
    double _sumI;
    int _sampleCount;
    unsigned long _startTime;
    double _lastAmps;

    // Tare
    int _tareCount;
};

#endif
