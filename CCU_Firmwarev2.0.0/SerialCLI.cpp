/*
 * SerialCLI.cpp
 * --------------
 * Implementation of the serial monitor debug interface.
 * Replicates all input handling from Central_control_command_test.ino.
 */

#include "SerialCLI.h"

SerialCLI::SerialCLI(OutletManager& manager)
    : _manager(manager),
      _waitingForData(false),
      _pendingCmd(0) {}

// ─── Begin ──────────────────────────────────────────────────
void SerialCLI::begin() {
    printHelp();
    Serial.println("Listening for PIC response...\n");
}

// ─── Update (Call in loop()) ────────────────────────────────
void SerialCLI::update() {
    if (!Serial.available()) return;

    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() == 0) return;

    if (_waitingForData) {
        _handleDataInput(input);
    } else {
        _handleInput(input);
    }
}

// ─── Handle Input (Features #15, #20-23, #25, #27-28) ──────
void SerialCLI::_handleInput(const String& input) {
    // --- Single key commands (1-8) --- (Feature #15)
    if (input.length() == 1 && input[0] >= '1' && input[0] <= '8') {
        char key = input[0];

        switch (key) {
            case '1':
                _manager.relayOn(SOCKET_A);
                break;
            case '2':
                _manager.relayOff(SOCKET_A);
                break;
            case '3':
                _manager.relayOn(SOCKET_B);
                break;
            case '4':
                _manager.relayOff(SOCKET_B);
                break;
            case '5':
                _manager.readSensors();
                break;
            case '6':  // Feature #16
                Serial.print("Threshold (mA): ");
                _waitingForData = true;
                _pendingCmd = 6;
                break;
            case '7':  // Feature #17
                Serial.print("New Device ID (hex): ");
                _waitingForData = true;
                _pendingCmd = 7;
                break;
            case '8':  // Feature #18
                Serial.print("New Master ID (hex): ");
                _waitingForData = true;
                _pendingCmd = 8;
                break;
        }
        return;
    }

    // --- Device selector: "d XX" or "d status" --- (Features #20, #21)
    if (input.startsWith("d ") || input.startsWith("D ")) {
        String arg = input.substring(2);
        arg.trim();

        if (arg.equalsIgnoreCase("status")) {
            _manager.getActiveDevice().printStatus();
            return;
        }

        // Parse hex device ID
        uint8_t newTarget = (uint8_t)strtol(arg.c_str(), NULL, 16);
        if (newTarget == 0 && arg != "0" && arg != "00") {
            Serial.println("Error: Invalid device ID");
            return;
        }
        _manager.selectDevice(newTarget);
        return;
    }

    // --- AT commands --- (Feature #22)
    if (input.startsWith("AT") || input.startsWith("at")) {
        _manager.sendATCommand(input);
        return;
    }

    // --- Raw hex mode (starts with "AA") --- (Feature #23)
    if (input.startsWith("AA") || input.startsWith("aa")) {
        Serial.print("[TX] RAW: ");
        Serial.println(input);
        _manager.sendRawHex(input);
        return;
    }

    // --- Help --- (Feature #25, #27)
    if (input.equalsIgnoreCase("help") || input == "?") {
        printHelp();
        return;
    }

    // --- Unknown command --- (Feature #28)
    Serial.println("Unknown command. Type 'help' for options.");
}

// ─── Handle Two-Step Data Input (Features #16-18) ───────────
void SerialCLI::_handleDataInput(const String& input) {
    _waitingForData = false;

    switch (_pendingCmd) {
        case 6: {
            // Threshold in mA (decimal input) — Feature #16
            unsigned int mA = (unsigned int)input.toInt();
            if (mA == 0 && input != "0") {
                Serial.println("Error: Invalid threshold value");
                _pendingCmd = 0;
                return;
            }
            _manager.setThreshold(mA);
            break;
        }
        case 7: {
            // Device ID in hex — Feature #17
            uint8_t id = (uint8_t)strtol(input.c_str(), NULL, 16);
            _manager.setDeviceID(id);
            break;
        }
        case 8: {
            // Master ID in hex — Feature #18
            uint8_t id = (uint8_t)strtol(input.c_str(), NULL, 16);
            _manager.setMasterID(id);
            break;
        }
    }
    _pendingCmd = 0;
}

// ─── Help Menu (Feature #25) ────────────────────────────────
void SerialCLI::printHelp() {
    Serial.println("\n========================================");
    Serial.println("  CCU Firmware v2.0.0 — HC-12 Master");
    Serial.println("========================================");
    Serial.print("  Target: 0x");
    uint8_t targetId = _manager.getActiveDeviceId();
    if (targetId < 0x10) Serial.print("0");
    Serial.println(targetId, HEX);
    Serial.println("----------------------------------------");
    Serial.println("  COMMANDS:");
    Serial.println("  1 = Relay A ON     5 = Read Sensors");
    Serial.println("  2 = Relay A OFF    6 = Set Threshold");
    Serial.println("  3 = Relay B ON     7 = Set Device ID");
    Serial.println("  4 = Relay B OFF    8 = Set Master ID");
    Serial.println("----------------------------------------");
    Serial.println("  DEVICE:");
    Serial.println("  d FE       -> switch target to 0xFE");
    Serial.println("  d status   -> show current state");
    Serial.println("----------------------------------------");
    Serial.println("  RAW HEX:");
    Serial.println("  AA FE 00 02 00 01 FD BB");
    Serial.println("----------------------------------------");
    Serial.println("  AT         -> HC-12 AT commands");
    Serial.println("  help       -> show this menu");
    Serial.println("========================================\n");
}
