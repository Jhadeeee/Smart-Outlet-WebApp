/*
 * OutletDevice.cpp
 * -----------------
 * Implementation of per-device state tracking.
 */

#include "OutletDevice.h"

OutletDevice::OutletDevice()
    : _deviceId(0x00),
      _active(false),
      _relayA(-1),
      _relayB(-1),
      _threshold(-1),
      _masterID(-1),
      _pendingThreshold(-1),
      _pendingMasterID(-1) {}

void OutletDevice::init(uint8_t deviceId) {
    _deviceId = deviceId;
    _active = true;
    resetState();
}

// ─── Getters ────────────────────────────────────────────────
uint8_t OutletDevice::getDeviceId() const  { return _deviceId; }
int8_t  OutletDevice::getRelayA() const    { return _relayA; }
int8_t  OutletDevice::getRelayB() const    { return _relayB; }
int     OutletDevice::getThreshold() const { return _threshold; }
int     OutletDevice::getMasterID() const  { return _masterID; }
bool    OutletDevice::isActive() const     { return _active; }

// ─── Pending Values ─────────────────────────────────────────
void OutletDevice::setPendingThreshold(int mA) {
    _pendingThreshold = mA;
}

void OutletDevice::setPendingMasterID(int id) {
    _pendingMasterID = id;
}

// ─── ACK Processing ────────────────────────────────────────
void OutletDevice::processACK(uint8_t dataH, uint8_t dataL) {
    // dataH = socket ID (for relay) or 0x00 (for system)
    // dataL = original command code echoed back

    Serial.print("  Socket: ");
    if (dataH == SOCKET_A)       Serial.println("A");
    else if (dataH == SOCKET_B)  Serial.println("B");
    else if (dataH == 0x00)      Serial.println("System");
    else { Serial.print("0x"); Serial.println(dataH, HEX); }

    Serial.print("  Action: ");
    switch (dataL) {
        case CMD_RELAY_ON:
            Serial.println("Relay ON");
            if (dataH == SOCKET_A) _relayA = 1;
            else if (dataH == SOCKET_B) _relayB = 1;
            break;

        case CMD_RELAY_OFF:
            Serial.println("Relay OFF");
            if (dataH == SOCKET_A) _relayA = 0;
            else if (dataH == SOCKET_B) _relayB = 0;
            break;

        case CMD_SET_THRESHOLD:
            Serial.println("Threshold Updated");
            if (_pendingThreshold >= 0) {
                _threshold = _pendingThreshold;
                _pendingThreshold = -1;
            }
            break;

        case CMD_SET_DEVICE_ID:
            Serial.println("Device ID Updated");
            break;

        case CMD_SET_ID_MASTER:
            Serial.println("Master ID Updated");
            if (_pendingMasterID >= 0) {
                _masterID = _pendingMasterID;
                _pendingMasterID = -1;
            }
            break;

        case CMD_PING:
            Serial.println("Pong");
            break;

        default:
            Serial.print("CMD 0x");
            Serial.println(dataL, HEX);
            break;
    }
}

// ─── State Reset ────────────────────────────────────────────
void OutletDevice::resetState() {
    _relayA = -1;
    _relayB = -1;
    _threshold = -1;
    _masterID = -1;
    _pendingThreshold = -1;
    _pendingMasterID = -1;
}

// ─── Status Display ─────────────────────────────────────────
void OutletDevice::printStatus() const {
    Serial.println("\n--- DEVICE STATUS ---");
    Serial.print("Target:    0x");
    if (_deviceId < 0x10) Serial.print("0");
    Serial.println(_deviceId, HEX);

    Serial.print("Socket A:  ");
    if (_relayA == -1) Serial.println("---");
    else Serial.println(_relayA ? "ON" : "OFF");

    Serial.print("Socket B:  ");
    if (_relayB == -1) Serial.println("---");
    else Serial.println(_relayB ? "ON" : "OFF");

    Serial.print("Threshold: ");
    if (_threshold == -1) Serial.println("---");
    else { Serial.print(_threshold); Serial.println(" mA"); }

    Serial.print("Master ID: ");
    if (_masterID == -1) Serial.println("---");
    else {
        Serial.print("0x");
        if (_masterID < 0x10) Serial.print("0");
        Serial.println(_masterID, HEX);
    }
    Serial.println("---------------------\n");
}
