/*
 * Dashboard.h
 * ------------
 * Local web dashboard served by ESP32 for controlling
 * PIC16F88 Smart Outlets via browser.
 *
 * Features:
 *   - Device list with add/rename/delete
 *   - Expandable device rows with toggle relay controls
 *   - Auto-polling current sensor values
 *   - Global Master ID configuration
 *   - Threshold configuration per device
 *   - WiFi settings page
 *   - REST API for JavaScript fetch() calls
 *
 * Accessible in both AP mode (192.168.4.1) and STA mode (LAN IP).
 */

#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>
#include <WebServer.h>
#include "../../Config.h"
#include "../HC12_RF/OutletManager.h"
#include "../SetupPage/ConfigStorage.h"

class Dashboard {
public:
    Dashboard(OutletManager& manager, ConfigStorage& config);

    // Start web server and register routes
    void begin();

    // Stop web server
    void stop();

    // Must be called in loop()
    void handleClient();

private:
    WebServer       _server;
    OutletManager&  _manager;
    ConfigStorage&  _config;

    // ─── Page Routes ─────────────────────────────
    void _handleDashboard();         // GET  /dashboard
    void _handleSettings();          // GET  /settings
    void _handleSaveSettings();      // POST /settings/save

    // ─── Device CRUD API ─────────────────────────
    void _handleApiDeviceList();     // GET  /api/devices
    void _handleApiAddDevice();      // POST /api/devices/add?name=X&id=FE
    void _handleApiRenameDevice();   // POST /api/devices/rename?index=0&name=X
    void _handleApiDeleteDevice();   // POST /api/devices/delete?index=0
    void _handleApiChangeDeviceId(); // POST /api/devices/changeId?index=0&newId=FE

    // ─── Control API ─────────────────────────────
    void _handleApiRelay();          // POST /api/relay?index=0&socket=1&state=on
    void _handleApiSetThreshold();   // POST /api/threshold?index=0&value=5000
    void _handleApiSetMasterID();    // POST /api/master?value=0A
    void _handleApiStatus();         // GET  /api/status?index=0
    void _handleApiReadSensors();    // POST /api/sensors?index=0

    // ─── HTML Builders ───────────────────────────
    String _buildDashboardPage();
    String _buildSettingsPage();
};

#endif // DASHBOARD_H
