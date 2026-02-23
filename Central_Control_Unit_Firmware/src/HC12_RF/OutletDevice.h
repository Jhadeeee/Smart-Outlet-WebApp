/*
 * OutletDevice.h
 * ---------------
 * Represents a single PIC16F88 Smart Outlet device.
 * Tracks relay states, threshold, and master ID based on
 * ACK confirmations received over HC-12 RF.
 *
 * State values use -1 to indicate "unknown" (no ACK received yet).
 * Pending values are staged on send and committed only when
 * the corresponding ACK arrives.
 */

#ifndef OUTLET_DEVICE_H
#define OUTLET_DEVICE_H

#include <Arduino.h>
#include "RFProtocol.h"

class OutletDevice {
public:
    OutletDevice();

    // Initialize with a specific device ID
    void init(uint8_t deviceId);

    // ─── Getters ──────────────────────────────
    uint8_t getDeviceId() const;
    const char* getName() const;     // User-assigned label
    int8_t  getRelayA() const;       // -1=unknown, 0=OFF, 1=ON
    int8_t  getRelayB() const;       // -1=unknown, 0=OFF, 1=ON
    int     getThreshold() const;    // -1=unknown, else mA
    int     getMasterID() const;     // -1=unknown, else hex ID
    int     getCurrentA() const;     // -1=unknown, else mA
    int     getCurrentB() const;     // -1=unknown, else mA

    // ─── Setters ─────────────────────────────
    void setName(const char* name);
    void setCurrentA(int mA);
    void setCurrentB(int mA);

    // ─── Pending Values (staged before ACK) ──
    void setPendingThreshold(int mA);
    void setPendingMasterID(int id);

    // ─── ACK Processing ─────────────────────
    // Process an ACK packet and update internal state.
    // dataH = socket ID (for relay commands) or 0x00
    // dataL = original command code echoed back
    void processACK(uint8_t dataH, uint8_t dataL);

    // ─── State Reset ─────────────────────────
    // Clear all tracked state (called when switching targets)
    void resetState();

    // ─── Debug Output ────────────────────────
    // Print formatted device status to Serial
    void printStatus() const;

    // Check if this device slot is active (has a valid ID)
    bool isActive() const;

private:
    uint8_t _deviceId;
    bool    _active;
    char    _name[20];         // Max 19 chars + null terminator

    // Relay states: -1 = unknown, 0 = OFF, 1 = ON
    int8_t  _relayA;
    int8_t  _relayB;

    // Current readings in mA: -1 = unknown
    int     _currentA;
    int     _currentB;

    // Configuration values: -1 = unknown
    int     _threshold;
    int     _masterID;

    // Pending values (committed on ACK)
    int     _pendingThreshold;
    int     _pendingMasterID;
};

#endif // OUTLET_DEVICE_H
