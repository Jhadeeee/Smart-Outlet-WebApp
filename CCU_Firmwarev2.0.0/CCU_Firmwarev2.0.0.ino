/*
 * ═══════════════════════════════════════════════════════════
 *   CCU Firmware v2.0.0 — Central Control Unit for ESP32
 * ═══════════════════════════════════════════════════════════
 *
 *  SYSTEM ARCHITECTURE:
 *    ┌─────────────┐    WiFi     ┌──────────┐
 *    │   ESP32     │◄──────────►│  Server  │
 *    │   (CCU)     │    HTTP     │  (Cloud) │
 *    │             │            └──────────┘
 *    │  HC-12 RF   │
 *    │  GPIO 16/17 │
 *    └──────┬──────┘
 *           │ 433MHz RF (8-byte packets)
 *    ┌──────┴──────┐
 *    │ PIC16F88    │  Smart Outlet #1 (0x01)
 *    │ PIC16F88    │  Smart Outlet #2 (0xFE)
 *    │ PIC16F88    │  Smart Outlet #N ...
 *    └─────────────┘
 *
 *  FLOW:
 *    1. Boot → Check for saved WiFi credentials in NVS
 *    2. If NO credentials → Start AP hotspot + Captive Portal
 *       - User connects to "CCU-Setup" WiFi
 *       - User enters SSID, Password, Server URL via web form
 *       - Credentials saved → ESP32 restarts
 *    3. If credentials exist → Connect to saved WiFi (STA mode)
 *       - On success → Begin cloud + HC-12 communication
 *       - On failure → Fall back to AP mode for re-setup
 *
 *  HC-12 RF:
 *    - Communicates with PIC16F88 Smart Outlets via 8-byte packets
 *    - Commands: relay control, sensor read, threshold/ID config
 *    - Serial CLI available for debug/testing (type 'help')
 *
 *  FACTORY RESET:
 *    Hold BOOT button (GPIO 0) for 3+ seconds during startup
 *    to clear all saved credentials and enter AP setup mode.
 *
 * ═══════════════════════════════════════════════════════════
 */

#include "Config.h"
#include "ConfigStorage.h"
#include "WiFiManager.h"
#include "CaptivePortal.h"
#include "Cloud.h"
#include "StatusLED.h"
#include "RFProtocol.h"
#include "OutletManager.h"
#include "SerialCLI.h"

// ─── Global Objects ─────────────────────────────────────────
ConfigStorage  configStorage;
WiFiManager    wifiManager;
CaptivePortal  captivePortal(configStorage);
Cloud          cloud;
StatusLED      statusLED;
OutletManager  outletManager;
SerialCLI      serialCLI(outletManager);

// ─── State Machine ──────────────────────────────────────────
enum class DeviceMode {
    SETUP,      // AP mode — captive portal active
    RUNNING     // STA mode — connected, cloud + HC-12 active
};

DeviceMode currentMode = DeviceMode::SETUP;

// ─── Timing ─────────────────────────────────────────────────
unsigned long lastSensorPoll = 0;       // Timer for sensor polling
unsigned int  cloudFailCount = 0;       // Tracks consecutive failures to suppress spam

// ─── Factory Reset Check ────────────────────────────────────
void checkFactoryReset() {
    pinMode(RESET_BTN_PIN, INPUT_PULLUP);
    delay(100);  // Debounce

    if (digitalRead(RESET_BTN_PIN) == LOW) {
        Serial.println("⚠ BOOT button held — waiting 3 seconds for factory reset...");
        unsigned long start = millis();

        while (digitalRead(RESET_BTN_PIN) == LOW) {
            if (millis() - start > 3000) {
                Serial.println("🔄 Factory reset triggered!");
                configStorage.clear();
                delay(500);
                ESP.restart();
            }
        }
        Serial.println("Released early — no reset.");
    }
}

// ─── Start AP Setup Mode ────────────────────────────────────
void enterSetupMode() {
    currentMode = DeviceMode::SETUP;

    Serial.println("\n╔════════════════════════════════════╗");
    Serial.println("║     ENTERING SETUP MODE (AP)       ║");
    Serial.println("╚════════════════════════════════════╝");

    wifiManager.startAP();
    captivePortal.begin();
    statusLED.setPattern(LEDPattern::SLOW_BLINK);

    Serial.println("Connect to WiFi: " + String(AP_SSID));
    Serial.println("Then open:       http://" + wifiManager.getLocalIP().toString());
}

// ─── Start Normal Running Mode ──────────────────────────────
void enterRunningMode() {
    currentMode = DeviceMode::RUNNING;

    Serial.println("\n╔════════════════════════════════════╗");
    Serial.println("║    ENTERING RUNNING MODE (STA)     ║");
    Serial.println("╚════════════════════════════════════╝");

    // Initialize cloud communication
    cloud.begin(configStorage.getServerUrl());
    statusLED.setPattern(LEDPattern::SOLID);

    // Check if server is reachable
    if (cloud.isReachable()) {
        Serial.println("✓ Server is reachable: " + cloud.getServerUrl());
    } else {
        Serial.println("✗ Server not reachable (will retry).");
    }

    // Initialize HC-12 outlet communication
    outletManager.begin();

    // Register known smart outlets
    outletManager.registerDevice(0xFE);  // Smart Outlet 1
    outletManager.registerDevice(0xFD);  // Smart Outlet 2

    serialCLI.begin();

    Serial.println("\n✓ HC-12 RF + Serial CLI ready.");
    Serial.println("  Type 'help' for command list.\n");
}

// ═══════════════════════════════════════════════════════════
//   SETUP
// ═══════════════════════════════════════════════════════════
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);  // Allow serial monitor to connect

    Serial.println("\n");
    Serial.println("═══════════════════════════════════════");
    Serial.println("  CCU Firmware v2.0.0 — ESP32 Boot");
    Serial.println("═══════════════════════════════════════");

    // Initialize modules
    statusLED.begin();
    configStorage.begin();

    // Check for factory reset (hold BOOT button)
    checkFactoryReset();

    // Check for saved credentials
    if (configStorage.hasSavedConfig()) {
        configStorage.load();

        Serial.println("Saved config found. Connecting to WiFi...");
        statusLED.setPattern(LEDPattern::FAST_BLINK);

        bool connected = wifiManager.connectToWiFi(
            configStorage.getSSID(),
            configStorage.getPassword()
        );

        if (connected) {
            enterRunningMode();
        } else {
            Serial.println("WiFi connection failed. Falling back to setup mode.");
            enterSetupMode();
        }
    } else {
        Serial.println("No saved config. Starting setup...");
        enterSetupMode();
    }
}

// ═══════════════════════════════════════════════════════════
//   LOOP
// ═══════════════════════════════════════════════════════════
void loop() {
    // Always update LED patterns
    statusLED.update();

    switch (currentMode) {
        // ─── Setup Mode: Handle captive portal ──────────
        case DeviceMode::SETUP:
            captivePortal.handleClient();
            break;

        // ─── Running Mode: Cloud + HC-12 communication ──
        case DeviceMode::RUNNING:
            // HC-12 RF: read incoming packets from smart outlets
            outletManager.update();

            // Serial CLI: handle debug commands from serial monitor
            serialCLI.update();

            // ── Forward sensor data to Django when available ──
            if (outletManager.hasNewReading()) {
                // Build Django device ID: "SO-" + hex ID (e.g., "SO-FE")
                String djangoId = String(DEVICE_ID_PREFIX) + outletManager.getLastPolledHexId();
                float currentMA = outletManager.getLastCurrentMA();
                outletManager.clearNewReading();

                int code = cloud.sendSensorData(djangoId, currentMA);
                if (code == 200) {
                    if (cloudFailCount > 0) {
                        Serial.println("✓ Cloud connection restored.");
                    }
                    cloudFailCount = 0;
                } else {
                    cloudFailCount++;
                    if (cloudFailCount == 1) {
                        Serial.println("✗ Cloud unreachable. Retrying silently...");
                    } else if (cloudFailCount % 6 == 0) {
                        Serial.println("✗ Cloud still unreachable (" + String(cloudFailCount) + " attempts).");
                    }
                }
            }

            // ── Forward overload events to Django ──
            if (outletManager.isOverloadDetected()) {
                String djangoId = String(DEVICE_ID_PREFIX) + outletManager.getLastPolledHexId();
                outletManager.clearOverload();

                Serial.println("[Event] Overload → sending to server...");
                cloud.sendEventLog(djangoId, "overload", "critical",
                    "Overload trip detected — relay auto-cutoff");
            }

            // ── Auto-poll sensors (round-robin all outlets) ──
            if (millis() - lastSensorPoll >= SENSOR_POLL_INTERVAL_MS) {
                lastSensorPoll = millis();
                outletManager.pollNextDevice();
            }

            // ── Check WiFi connectivity ──
            if (!wifiManager.isConnected()) {
                Serial.println("WiFi lost! Attempting reconnection...");
                statusLED.setPattern(LEDPattern::FAST_BLINK);

                bool reconnected = wifiManager.connectToWiFi(
                    configStorage.getSSID(),
                    configStorage.getPassword()
                );

                if (reconnected) {
                    statusLED.setPattern(LEDPattern::SOLID);
                    Serial.println("Reconnected to WiFi.");
                } else {
                    Serial.println("Reconnection failed. Entering setup mode.");
                    enterSetupMode();
                    return;
                }
            }
            break;
    }
}
