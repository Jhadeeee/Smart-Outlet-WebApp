/*
 * OutletManager.h
 * ----------------
 * Central coordinator for HC-12 RF communication with
 * PIC16F88 Smart Outlet devices.
 *
 * Manages:
 *   - HC-12 serial link (HardwareSerial on UART2)
 *   - RX byte assembly into 8-byte packets
 *   - Packet parsing and dispatch to OutletDevice state
 *   - Command sending (relay control, sensor read, config)
 *   - Device selection (multi-outlet addressing)
 *   - AT command passthrough for HC-12 configuration
 *   - Raw hex fallback for manual testing
 */

#ifndef OUTLET_MANAGER_H
#define OUTLET_MANAGER_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "Config.h"
#include "RFProtocol.h"
#include "OutletDevice.h"

class OutletManager {
public:
    OutletManager();

    // Initialize HC-12 serial link
    void begin();

    // Must be called in loop() — reads HC-12 bytes, assembles packets
    void update();

    // ─── Command Sending ────────────────────
    // Send a command to the currently active device
    void sendCommand(uint8_t cmd, uint8_t dataH, uint8_t dataL);

    // Convenience methods
    void relayOn(uint8_t socket);      // socket: SOCKET_A or SOCKET_B
    void relayOff(uint8_t socket);
    void readSensors();
    void setThreshold(unsigned int mA);
    void setDeviceID(uint8_t newId);
    void setMasterID(uint8_t newId);
    void ping();

    // ─── Device Management ──────────────────
    // Select a device by ID (creates/finds it in the device array)
    void selectDevice(uint8_t deviceId);

    // Get a reference to the currently active device
    OutletDevice& getActiveDevice();

    // Get the active device's ID
    uint8_t getActiveDeviceId() const;

    // ─── HC-12 Utilities ────────────────────
    // Send an AT command to the HC-12 module
    void sendATCommand(const String& cmd);

    // Send raw hex bytes directly via HC-12
    void sendRawHex(const String& hexStr);

    // Get the HC-12 serial reference (for advanced use)
    HardwareSerial& getHC12();

private:
    HardwareSerial _hc12;           // UART2 for HC-12
    uint8_t       _senderID;        // ESP32 master ID (CCU_SENDER_ID)

    // Device array
    OutletDevice  _devices[MAX_OUTLETS];
    uint8_t       _deviceCount;
    uint8_t       _activeIndex;     // Index into _devices[]

    // RX buffer for packet assembly
    uint8_t       _rxBuffer[RF_PACKET_SIZE];
    uint8_t       _rxIndex;

    // ─── Internal Methods ───────────────────
    // Find device index by ID, returns -1 if not found
    int _findDevice(uint8_t deviceId);

    // Add a new device, returns index
    int _addDevice(uint8_t deviceId);

    // Parse a complete 8-byte packet
    void _parsePacket(const uint8_t* frame);
};

#endif // OUTLET_MANAGER_H
