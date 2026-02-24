#ifndef BREAKER_MONITOR_H
#define BREAKER_MONITOR_H

#include "SCT013.h"
#include "../../Config.h"

/**
 * BreakerMonitor — Thin wrapper around SCT013 for the main breaker panel.
 * 
 * Reads total/main load current via a non-invasive clamp-on CT sensor
 * wired directly to an ESP32 ADC pin (no HC-12 involved).
 * 
 * Uses non-blocking mode so it doesn't stall the main loop.
 */
class BreakerMonitor {
public:
    BreakerMonitor();

    /**
     * @brief Initialize the sensor. Call in setup().
     */
    void begin();

    /**
     * @brief Non-blocking update. Call in loop() every iteration.
     * @return true if a new reading is available.
     */
    bool update();

    /**
     * @brief Get the latest RMS current in Amps.
     */
    double getAmps() const;

    /**
     * @brief Get the latest RMS current in milliamps (for consistency with PIC outlets).
     */
    int getMilliAmps() const;

    /**
     * @brief Check if a valid reading has been obtained at least once.
     */
    bool hasReading() const;

    /**
     * @brief Tare (zero) the sensor — call when no load is connected.
     */
    void tare();

    /**
     * @brief Check if tare is complete.
     */
    bool isTareComplete() const;

    /**
     * @brief Get the current overload threshold in mA.
     */
    int getThreshold() const;

    /**
     * @brief Set the overload threshold in mA.
     */
    void setThreshold(int mA);

    /**
     * @brief Check if current exceeds the threshold.
     */
    bool isOverload() const;

private:
    SCT013   _sensor;
    double   _lastAmps;
    bool     _hasReading;
    int      _thresholdMA;
};

#endif
