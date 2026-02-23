/*
 * OutletManager.cpp
 * ------------------
 * Implementation of the HC-12 communication manager.
 */

#include "OutletManager.h"

OutletManager::OutletManager()
    : _hc12(2),                     // UART2
      _senderID(CCU_SENDER_ID),
      _deviceCount(0),
      _activeIndex(0),
      _rxIndex(0),
      _lastAckSender(0) {
    // No default device — dashboard starts empty
}

// ─── Initialize HC-12 ──────────────────────────────────────
void OutletManager::begin() {
    _hc12.begin(HC12_BAUD, SERIAL_8N1, HC12_RX_PIN, HC12_TX_PIN);
    Serial.println("[OutletManager] HC-12 initialized (GPIO " +
                   String(HC12_RX_PIN) + "/" + String(HC12_TX_PIN) +
                   " @ " + String(HC12_BAUD) + " baud)");
    Serial.println("[OutletManager] Active target: 0x" +
                   String(_devices[_activeIndex].getDeviceId(), HEX));
}

// ─── Update (Call in loop()) ────────────────────────────────
void OutletManager::update() {
    if (!_hc12.available()) return;

    while (_hc12.available()) {
        uint8_t byte = _hc12.read();

        // ASCII debug text passthrough (Feature #4)
        // If we haven't started a packet and byte isn't SOF,
        // pass through printable ASCII or newlines
        if (_rxIndex == 0 && byte != RF_SOF) {
            if (byte >= 32 && byte <= 126) Serial.write(byte);
            else if (byte == 13 || byte == 10) Serial.write(byte);
            continue;
        }

        // Only start buffering on SOF byte
        if (_rxIndex == 0 && byte != RF_SOF) continue;

        _rxBuffer[_rxIndex++] = byte;

        // Once we have a full packet, parse it
        if (_rxIndex >= RF_PACKET_SIZE) {
            _parsePacket(_rxBuffer);
            _rxIndex = 0;
        }
    }
}

// ─── Send Command ───────────────────────────────────────────
void OutletManager::sendCommand(uint8_t cmd, uint8_t dataH, uint8_t dataL) {
    uint8_t targetId = _devices[_activeIndex].getDeviceId();

    RFPacket pkt = RFProtocol::build(targetId, _senderID, cmd, dataH, dataL);

    // Send via HC-12
    uint8_t buf[RF_PACKET_SIZE];
    RFProtocol::toBuffer(pkt, buf);
    for (int i = 0; i < RF_PACKET_SIZE; i++) {
        _hc12.write(buf[i]);
    }

    // Print TX packet to serial monitor
    RFProtocol::printPacket(pkt, "RAW");
}

// ─── Convenience Commands ───────────────────────────────────
void OutletManager::relayOn(uint8_t socket) {
    Serial.print("[TX] Relay ");
    Serial.print(socket == SOCKET_A ? "A" : "B");
    Serial.print(" ON -> 0x");
    Serial.println(_devices[_activeIndex].getDeviceId(), HEX);
    sendCommand(CMD_RELAY_ON, 0x00, socket);
}

void OutletManager::relayOff(uint8_t socket) {
    Serial.print("[TX] Relay ");
    Serial.print(socket == SOCKET_A ? "A" : "B");
    Serial.print(" OFF -> 0x");
    Serial.println(_devices[_activeIndex].getDeviceId(), HEX);
    sendCommand(CMD_RELAY_OFF, 0x00, socket);
}

void OutletManager::readSensors() {
    Serial.print("[TX] Read Sensors -> 0x");
    Serial.println(_devices[_activeIndex].getDeviceId(), HEX);
    sendCommand(CMD_READ_CURRENT, 0x00, 0x00);
}

void OutletManager::setThreshold(unsigned int mA) {
    uint8_t hi = (mA >> 8) & 0xFF;
    uint8_t lo = mA & 0xFF;
    Serial.print("[TX] Set Threshold ");
    Serial.print(mA);
    Serial.print("mA -> 0x");
    Serial.println(_devices[_activeIndex].getDeviceId(), HEX);
    _devices[_activeIndex].setPendingThreshold(mA);
    sendCommand(CMD_SET_THRESHOLD, hi, lo);
}

void OutletManager::setDeviceID(uint8_t newId) {
    Serial.print("[TX] Set Device ID 0x");
    if (newId < 0x10) Serial.print("0");
    Serial.print(newId, HEX);
    Serial.print(" -> 0x");
    Serial.println(_devices[_activeIndex].getDeviceId(), HEX);
    sendCommand(CMD_SET_DEVICE_ID, 0x00, newId);
}

void OutletManager::setMasterID(uint8_t newId) {
    Serial.print("[TX] Set Master ID 0x");
    if (newId < 0x10) Serial.print("0");
    Serial.print(newId, HEX);
    Serial.print(" -> 0x");
    Serial.println(_devices[_activeIndex].getDeviceId(), HEX);
    _devices[_activeIndex].setPendingMasterID(newId);
    sendCommand(CMD_SET_ID_MASTER, 0x00, newId);
}

void OutletManager::ping() {
    Serial.print("[TX] Ping -> 0x");
    Serial.println(_devices[_activeIndex].getDeviceId(), HEX);
    sendCommand(CMD_PING, 0x00, 0x00);
}

// ─── Device Management ─────────────────────────────────────
void OutletManager::selectDevice(uint8_t deviceId) {
    int idx = _findDevice(deviceId);
    if (idx < 0) {
        idx = _addDevice(deviceId);
        if (idx < 0) {
            Serial.println("[OutletManager] Error: Max outlets reached!");
            return;
        }
    }
    _activeIndex = idx;

    Serial.print("Target: 0x");
    if (deviceId < 0x10) Serial.print("0");
    Serial.println(deviceId, HEX);
}

OutletDevice& OutletManager::getActiveDevice() {
    return _devices[_activeIndex];
}

uint8_t OutletManager::getActiveDeviceId() const {
    if (_deviceCount == 0) return 0x00;
    return _devices[_activeIndex].getDeviceId();
}

uint8_t OutletManager::getDeviceCount() const {
    return _deviceCount;
}

OutletDevice& OutletManager::getDevice(uint8_t index) {
    return _devices[index];
}

bool OutletManager::removeDevice(uint8_t index) {
    if (index >= _deviceCount) return false;

    // Shift remaining devices down
    for (uint8_t i = index; i < _deviceCount - 1; i++) {
        _devices[i] = _devices[i + 1];
    }
    _deviceCount--;

    // Reset the vacated slot
    _devices[_deviceCount] = OutletDevice();

    // Fix active index
    if (_deviceCount == 0) {
        _activeIndex = 0;
    } else if (_activeIndex >= _deviceCount) {
        _activeIndex = _deviceCount - 1;
    }

    return true;
}

uint8_t OutletManager::getSenderID() const {
    return _senderID;
}

void OutletManager::setSenderID(uint8_t id) {
    _senderID = id;
    Serial.print("[OutletManager] Sender ID updated to 0x");
    if (id < 0x10) Serial.print("0");
    Serial.println(id, HEX);
}

uint8_t OutletManager::getLastAckSender() const {
    return _lastAckSender;
}

// ─── AT Command Passthrough ─────────────────────────────────
void OutletManager::sendATCommand(const String& cmd) {
    Serial.print("[AT] ");
    Serial.println(cmd);
    _hc12.print(cmd);
}

// ─── Raw Hex Sender ─────────────────────────────────────────
void OutletManager::sendRawHex(const String& hexStr) {
    String cleaned = hexStr;
    cleaned.replace(" ", "");

    int len = cleaned.length();
    if (len % 2 != 0) {
        Serial.println("Error: Hex string must have even number of characters.");
        return;
    }

    for (int i = 0; i < len; i += 2) {
        String byteStr = cleaned.substring(i, i + 2);
        uint8_t b = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
        _hc12.write(b);
    }
}

// ─── Get HC-12 Reference ────────────────────────────────────
HardwareSerial& OutletManager::getHC12() {
    return _hc12;
}

// ─── Find Device by ID ─────────────────────────────────────
int OutletManager::_findDevice(uint8_t deviceId) {
    for (uint8_t i = 0; i < _deviceCount; i++) {
        if (_devices[i].getDeviceId() == deviceId) {
            return i;
        }
    }
    return -1;
}

// ─── Add New Device ─────────────────────────────────────────
int OutletManager::_addDevice(uint8_t deviceId) {
    if (_deviceCount >= MAX_OUTLETS) return -1;

    _devices[_deviceCount].init(deviceId);
    return _deviceCount++;
}

// ─── Parse Incoming Packet ──────────────────────────────────
void OutletManager::_parsePacket(const uint8_t* frame) {
    RFPacket pkt = RFProtocol::fromBuffer(frame);

    // Verify CRC
    if (!RFProtocol::verify(pkt)) {
        Serial.println("[RX] CRC Error — packet dropped.");
        return;
    }

    uint8_t sender  = pkt.sender;
    uint8_t cmd     = pkt.command;
    uint8_t dataH   = pkt.dataH;
    uint8_t dataL   = pkt.dataL;
    uint16_t val16  = ((uint16_t)dataH << 8) | dataL;

    Serial.println("\n--- RX PACKET ---");

    // Print raw hex (Feature #30)
    RFProtocol::printPacket(pkt, "RAW");

    // Print sender (Feature #31)
    Serial.print("FROM: PIC 0x");
    if (sender < 0x10) Serial.print("0");
    Serial.println(sender, HEX);

    // Find or create the sender device for state tracking
    int senderIdx = _findDevice(sender);

    Serial.print("TYPE: ");

    if (cmd == CMD_ACK) {
        Serial.println("ACK");
        _lastAckSender = sender;  // Track for Device ID change detection

        // Update state on the sender device (Feature #5, #6, #7, #10-12)
        if (senderIdx >= 0) {
            _devices[senderIdx].processACK(dataH, dataL);
        } else {
            // ACK from an unknown device — still display it
            Serial.print("  Socket: ");
            if (dataH == SOCKET_A)       Serial.println("A");
            else if (dataH == SOCKET_B)  Serial.println("B");
            else if (dataH == 0x00)      Serial.println("System");
            else { Serial.print("0x"); Serial.println(dataH, HEX); }

            Serial.print("  Action: CMD 0x");
            Serial.println(dataL, HEX);
        }
    }
    else if (cmd == CMD_REPORT_DATA) {
        Serial.println("DATA REPORT");

        // Overload trip detection (Feature #9)
        if (val16 == 0xFFFF) {
            Serial.println("  >>> OVERLOAD TRIP! <<<");
        } else {
            // Current reading (Feature #8)
            Serial.print("  Current: ");
            Serial.print(val16);
            Serial.print(" mA (");
            Serial.print(val16 / 1000.0, 2);
            Serial.println(" A)");

            // Store current on active device, using sender as socket ID
            // PIC sets sender_id = 0x01 (Socket A) or 0x02 (Socket B)
            if (_deviceCount > 0) {
                if (sender == SOCKET_A)
                    _devices[_activeIndex].setCurrentA(val16);
                else if (sender == SOCKET_B)
                    _devices[_activeIndex].setCurrentB(val16);
            }
        }
    }
    else {
        // Unknown command type
        Serial.print("CMD 0x");
        Serial.println(cmd, HEX);
    }

    Serial.println("-----------------");
}
