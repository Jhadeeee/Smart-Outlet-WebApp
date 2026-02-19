/*
 * CaptivePortal.h
 * -----------------
 * Serves a captive portal web page for WiFi credential setup.
 * Uses DNS redirection + WebServer to capture user input.
 */

#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "ConfigStorage.h"

class CaptivePortal {
public:
    CaptivePortal(ConfigStorage& configStorage);

    // Start the web server and DNS server
    void begin();

    // Stop all servers
    void stop();

    // Must be called in loop() to handle DNS + HTTP requests
    void handleClient();

    // Check if user has submitted credentials (triggers restart)
    bool isSubmitted() const;

private:
    // ─── Route Handlers ───────────────────────
    void _handleRoot();
    void _handleSubmit();
    void _handleNotFound();

    // ─── HTML Page Builders ───────────────────
    String _buildSetupPage();
    String _buildSuccessPage(const String& serverUrl);

    // ─── Members ──────────────────────────────
    WebServer       _server;
    DNSServer       _dnsServer;
    ConfigStorage&  _configStorage;
    bool            _submitted;
};

#endif // CAPTIVE_PORTAL_H
