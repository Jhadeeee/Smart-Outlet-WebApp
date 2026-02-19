/*
 * Cloud.cpp
 * ----------
 * Implementation of HTTP server communication.
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

int Cloud::sendData(const String& jsonPayload) {
    if (_serverUrl.length() == 0) {
        return -1;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return -1;
    }

    HTTPClient http;
    String endpoint = _serverUrl + "/api/data";  // Customize this endpoint

    http.begin(endpoint);
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
