/*
 * ConfigStorage.h
 * ----------------
 * Handles persistent storage of WiFi credentials and server URL
 * using the ESP32 Preferences (NVS) library.
 */

#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>
#include "../../Config.h"

class ConfigStorage {
public:
    ConfigStorage();

    // Initialize NVS storage
    void begin();

    // Save credentials to flash
    void save(const String& ssid, const String& password, const String& serverUrl);

    // Load credentials from flash into member variables
    bool load();

    // Check if valid credentials exist
    bool hasSavedConfig();

    // Clear all saved credentials (factory reset)
    void clear();

    // ─── Getters ──────────────────────────────
    String getSSID() const;
    String getPassword() const;
    String getServerUrl() const;

private:
    Preferences _preferences;
    String _ssid;
    String _password;
    String _serverUrl;
};

#endif // CONFIG_STORAGE_H
