/*
 * Cloud.cpp
 * ----------
 * Implementation of HTTP server communication.
 * Sends sensor readings and event logs to Django API.
 */

#include "Cloud.h"

Cloud::Cloud()
    : _serverUrl(""),
      _lastResponseCode(0),
      _lastResponse("") {}

void Cloud::begin(const String& serverUrl) {
    _serverUrl = serverUrl;

    // Ensure URL doesn't end with a trailing slash
    if (_serverUrl.endsWith("/")) {
        _serverUrl.remove(_serverUrl.length() - 1);
    }

    Serial.println("[Cloud] Initialized with server: " + _serverUrl);
}

// ─── Send Sensor Data ───────────────────────────────────────
int Cloud::sendSensorData(const String& deviceId, float currentMA) {
    // Convert mA to Amps for the API
    float currentA = currentMA / 1000.0;

    // Build JSON matching Django's expected format:
    // { device_id, voltage, current, power, energy, temperature }
    String json = "{";
    json += "\"device_id\":\"" + deviceId + "\",";
    json += "\"voltage\":0,";                          // PIC doesn't measure voltage
    json += "\"current\":" + String(currentA, 3) + ",";
    json += "\"power\":0,";                            // Can't calculate without voltage
    json += "\"energy\":0";
    json += "}";

    return sendData("/api/sensor-data/", json);
}

// ─── Send Event Log ─────────────────────────────────────────
int Cloud::sendEventLog(const String& deviceId, const String& eventType,
                        const String& severity, const String& message,
                        const String& socketLabel, float currentMA) {
    // Build JSON matching Django's expected format:
    // { device_id, event_type, severity, message, socket_label, current_reading }
    String json = "{";
    json += "\"device_id\":\"" + deviceId + "\",";
    json += "\"event_type\":\"" + eventType + "\",";
    json += "\"severity\":\"" + severity + "\",";
    json += "\"message\":\"" + message + "\"";

    if (socketLabel.length() > 0) {
        json += ",\"socket_label\":\"" + socketLabel + "\"";
    }
    if (currentMA > 0) {
        json += ",\"current_reading\":" + String(currentMA, 1);
    }

    json += "}";

    return sendData("/api/event-log/", json);
}

// ─── Generic POST ───────────────────────────────────────────
int Cloud::sendData(const String& endpoint, const String& jsonPayload) {
    if (_serverUrl.length() == 0) {
        return -1;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return -1;
    }

    HTTPClient http;
    String url = _serverUrl + endpoint;

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT_MS);

    _lastResponseCode = http.POST(jsonPayload);

    if (_lastResponseCode > 0) {
        _lastResponse = http.getString();
    } else {
        _lastResponse = http.errorToString(_lastResponseCode);
    }

    http.end();
    return _lastResponseCode;
}

// ─── Server Reachability ────────────────────────────────────
bool Cloud::isReachable() {
    if (_serverUrl.length() == 0 || WiFi.status() != WL_CONNECTED) {
        return false;
    }

    HTTPClient http;
    http.begin(_serverUrl);
    http.setTimeout(HTTP_TIMEOUT_MS);

    int code = http.GET();
    http.end();

    bool reachable = (code > 0);
    Serial.println("[Cloud] Server reachable: " + String(reachable ? "YES" : "NO"));
    return reachable;
}

String Cloud::getServerUrl() const       { return _serverUrl; }
int Cloud::getLastResponseCode() const   { return _lastResponseCode; }
String Cloud::getLastResponse() const    { return _lastResponse; }
