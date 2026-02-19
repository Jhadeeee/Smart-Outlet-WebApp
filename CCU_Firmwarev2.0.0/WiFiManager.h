/*
 * WiFiManager.h
 * --------------
 * Manages ESP32 WiFi modes: Access Point (AP) for setup
 * and Station (STA) for normal operation.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "Config.h"

enum class WiFiState {
    IDLE,
    AP_MODE,
    CONNECTING,
    CONNECTED,
    DISCONNECTED
};

class WiFiManager {
public:
    WiFiManager();

    // Start Access Point mode (hotspot for setup)
    void startAP(const char* ssid = AP_SSID, const char* password = AP_PASSWORD);

    // Stop Access Point
    void stopAP();

    // Connect to a WiFi network in Station mode
    bool connectToWiFi(const String& ssid, const String& password, unsigned long timeoutMs = WIFI_CONNECT_TIMEOUT_MS);

    // Disconnect from WiFi
    void disconnect();

    // Check connection status
    bool isConnected();

    // Get current IP address (works in both AP and STA mode)
    IPAddress getLocalIP();

    // Get current WiFi state
    WiFiState getState() const;

    // Get state as human-readable string
    String getStateString() const;

private:
    WiFiState _state;
};

#endif // WIFI_MANAGER_H
