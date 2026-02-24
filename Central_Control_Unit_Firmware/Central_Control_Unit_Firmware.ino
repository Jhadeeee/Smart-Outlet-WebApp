/*
 * ═══════════════════════════════════════════════════════════
 *   CCU Firmware v4.0.0 — Central Control Unit for ESP32
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
#include <ArduinoJson.h>
#include "src/SetupPage/ConfigStorage.h"
#include "src/SetupPage/CaptivePortal.h"
#include "src/WiFiServer/WiFiManager.h"
#include "src/WiFiServer/Cloud.h"
#include "src/LocalDashboard/StatusLED.h"
#include "src/HC12_RF/RFProtocol.h"
#include "src/HC12_RF/OutletManager.h"
#include "src/LocalDashboard/SerialCLI.h"
#include "src/LocalDashboard/Dashboard.h"
#include "src/BreakerMonitor/BreakerMonitor.h"

// ─── Global Objects ─────────────────────────────────────────
ConfigStorage  configStorage;
WiFiManager    wifiManager;
CaptivePortal  captivePortal(configStorage);
Cloud          cloud;
StatusLED      statusLED;
OutletManager  outletManager;
SerialCLI      serialCLI(outletManager);
BreakerMonitor breakerMonitor;
Dashboard      dashboard(outletManager, configStorage, breakerMonitor);

// ─── State Machine ──────────────────────────────────────────
enum class DeviceMode {
    SETUP,            // AP mode — captive portal active
    LOCAL_DASHBOARD,  // AP mode — dashboard + HC-12 (no cloud)
    RUNNING           // STA mode — connected, cloud + HC-12 + dashboard
};

DeviceMode currentMode = DeviceMode::SETUP;

// ─── Timing ─────────────────────────────────────────────────
unsigned long lastCloudSend = 0;
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
    breakerMonitor.begin();
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
    dashboard.begin();
    breakerMonitor.begin();

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
    Serial.println("  CCU Firmware v4.0.0 — ESP32 Boot");
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
            breakerMonitor.update();
            serialCLI.update();
            break;

        // ─── Running Mode: Cloud + HC-12 communication ──
        case DeviceMode::RUNNING:
            // HC-12 RF: read incoming packets from smart outlets
            outletManager.update();

            // Breaker Monitor: read SCT013 sensor
            breakerMonitor.update();

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

            // Periodic data sending
            if (millis() - lastCloudSend >= CLOUD_SEND_INTERVAL_MS) {
                lastCloudSend = millis();

                // Build the comprehensive Test Dashboard JSON Payload
                StaticJsonDocument<1024> doc;
                
                // 1. CCU Block
                JsonObject ccu = doc.createNestedObject("ccu");
                ccu["uptime_seconds"] = millis() / 1000;
                ccu["main_breaker_mA"] = breakerMonitor.getMilliAmps();
                ccu["main_breaker_limit_mA"] = breakerMonitor.getThreshold();
                ccu["is_overload"] = breakerMonitor.isOverload();
                
                // 2. Devices Array
                JsonArray devicesArray = doc.createNestedArray("devices");
                for (uint8_t i = 0; i < outletManager.getDeviceCount(); i++) {
                    OutletDevice& dev = outletManager.getDevice(i);
                    // Add device if it has a valid ID
                    if (dev.getDeviceId() != 0x00 && dev.getDeviceId() != 0xFF) {
                        JsonObject devObj = devicesArray.createNestedObject();
                        // Format HEX ID as string (e.g., "FE")
                        char hexStr[3];
                        sprintf(hexStr, "%02X", dev.getDeviceId());
                        devObj["id"] = String(hexStr);
                        devObj["name"] = dev.getName();
                        devObj["limit_mA"] = dev.getThreshold() == -1 ? 5000 : dev.getThreshold();
                        
                        JsonObject sockA = devObj.createNestedObject("socket_a");
                        sockA["state"] = dev.getRelayA() == -1 ? 0 : dev.getRelayA();
                        sockA["mA"] = dev.getCurrentA();
                        
                        JsonObject sockB = devObj.createNestedObject("socket_b");
                        sockB["state"] = dev.getRelayB() == -1 ? 0 : dev.getRelayB();
                        sockB["mA"] = dev.getCurrentB();
                    }
                }
                
                String payload;
                serializeJson(doc, payload);

                int responseCode = cloud.sendData(payload);

                if (responseCode == 200) {
                    if (cloudFailCount > 0) {
                        Serial.println("✓ Cloud connection restored.");
                    }
                    cloudFailCount = 0;
                    statusLED.setPattern(LEDPattern::SOLID);
                    
                    // --- PHASE 2: Fetch and Execute Pending Commands ---
                    String commandJson = cloud.fetchCommands();
                    if (commandJson != "" && commandJson != "{}") {
                        StaticJsonDocument<2048> cmdDoc;
                        DeserializationError err = deserializeJson(cmdDoc, commandJson);
                        
                        // Proceed only if parsing succeeded and the server reported success
                        if (!err && cmdDoc["status"] == "success") {
                            JsonArray cmds = cmdDoc["commands"].as<JsonArray>();
                            
                            for (JsonObject cmd : cmds) {
                                String cmdType = cmd["command"];
                                String targetStr = cmd["target"];   // e.g "ALL", "0xFE"
                                JsonObject payload = cmd["payload"];
                                
                                Serial.println("📥 [CCU Command] Received: " + cmdType + " for " + targetStr);
                                
                                // Convert target string back to hex byte 
                                // Default to broadcast (0x00) if "ALL" doesn't parse
                                uint8_t targetId = 0x00;
                                if (targetStr != "ALL" && targetStr.length() > 0) {
                                    targetId = (uint8_t)strtol(targetStr.c_str(), NULL, 16);
                                }
                                
                                // Execute local or HC-12 Actions based on Command Type
                                if (cmdType == "CMD_SET_LIMIT") {
                                    int limit = payload["limit_mA"] | -1;
                                    if (limit > 0) {
                                        Serial.printf("  > Setting local Main Breaker limit to %d mA\n", limit);
                                        breakerMonitor.setThreshold(limit);
                                        // TODO: if targetId != 0x00, we technically should send this to a specific outlet
                                    }
                                } 
                                else if (cmdType == "CMD_CUT_POWER") {
                                    Serial.println("  > ⛔ EMERGENCY OVERRIDE! Disconnecting all output loads.");
                                    if (targetId == 0x00 || targetStr == "ALL") {
                                       // Loop through all physical devices and kill them
                                       for (uint8_t i = 0; i < outletManager.getDeviceCount(); i++) {
                                           OutletDevice& dev = outletManager.getDevice(i);
                                           if (dev.getDeviceId() != 0x00 && dev.getDeviceId() != 0xFF) {
                                               Serial.printf("    > Sending KILL to Outlet 0x%02X\n", dev.getDeviceId());
                                               outletManager.setRelays(dev.getDeviceId(), false, false);
                                               delay(150); // Small delay so the HC-12 buffer doesn't overflow
                                           }
                                       }
                                    } else {
                                        Serial.printf("    > Sending KILL to Outlet 0x%02X\n", targetId);
                                        outletManager.setRelays(targetId, false, false);
                                    }
                                }
                                else if (cmdType == "CMD_ADD_DEVICE") {
                                   Serial.println("  > Scanning for new unconfigured devices... (Not fully implemented on PIC yet)");
                                   // In the future: Broadcast a payload requesting devices with ID 0xFF to identify
                                }
                                else if (cmdType == "CMD_SET_ID_MASTER") {
                                    String newIdStr = payload["new_id"];
                                    if (newIdStr.length() > 0) {
                                        uint8_t newId = (uint8_t)strtol(newIdStr.c_str(), NULL, 16);
                                        Serial.printf("  > Updating CCU Master ID Config to 0x%02X\n", newId);
                                        // Currently hardcoded by CCU_SENDER_ID in Config.h, but we can log intent:
                                        Serial.println("    (Config.h modification required to persist)");
                                    }
                                }
                            }
                        } else if (err) {
                            Serial.println("✗ Failed to parse command JSON: " + String(err.c_str()));
                        }
                    }
                    
                } else {
                    cloudFailCount++;
                    if (cloudFailCount == 1) {
                        // Only log the first failure
                        Serial.println("✗ Cloud unreachable. Retrying silently...");
                    } else if (cloudFailCount % 6 == 0) {
                        // Reminder every ~60 seconds
                        Serial.println("✗ Cloud still unreachable (" + String(cloudFailCount) + " attempts).");
                    }
                    statusLED.setPattern(LEDPattern::SOLID);
                }
            }
            break;
    }
}
