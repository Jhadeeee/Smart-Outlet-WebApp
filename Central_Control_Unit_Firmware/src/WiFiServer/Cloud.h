/*
 * Cloud.h
 * --------
 * Handles HTTP communication with the remote server.
 * Sends sensor data via POST and polls for commands via GET.
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

    // Send JSON data to server via POST /api/data/
    int sendData(const String& jsonPayload);

    // Send JSON data to a custom endpoint (e.g., /api/breaker-data/)
    int sendToEndpoint(const String& endpoint, const String& jsonPayload);

    // Poll for pending commands via GET /api/commands/<deviceId>/
    // Returns the raw JSON response body (caller parses it)
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
