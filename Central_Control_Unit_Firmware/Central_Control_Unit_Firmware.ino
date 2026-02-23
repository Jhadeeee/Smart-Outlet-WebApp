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
#include "src/SetupPage/ConfigStorage.h"
#include "src/SetupPage/CaptivePortal.h"
#include "src/WiFiServer/WiFiManager.h"
#include "src/WiFiServer/Cloud.h"
#include "src/LocalDashboard/StatusLED.h"
#include "src/HC12_RF/RFProtocol.h"
#include "src/HC12_RF/OutletManager.h"
#include "src/LocalDashboard/SerialCLI.h"
#include "src/LocalDashboard/Dashboard.h"
#include "src/SCTSensor/SCTSensor.h"

// ─── Global Objects ─────────────────────────────────────────
ConfigStorage  configStorage;
WiFiManager    wifiManager;
CaptivePortal  captivePortal(configStorage);
Cloud          cloud;
StatusLED      statusLED;
OutletManager  outletManager;
SerialCLI      serialCLI(outletManager);
Dashboard      dashboard(outletManager, configStorage);
SCTSensor      sctSensor;

// ─── State Machine ──────────────────────────────────────────
enum class DeviceMode {
    SETUP,            // AP mode — captive portal active
    LOCAL_DASHBOARD,  // AP mode — dashboard + HC-12 (no cloud)
    RUNNING           // STA mode — connected, cloud + HC-12 + dashboard
};

DeviceMode currentMode = DeviceMode::SETUP;

// ─── Timing ─────────────────────────────────────────────────
unsigned long lastCloudSend = 0;
unsigned long lastSCTRead   = 0;
unsigned int  cloudFailCount = 0;    // Tracks consecutive failures to suppress spam

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

// ─── Start Local Dashboard Mode (AP + HC-12, no cloud) ──────
void enterLocalDashboardMode() {
    currentMode = DeviceMode::LOCAL_DASHBOARD;

    Serial.println("\n╔════════════════════════════════════╗");
    Serial.println("║   ENTERING LOCAL DASHBOARD (AP)    ║");
    Serial.println("╚════════════════════════════════════╝");

    // Stop captive portal (DNS redirect), keep AP running
    captivePortal.stop();

    // Start dashboard web server + HC-12
    dashboard.begin();
    outletManager.begin();
    serialCLI.begin();
    statusLED.setPattern(LEDPattern::SOLID);

    Serial.println("\n✓ Dashboard: http://" + wifiManager.getLocalIP().toString() + "/dashboard");
    Serial.println("✓ HC-12 RF + Serial CLI ready.");
    Serial.println("  Type 'help' for command list.\n");
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

    // Initialize HC-12 outlet communication + dashboard
    outletManager.begin();
    serialCLI.begin();

    // Initialize SCT-013 current sensor (main breaker)
    sctSensor.begin(SCT_ADC_PIN);
    dashboard.begin();

    Serial.println("\n✓ HC-12 RF + Serial CLI + Dashboard ready.");
    Serial.println("  Dashboard: http://" + wifiManager.getLocalIP().toString() + "/dashboard");
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

            // Check if user clicked "Local Dashboard"
            if (captivePortal.isDashboardRequested()) {
                enterLocalDashboardMode();
                return;
            }
            break;

        // ─── Local Dashboard: AP + HC-12 (no cloud) ─────
        case DeviceMode::LOCAL_DASHBOARD:
            dashboard.handleClient();
            outletManager.update();
            serialCLI.update();
            break;

        // ─── Running Mode: Cloud + HC-12 communication ──
        case DeviceMode::RUNNING:
            // HC-12 RF: read incoming packets from smart outlets
            outletManager.update();

            // Serial CLI: handle debug commands from serial monitor
            serialCLI.update();

            // Dashboard: handle web requests
            dashboard.handleClient();

            // Check WiFi is still connected
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

            // ─── SCT Sensor: Read and send main breaker current ───
            if (millis() - lastSCTRead >= SCT_READ_INTERVAL_MS) {
                lastSCTRead = millis();
                int breakerMA = sctSensor.readCurrentRMS();

                // Build JSON: {"ccu_id": "01", "current_ma": 4500}
                String ccuHex = String(CCU_SENDER_ID, HEX);
                ccuHex.toUpperCase();

                String breakerPayload = "{";
                breakerPayload += "\"ccu_id\":\"" + ccuHex + "\",";
                breakerPayload += "\"current_ma\":" + String(breakerMA);
                breakerPayload += "}";

                int rc = cloud.sendToEndpoint("/api/breaker-data/", breakerPayload);
                if (rc == 200) {
                    Serial.println("[SCT] Sent breaker current: " + String(breakerMA) + "mA");
                }
            }

            // Periodic data sending — send actual sensor data per outlet
            if (millis() - lastCloudSend >= CLOUD_SEND_INTERVAL_MS) {
                lastCloudSend = millis();

                bool anySendOk = false;
                uint8_t count = outletManager.getDeviceCount();

                for (uint8_t i = 0; i < count; i++) {
                    OutletDevice& dev = outletManager.getDevice(i);
                    if (!dev.isActive()) continue;

                    // Build JSON payload matching Django /api/data/ format
                    String deviceHex = String(dev.getDeviceId(), HEX);
                    deviceHex.toUpperCase();

                    int curA = dev.getCurrentA();
                    int curB = dev.getCurrentB();
                    bool overload = (curA == 65535 || curB == 65535);

                    String payload = "{";
                    payload += "\"device_id\":\"" + deviceHex + "\",";
                    payload += "\"current_a\":" + String(curA < 0 ? 0 : curA) + ",";
                    payload += "\"current_b\":" + String(curB < 0 ? 0 : curB) + ",";
                    payload += "\"relay_a\":" + String(dev.getRelayA() == 1 ? "true" : "false") + ",";
                    payload += "\"relay_b\":" + String(dev.getRelayB() == 1 ? "true" : "false") + ",";
                    payload += "\"is_overload\":" + String(overload ? "true" : "false");
                    payload += "}";

                    int responseCode = cloud.sendData(payload);
                    if (responseCode == 200) anySendOk = true;
                }

                // Also poll for pending commands from the UI
                for (uint8_t i = 0; i < count; i++) {
                    OutletDevice& dev = outletManager.getDevice(i);
                    if (!dev.isActive()) continue;

                    String deviceHex = String(dev.getDeviceId(), HEX);
                    deviceHex.toUpperCase();

                    String cmdJson = cloud.fetchCommands(deviceHex);
                    if (cmdJson.length() > 0) {
                        // Select this device as target before executing commands
                        outletManager.selectDevice(dev.getDeviceId());

                        // Simple parsing: find each "command":"xxx" and "socket":"x"
                        int searchFrom = 0;
                        while (true) {
                            int cmdIdx = cmdJson.indexOf("\"command\":", searchFrom);
                            if (cmdIdx < 0) break;

                            // Extract command value
                            int valStart = cmdJson.indexOf("\"", cmdIdx + 10) + 1;
                            int valEnd   = cmdJson.indexOf("\"", valStart);
                            String cmd   = cmdJson.substring(valStart, valEnd);

                            // Extract socket value
                            String sock = "";
                            int sockIdx = cmdJson.indexOf("\"socket\":", valEnd);
                            if (sockIdx >= 0 && sockIdx < cmdJson.indexOf("}", valEnd) + 1) {
                                int sStart = cmdJson.indexOf("\"", sockIdx + 9) + 1;
                                int sEnd   = cmdJson.indexOf("\"", sStart);
                                sock = cmdJson.substring(sStart, sEnd);
                            }

                            // Extract value (for threshold)
                            int valFieldIdx = cmdJson.indexOf("\"value\":", valEnd);
                            int cmdValue = 0;
                            if (valFieldIdx >= 0 && valFieldIdx < cmdJson.indexOf("}", valEnd) + 1) {
                                int numStart = valFieldIdx + 8;
                                // Skip whitespace
                                while (numStart < (int)cmdJson.length() && cmdJson[numStart] == ' ') numStart++;
                                if (cmdJson[numStart] != 'n') {  // not "null"
                                    cmdValue = cmdJson.substring(numStart).toInt();
                                }
                            }

                            // Execute the command
                            uint8_t socket = (sock == "b") ? SOCKET_B : SOCKET_A;

                            if (cmd == "relay_on") {
                                Serial.println("[Cloud CMD] Relay ON socket " + sock);
                                outletManager.relayOn(socket);
                            } else if (cmd == "relay_off") {
                                Serial.println("[Cloud CMD] Relay OFF socket " + sock);
                                outletManager.relayOff(socket);
                            } else if (cmd == "set_threshold") {
                                Serial.println("[Cloud CMD] Set threshold " + String(cmdValue) + "mA");
                                outletManager.setThreshold(cmdValue);
                            } else if (cmd == "read_sensors") {
                                Serial.println("[Cloud CMD] Read sensors");
                                outletManager.readSensors();
                            } else if (cmd == "ping") {
                                Serial.println("[Cloud CMD] Ping");
                                outletManager.ping();
                            }

                            searchFrom = valEnd + 1;
                        }
                    }
                }

                // Status tracking
                if (count == 0 || anySendOk) {
                    if (cloudFailCount > 0) {
                        Serial.println("✓ Cloud connection restored.");
                    }
                    cloudFailCount = 0;
                    statusLED.setPattern(LEDPattern::SOLID);
                } else {
                    cloudFailCount++;
                    if (cloudFailCount == 1) {
                        Serial.println("✗ Cloud unreachable. Retrying silently...");
                    } else if (cloudFailCount % 6 == 0) {
                        Serial.println("✗ Cloud still unreachable (" + String(cloudFailCount) + " attempts).");
                    }
                    statusLED.setPattern(LEDPattern::SOLID);
                }
            }
            break;
    }
}
