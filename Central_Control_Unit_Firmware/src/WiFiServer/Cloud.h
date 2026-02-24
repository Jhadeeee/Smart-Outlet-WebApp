/*
 * Cloud.h
 * --------
 * Handles HTTP communication with the remote server.
 */

#ifndef CLOUD_H
#define CLOUD_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "../../Config.h"

class Cloud {
public:
    Cloud();

    // Initialize with server URL
    void begin(const String& serverUrl);

    // Send JSON data to server via POST (Outlet Sensors)
    int sendSensorData(const String& jsonPayload);

    // Send JSON data to server via POST (Main Breaker)
    int sendBreakerData(const String& jsonPayload);

    // Fetch pending commands for a specific device (GET request)
    String fetchCommands(const String& deviceId);

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
