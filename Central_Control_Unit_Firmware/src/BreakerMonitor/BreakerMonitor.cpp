#include "BreakerMonitor.h"

BreakerMonitor::BreakerMonitor()
    : _sensor(BREAKER_ADC_PIN),   // Auto-config: ESP32 → 3.3V, 12-bit
      _lastAmps(0.0),
      _hasReading(false),
      _thresholdMA(BREAKER_DEFAULT_THRESHOLD_MA)
{
}

void BreakerMonitor::begin() {
    _sensor.begin(BREAKER_CT_TURNS, BREAKER_BURDEN_RESISTOR);
    _sensor.setFrequency(BREAKER_LINE_FREQ);
    Serial.println("[BreakerMonitor] Initialized on GPIO " + String(BREAKER_ADC_PIN));
    Serial.println("[BreakerMonitor] CT=" + String(BREAKER_CT_TURNS) + 
                   " Burden=" + String(BREAKER_BURDEN_RESISTOR) + 
                   "Ω  Freq=" + String(BREAKER_LINE_FREQ) + "Hz");
}

bool BreakerMonitor::update() {
    if (_sensor.update()) {
        _lastAmps = _sensor.getLastAmps();
        _hasReading = true;
        return true;
    }
    return false;
}

double BreakerMonitor::getAmps() const {
    return _lastAmps;
}

int BreakerMonitor::getMilliAmps() const {
    return (int)(_lastAmps * 1000.0);
}

bool BreakerMonitor::hasReading() const {
    return _hasReading;
}

void BreakerMonitor::tare() {
    _sensor.tareNoDelay();
}

bool BreakerMonitor::isTareComplete() const {
    // Cast away const — getTareStatus() doesn't modify state but isn't marked const
    return const_cast<SCT013&>(_sensor).getTareStatus();
}

int BreakerMonitor::getThreshold() const {
    return _thresholdMA;
}

void BreakerMonitor::setThreshold(int mA) {
    _thresholdMA = mA;
}

bool BreakerMonitor::isOverload() const {
    if (!_hasReading) return false;
    if (_thresholdMA <= 0) return false;
    return getMilliAmps() > _thresholdMA;
}
