/*
 * ConfigStorage.cpp
 * ------------------
 * Implementation of persistent credential storage using ESP32 NVS.
 */

#include "ConfigStorage.h"

ConfigStorage::ConfigStorage()
    : _ssid(""), _password(""), _serverUrl("") {}

void ConfigStorage::begin() {
    _preferences.begin(NVS_NAMESPACE, false);  // false = read/write mode
    Serial.println("[ConfigStorage] NVS initialized.");
}

void ConfigStorage::save(const String& ssid, const String& password, const String& serverUrl) {
    _preferences.putString(NVS_KEY_SSID, ssid);
    _preferences.putString(NVS_KEY_PASSWORD, password);
    _preferences.putString(NVS_KEY_SERVER, serverUrl);

    _ssid = ssid;
    _password = password;
    _serverUrl = serverUrl;

    Serial.println("[ConfigStorage] Credentials saved to NVS.");
    Serial.println("  SSID:       " + _ssid);
    Serial.println("  Server URL: " + _serverUrl);
}

bool ConfigStorage::load() {
    _ssid      = _preferences.getString(NVS_KEY_SSID, "");
    _password  = _preferences.getString(NVS_KEY_PASSWORD, "");
    _serverUrl = _preferences.getString(NVS_KEY_SERVER, "");

    if (_ssid.length() > 0) {
        Serial.println("[ConfigStorage] Loaded credentials from NVS.");
        Serial.println("  SSID:       " + _ssid);
        Serial.println("  Server URL: " + _serverUrl);
        return true;
    }

    Serial.println("[ConfigStorage] No saved credentials found.");
    return false;
}

bool ConfigStorage::hasSavedConfig() {
    String ssid = _preferences.getString(NVS_KEY_SSID, "");
    return ssid.length() > 0;
}

void ConfigStorage::clear() {
    _preferences.clear();
    _ssid = "";
    _password = "";
    _serverUrl = "";
    Serial.println("[ConfigStorage] All credentials cleared (factory reset).");
}

String ConfigStorage::getSSID() const      { return _ssid; }
String ConfigStorage::getPassword() const  { return _password; }
String ConfigStorage::getServerUrl() const { return _serverUrl; }
