/*
 * Cloud.h
 * --------
 * Handles HTTP communication with the Django server.
 * Provides methods for sending sensor data and event logs
 * to the appropriate API endpoints.
 */

#ifndef CLOUD_H
#define CLOUD_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "Config.h"

class Cloud {
public:
    Cloud();

    // Initialize with server URL
    void begin(const String& serverUrl);

    // ─── Sensor Data ────────────────────────
    // Send current reading to /api/sensor-data/
    // Returns HTTP response code (200 = success)
    int sendSensorData(const String& deviceId, float currentMA);

    // ─── Event Logging ──────────────────────
    // Send event log to /api/event-log/
    // eventType: "overload", "cutoff", "power_on", "power_off", "warning"
    // severity: "info", "warning", "critical"
    int sendEventLog(const String& deviceId, const String& eventType,
                     const String& severity, const String& message,
                     const String& socketLabel = "", float currentMA = 0);

    // ─── Generic POST ───────────────────────
    // Send raw JSON to a specific endpoint
    int sendData(const String& endpoint, const String& jsonPayload);

    // ─── Server Status ──────────────────────
    // Check if server is reachable (GET request)
    bool isReachable();

    // Get the configured server URL
    String getServerUrl() const;

    // Get the last HTTP response code
    int getLastResponseCode() const;

    // Get the last response body
    String getLastResponse() const;

private:
    String _serverUrl;
    int    _lastResponseCode;
    String _lastResponse;
};

#endif // CLOUD_H
