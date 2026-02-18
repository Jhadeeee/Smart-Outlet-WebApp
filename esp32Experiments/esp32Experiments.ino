/*
 * ============================================
 *   ESP32 Central Unit — Smart Outlet System
 * ============================================
 * 
 * Main entry point. All logic is split into:
 *   commands.h/cpp — Protocol parsing, outlet state, overload detection
 *   cloud.h/cpp    — WiFi & HTTP cloud logging (Phase 2)
 * 
 * PROTOCOL:  <device_id>:<command>
 *   SO-001:A:1500       → Socket A current reading (1500 mA)
 *   SO-001:B:2000       → Socket B current reading (2000 mA)
 *   SO-001:R1=ON        → Relay 1 (Socket A) turned ON
 *   SO-001:R1=OFF       → Relay 1 (Socket A) turned OFF
 *   SO-001:R2=ON        → Relay 2 (Socket B) turned ON
 *   SO-001:R2=OFF       → Relay 2 (Socket B) turned OFF
 *   STATUS              → Show all outlet statuses
 * 
 * WIRING (for later with HC12):
 *   ESP32 GPIO16 (RX2) ← HC12 TX
 *   ESP32 GPIO17 (TX2) → HC12 RX
 *   GND ↔ GND
 */

#include "cloud.h"      // Must be first — commands.h calls cloud functions
#include "commands.h"

// ============ SETUP ============

void setup() {
  Serial.begin(115200);
  // Serial2.begin(9600);  // ← Uncomment later for HC12 on RX2/TX2

  delay(1000);
  printWelcome();
  connectWiFi();  // Phase 2: connects to WiFi. Currently just prints a stub message.
}

// ============ MAIN LOOP ============

void loop() {
  // Read from Serial Monitor (you typing)
  // Later: change to Serial2 for HC12
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() == 0) return;

    // Special command: show all outlet statuses
    if (input.equalsIgnoreCase("STATUS")) {
      printAllStatus();
      return;
    }

    // Parse the protocol: <device_id>:<command>
    handleCommand(input);
  }
}
