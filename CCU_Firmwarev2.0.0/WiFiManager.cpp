/*
 * WiFiManager.cpp
 * ----------------
 * Implementation of WiFi AP/STA mode management.
 */

#include "WiFiManager.h"

WiFiManager::WiFiManager()
    : _state(WiFiState::IDLE) {}

void WiFiManager::startAP(const char* ssid, const char* password) {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

    if (strlen(password) > 0) {
        WiFi.softAP(ssid, password);
    } else {
        WiFi.softAP(ssid);  // Open network (no password)
    }

    _state = WiFiState::AP_MODE;

    Serial.println("[WiFiManager] Access Point started.");
    Serial.println("  SSID:     " + String(ssid));
    Serial.println("  IP:       " + WiFi.softAPIP().toString());
}

void WiFiManager::stopAP() {
    WiFi.softAPdisconnect(true);
    Serial.println("[WiFiManager] Access Point stopped.");
}

bool WiFiManager::connectToWiFi(const String& ssid, const String& password, unsigned long timeoutMs) {
    Serial.println("[WiFiManager] Connecting to WiFi...");
    Serial.println("  SSID: " + ssid);

    _state = WiFiState::CONNECTING;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    unsigned long startTime = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > timeoutMs) {
            Serial.println("\n[WiFiManager] Connection timed out!");
            _state = WiFiState::DISCONNECTED;
            return false;
        }
        Serial.print(".");
        delay(WIFI_RETRY_DELAY_MS);
    }

    _state = WiFiState::CONNECTED;
    Serial.println("\n[WiFiManager] Connected!");
    Serial.println("  IP:   " + WiFi.localIP().toString());
    Serial.println("  RSSI: " + String(WiFi.RSSI()) + " dBm");

    return true;
}

void WiFiManager::disconnect() {
    WiFi.disconnect(true);
    _state = WiFiState::DISCONNECTED;
    Serial.println("[WiFiManager] Disconnected from WiFi.");
}

bool WiFiManager::isConnected() {
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected && _state != WiFiState::CONNECTED) {
        _state = WiFiState::CONNECTED;
    } else if (!connected && _state == WiFiState::CONNECTED) {
        _state = WiFiState::DISCONNECTED;
    }
    return connected;
}

IPAddress WiFiManager::getLocalIP() {
    if (_state == WiFiState::AP_MODE) {
        return WiFi.softAPIP();
    }
    return WiFi.localIP();
}

WiFiState WiFiManager::getState() const {
    return _state;
}

String WiFiManager::getStateString() const {
    switch (_state) {
        case WiFiState::IDLE:          return "IDLE";
        case WiFiState::AP_MODE:       return "AP_MODE";
        case WiFiState::CONNECTING:    return "CONNECTING";
        case WiFiState::CONNECTED:     return "CONNECTED";
        case WiFiState::DISCONNECTED:  return "DISCONNECTED";
        default:                       return "UNKNOWN";
    }
}
