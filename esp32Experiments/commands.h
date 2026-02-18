/*
 * commands.h — Smart Outlet Command Parsing & Protocol Logic
 * 
 * Handles the protocol: <device_id>:<command>
 * and all outlet state management (relays, current readings, overload).
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <Arduino.h>

// ============ CONFIGURATION ============

#define OVERLOAD_THRESHOLD 5000.0   // Max total current per outlet (mA)
#define MAX_OUTLETS 3               // Number of outlets in system

// ============ DATA STRUCTURE ============

struct SmartOutlet {
  String deviceId;
  float socketA_mA;       // Latest current for Socket A
  float socketB_mA;       // Latest current for Socket B
  bool relay1;            // Relay 1 (Socket A): true=ON
  bool relay2;            // Relay 2 (Socket B): true=ON
  bool overloaded;        // Overload state
};

// ============ OUTLETS ARRAY ============

SmartOutlet outlets[MAX_OUTLETS] = {
  {"SO-001", 0, 0, false, false, false},
  {"SO-002", 0, 0, false, false, false},
  {"SO-003", 0, 0, false, false, false}
};

// ============ HELPER: FIND OUTLET ============

int findOutlet(String deviceId) {
  for (int i = 0; i < MAX_OUTLETS; i++) {
    if (outlets[i].deviceId == deviceId) {
      return i;
    }
  }
  return -1;
}

// ============ WELCOME MESSAGE ============

void printWelcome() {
  Serial.println();
  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║   ESP32 Central Unit — Smart Outlet  ║");
  Serial.println("╠══════════════════════════════════════╣");
  Serial.println("║ Type commands to simulate outlets:   ║");
  Serial.println("║                                      ║");
  Serial.println("║  SO-001:A:1500    (Socket A: 1500mA) ║");
  Serial.println("║  SO-001:B:2000    (Socket B: 2000mA) ║");
  Serial.println("║  SO-001:R1=ON     (Relay 1 ON)       ║");
  Serial.println("║  SO-001:R1=OFF    (Relay 1 OFF)      ║");
  Serial.println("║  SO-001:R2=ON     (Relay 2 ON)       ║");
  Serial.println("║  SO-001:R2=OFF    (Relay 2 OFF)      ║");
  Serial.println("║  STATUS           (Show all outlets)  ║");
  Serial.println("║                                      ║");
  Serial.println("║ Threshold: " + String((int)OVERLOAD_THRESHOLD) + " mA (per outlet)       ║");
  Serial.println("╚══════════════════════════════════════╝");
  Serial.println();
}

// ============ CURRENT READING HANDLER ============

void handleCurrentReading(int idx, char socket, String valueStr) {
  float mA = valueStr.toFloat();

  // Update the reading
  if (socket == 'A') {
    outlets[idx].socketA_mA = mA;
  } else {
    outlets[idx].socketB_mA = mA;
  }

  float totalMA = outlets[idx].socketA_mA + outlets[idx].socketB_mA;

  // Display the readings
  Serial.println("📊 Socket A: " + String(outlets[idx].socketA_mA, 0) + " mA");
  Serial.println("📊 Socket B: " + String(outlets[idx].socketB_mA, 0) + " mA");
  Serial.println("📊 Total:    " + String(totalMA, 0) + " mA / " 
                 + String((int)OVERLOAD_THRESHOLD) + " mA");

  // Check overload: sum of both sockets
  if (totalMA > OVERLOAD_THRESHOLD) {
    Serial.println();
    Serial.println("🚨🚨🚨 OVERLOAD DETECTED! 🚨🚨🚨");
    Serial.println("⚡ Total " + String(totalMA, 0) + " mA exceeds threshold " 
                   + String((int)OVERLOAD_THRESHOLD) + " mA");
    Serial.println("📤 Sending cutoff to " + outlets[idx].deviceId + ":");
    Serial.println("   → " + outlets[idx].deviceId + ":SET:R1=OFF");
    Serial.println("   → " + outlets[idx].deviceId + ":SET:R2=OFF");

    // Cut off both relays
    outlets[idx].relay1 = false;
    outlets[idx].relay2 = false;
    outlets[idx].overloaded = true;

    // In Phase 2: send cutoff via Serial2 (HC12) to the actual outlet
    // Serial2.println(outlets[idx].deviceId + ":SET:R1=OFF");
    // Serial2.println(outlets[idx].deviceId + ":SET:R2=OFF");

    // Log critical event to cloud
    postEventLog(outlets[idx].deviceId, "overload", "critical", totalMA, "AB");

    // 5-second countdown then clear all readings
    Serial.println();
    for (int s = 5; s > 0; s--) {
      Serial.println("🔄 Clearing load in " + String(s) + "s...");
      delay(1000);
    }
    outlets[idx].socketA_mA = 0;
    outlets[idx].socketB_mA = 0;
    outlets[idx].overloaded = false;
    Serial.println("✅ Load cleared. Socket A: 0 mA | Socket B: 0 mA");
    Serial.println("   Outlet ready for new readings.");

  } else if (totalMA > OVERLOAD_THRESHOLD * 0.8) {
    Serial.println("⚠️  WARNING: approaching threshold (" 
                   + String((totalMA / OVERLOAD_THRESHOLD) * 100, 0) + "%)");
    outlets[idx].overloaded = false;
  } else {
    Serial.println("✅ Normal");
    outlets[idx].overloaded = false;
  }

  // Push reading to cloud (real-time UI via WebSocket)
  postSensorData(outlets[idx].deviceId, socket, mA);
}

// ============ RELAY COMMAND HANDLER ============

void handleRelayCommand(int idx, int relay, bool state) {
  String socketName = (relay == 1) ? "A" : "B";
  String stateStr = state ? "ON" : "OFF";

  // Check if outlet is in overload — don't allow turning ON
  if (state && outlets[idx].overloaded) {
    Serial.println("🚫 Cannot turn ON — outlet is in OVERLOAD state!");
    Serial.println("   Reset current readings first (send lower values).");
    return;
  }

  // Update relay state
  if (relay == 1) {
    outlets[idx].relay1 = state;
  } else {
    outlets[idx].relay2 = state;
  }

  String icon = state ? "🟢" : "🔴";
  Serial.println(icon + " Relay " + String(relay) + " (Socket " + socketName + ") → " + stateStr);

  // Show current readings alongside relay state
  float totalMA = outlets[idx].socketA_mA + outlets[idx].socketB_mA;
  Serial.println("📊 Socket A: " + String(outlets[idx].socketA_mA, 0) + " mA  [Relay1: " 
                 + String(outlets[idx].relay1 ? "ON" : "OFF") + "]");
  Serial.println("📊 Socket B: " + String(outlets[idx].socketB_mA, 0) + " mA  [Relay2: " 
                 + String(outlets[idx].relay2 ? "ON" : "OFF") + "]");
  Serial.println("📊 Total:    " + String(totalMA, 0) + " mA / " 
                 + String((int)OVERLOAD_THRESHOLD) + " mA");

  // Log relay event to cloud
  String eventType = state ? "power_on" : "power_off";
  postEventLog(outlets[idx].deviceId, eventType, "info", 0, socketName);
}

// ============ COMMAND HANDLER ============

void handleCommand(String raw) {
  Serial.println("──────────────────────────────────────");
  Serial.println("📥 Received: " + raw);

  // Find the first colon to extract device ID
  int firstColon = raw.indexOf(':');
  if (firstColon == -1) {
    Serial.println("❌ Invalid format. Use: SO-001:A:1500");
    Serial.println("──────────────────────────────────────");
    return;
  }

  String deviceId = raw.substring(0, firstColon);
  String command = raw.substring(firstColon + 1);

  // Find the outlet
  int outletIndex = findOutlet(deviceId);
  if (outletIndex == -1) {
    Serial.println("❌ Unknown device: " + deviceId);
    Serial.println("   Known devices: SO-001, SO-002, SO-003");
    Serial.println("──────────────────────────────────────");
    return;
  }

  Serial.println("🔌 Device: " + deviceId);

  // Route the command
  if (command.startsWith("A:")) {
    handleCurrentReading(outletIndex, 'A', command.substring(2));
  }
  else if (command.startsWith("B:")) {
    handleCurrentReading(outletIndex, 'B', command.substring(2));
  }
  else if (command == "R1=ON") {
    handleRelayCommand(outletIndex, 1, true);
  }
  else if (command == "R1=OFF") {
    handleRelayCommand(outletIndex, 1, false);
  }
  else if (command == "R2=ON") {
    handleRelayCommand(outletIndex, 2, true);
  }
  else if (command == "R2=OFF") {
    handleRelayCommand(outletIndex, 2, false);
  }
  else {
    Serial.println("❌ Unknown command: " + command);
  }

  Serial.println("──────────────────────────────────────");
  Serial.println();
}

// ============ STATUS DISPLAY ============

void printAllStatus() {
  Serial.println();
  Serial.println("══════════════════════════════════════");
  Serial.println("📋 STATUS — All Outlets");
  Serial.println("══════════════════════════════════════");

  for (int i = 0; i < MAX_OUTLETS; i++) {
    float total = outlets[i].socketA_mA + outlets[i].socketB_mA;
    String overloadFlag = outlets[i].overloaded ? " 🚨 OVERLOAD" : "";

    Serial.println("──────────────────────────────────────");
    Serial.println("🔌 Device: " + outlets[i].deviceId + overloadFlag);
    Serial.println("📊 Socket A: " + String(outlets[i].socketA_mA, 0) + " mA  [Relay1: " 
                   + String(outlets[i].relay1 ? "ON" : "OFF") + "]");
    Serial.println("📊 Socket B: " + String(outlets[i].socketB_mA, 0) + " mA  [Relay2: " 
                   + String(outlets[i].relay2 ? "ON" : "OFF") + "]");
    Serial.println("📊 Total:    " + String(total, 0) + " mA / " 
                   + String((int)OVERLOAD_THRESHOLD) + " mA");
  }

  Serial.println("══════════════════════════════════════");
  Serial.println();
}

#endif
