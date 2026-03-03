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
    breakerMonitor.tare();  // Zero-calibrate ADC offset
    outletManager.setBreakerMonitor(&breakerMonitor);  // Serial reads breaker live
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
        
        // Sync device list from Django database
        Serial.println("  Syncing device list from server...");
        String devJson = cloud.fetchDevices();
        devJson.replace(" ", ""); // Strip spaces for parsing
        
        if (devJson.indexOf("\"success\":true") >= 0) {
            // Parse device IDs from: {"success":true,"devices":["FE","FD"]}
            int arrStart = devJson.indexOf("[");
            int arrEnd = devJson.indexOf("]");
            
            if (arrStart >= 0 && arrEnd > arrStart) {
                String arr = devJson.substring(arrStart + 1, arrEnd);
                int count = 0;
                
                while (arr.length() > 0) {
                    int qStart = arr.indexOf("\"");
                    if (qStart < 0) break;
                    int qEnd = arr.indexOf("\"", qStart + 1);
                    if (qEnd < 0) break;
                    
                    String devId = arr.substring(qStart + 1, qEnd);
                    uint8_t id = (uint8_t)strtol(devId.c_str(), NULL, 16);
                    outletManager.selectDevice(id);
                    count++;
                    
                    arr = arr.substring(qEnd + 1);
                }
                
                Serial.println("  ✓ Synced " + String(count) + " device(s) from server.");
            }
        } else {
            Serial.println("  ✗ Could not fetch device list.");
        }
    } else {
        Serial.println("✗ Server not reachable (will retry).");
    }

    // Initialize HC-12 outlet communication + dashboard
    outletManager.begin();
    serialCLI.begin();
    dashboard.begin();
    breakerMonitor.begin();
    breakerMonitor.tare();  // Zero-calibrate ADC after WiFi connected
    outletManager.setBreakerMonitor(&breakerMonitor);  // Serial reads breaker live

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
            outletManager.update();      // Serial reads _lastAmps (same value dashboard saw)
            breakerMonitor.update();     // Update AFTER serial print — new reading for next cycle
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

            // Periodic cloud data sync & command fetch
            if (millis() - lastCloudSend >= CLOUD_SEND_INTERVAL_MS) {
                lastCloudSend = millis();
                bool anyFail = false;

                // 0. Re-sync device list from Django (picks up newly added outlets)
                String devJson = cloud.fetchDevices();
                devJson.replace(" ", "");
                if (devJson.indexOf("\"success\":true") >= 0) {
                    int arrStart = devJson.indexOf("[");
                    int arrEnd = devJson.indexOf("]");
                    if (arrStart >= 0 && arrEnd > arrStart) {
                        String arr = devJson.substring(arrStart + 1, arrEnd);
                        while (arr.length() > 0) {
                            int qStart = arr.indexOf("\"");
                            if (qStart < 0) break;
                            int qEnd = arr.indexOf("\"", qStart + 1);
                            if (qEnd < 0) break;
                            String devId = arr.substring(qStart + 1, qEnd);
                            uint8_t id = (uint8_t)strtol(devId.c_str(), NULL, 16);
                            outletManager.selectDevice(id); // No-op if already exists
                            arr = arr.substring(qEnd + 1);
                        }
                    }
                }

                // 1. ALWAYS send Breaker Data (same speed as sensors)
                if (breakerMonitor.hasReading()) {
                    String hexId = String(outletManager.getSenderID(), HEX);
                    hexId.toUpperCase();
                    String breakerPayload = "{\"ccu_id\":\"";
                    if (outletManager.getSenderID() < 0x10) breakerPayload += "0";
                    breakerPayload += hexId + "\",";
                    
                    // Read live value directly — same source as dashboard
                    breakerPayload += "\"current_ma\":" + String(breakerMonitor.getMilliAmps()) + "}";
                    
                    int res = cloud.sendBreakerData(breakerPayload);
                    if (res != 200 && res != 201) anyFail = true;
                }

                // 2. Fetch focused device from Django
                String focusJson = cloud.fetchFocusDevice();
                focusJson.replace(" ", "");
                String focusedId = "";
                
                // Parse: {"success":true,"device_id":"03"} or {"success":true,"device_id":null}
                if (focusJson.indexOf("\"device_id\":\"") >= 0) {
                    int idStart = focusJson.indexOf("\"device_id\":\"") + 13;
                    int idEnd = focusJson.indexOf("\"", idStart);
                    if (idEnd > idStart) {
                        focusedId = focusJson.substring(idStart, idEnd);
                    }
                }

                // 3. Only read sensors for the focused device
                if (focusedId.length() > 0) {
                    uint8_t focusId = (uint8_t)strtol(focusedId.c_str(), NULL, 16);
                    
                    // Find the device
                    int devIdx = -1;
                    for (uint8_t i = 0; i < outletManager.getDeviceCount(); i++) {
                        if (outletManager.getDevice(i).getDeviceId() == focusId) {
                            devIdx = i;
                            break;
                        }
                    }

                    if (devIdx >= 0) {
                        OutletDevice& dev = outletManager.getDevice(devIdx);
                        outletManager.selectDevice(dev.getDeviceId());
                        
                        // Flush stale RX data
                        while(outletManager.getHC12().available()) {
                            outletManager.getHC12().read();
                        }
                        
                        outletManager.readSensors();
                        
                        // Wait for PIC replies (Socket A & B)
                        unsigned long waitStart = millis();
                        while (millis() - waitStart < 350) {
                            outletManager.update();
                        }

                        // Send sensor data to cloud
                        String hexId = String(dev.getDeviceId(), HEX);
                        hexId.toUpperCase();
                        String payload = "{\"device_id\":\"";
                        if (dev.getDeviceId() < 0x10) payload += "0";
                        payload += hexId + "\",";
                        
                        payload += "\"current_a\":" + String(dev.getCurrentA()) + ",";
                        payload += "\"current_b\":" + String(dev.getCurrentB()) + ",";
                        payload += "\"relay_a\":" + String(dev.getRelayA() == 1 ? "true" : "false") + ",";
                        payload += "\"relay_b\":" + String(dev.getRelayB() == 1 ? "true" : "false") + ",";
                        bool isOverload = (dev.getCurrentA() == 65535 || dev.getCurrentB() == 65535);
                        payload += "\"is_overload\":" + String(isOverload ? "true" : "false") + "}";
                        
                        int res = cloud.sendSensorData(payload);
                        if (res != 200 && res != 201) anyFail = true;

                        // Fetch commands only for the focused device
                        String devIdStr = String(dev.getDeviceId(), HEX);
                        devIdStr.toUpperCase();
                        if (dev.getDeviceId() < 0x10) devIdStr = "0" + devIdStr;
                        
                        String jsonCommands = cloud.fetchCommands(devIdStr);
                        
                        if (jsonCommands.indexOf("\"success\": true") >= 0 || jsonCommands.indexOf("\"success\":true") >= 0) {
                            int startIdx = 0;
                            while (true) {
                                startIdx = jsonCommands.indexOf("{\"command\":", startIdx);
                                if (startIdx < 0) break;
                                int endIdx = jsonCommands.indexOf("}", startIdx);
                                if (endIdx < 0) break;
                                
                                String cmdObj = jsonCommands.substring(startIdx, endIdx);
                                
                                String cmd = "";
                                int cmdStart = cmdObj.indexOf("\"command\": \"");
                                if (cmdStart >= 0) {
                                    cmdStart += 12;
                                    int cmdEnd = cmdObj.indexOf("\"", cmdStart);
                                    if (cmdEnd >= 0) cmd = cmdObj.substring(cmdStart, cmdEnd);
                                }
                                
                                String socketStr = "";
                                int sockStart = cmdObj.indexOf("\"socket\": \"");
                                if (sockStart >= 0) {
                                    sockStart += 11;
                                    int sockEnd = cmdObj.indexOf("\"", sockStart);
                                    if (sockEnd >= 0) socketStr = cmdObj.substring(sockStart, sockEnd);
                                }
                                
                                int value = 0;
                                int valStart = cmdObj.indexOf("\"value\":");
                                if (valStart >= 0) {
                                    valStart += 8;
                                    int valEnd = cmdObj.indexOf(",", valStart);
                                    if (valEnd < 0) valEnd = cmdObj.length(); 
                                    String valStr = cmdObj.substring(valStart, valEnd);
                                    valStr.trim();
                                    if (valStr != "null") value = valStr.toInt();
                                }
                                
                                outletManager.selectDevice(dev.getDeviceId());
                                if (cmd == "relay_on") {
                                    outletManager.relayOn(socketStr == "a" ? SOCKET_A : SOCKET_B);
                                    delay(100);
                                } else if (cmd == "relay_off") {
                                    outletManager.relayOff(socketStr == "a" ? SOCKET_A : SOCKET_B);
                                    delay(100);
                                } else if (cmd == "set_threshold" && value > 0) {
                                    outletManager.setThreshold(value);
                                    delay(100);
                                }
                                
                                startIdx = endIdx + 1;
                            }
                        } else if (jsonCommands.length() > 0) {
                            anyFail = true;
                        }
                    }
                }
                // If no device focused → skip sensor reads entirely (silent)

                // Connection monitoring
                if (!anyFail) {
                    if (cloudFailCount > 0) {
                        Serial.println("[CLOUD] Connection restored.");
                    }
                    cloudFailCount = 0;
                    statusLED.setPattern(LEDPattern::SOLID);
                } else {
                    cloudFailCount++;
                    if (cloudFailCount == 1) {
                        Serial.println("[CLOUD] Communication issue. Retrying...");
                    } else if (cloudFailCount % 6 == 0) {
                        Serial.println("[CLOUD] Still failing (" + String(cloudFailCount) + " attempts).");
                    }
                    statusLED.setPattern(LEDPattern::SOLID);
                }
            }
            break;
    }
}
