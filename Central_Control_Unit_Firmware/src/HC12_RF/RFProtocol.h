/*
 * RFProtocol.h
 * -------------
 * Defines the 8-byte RF packet structure and command codes
 * for HC-12 communication between the CCU (ESP32) and 
 * PIC16F88 Smart Outlet devices.
 *
 * Packet Format:
 *   [SOF 0xAA] [TARGET] [SENDER] [CMD] [DATA_H] [DATA_L] [CRC] [EOF 0xBB]
 *
 * CRC = TARGET ^ SENDER ^ CMD ^ DATA_H ^ DATA_L
 */

#ifndef RF_PROTOCOL_H
#define RF_PROTOCOL_H

#include <Arduino.h>
#include "../../Config.h"

// ─── Command Codes ──────────────────────────────────────────
#define CMD_PING            0x01
#define CMD_RELAY_ON        0x02
#define CMD_RELAY_OFF       0x03
#define CMD_READ_CURRENT    0x04
#define CMD_REPORT_DATA     0x05   // Response: PIC → CCU
#define CMD_ACK             0x06   // Response: PIC → CCU
#define CMD_SET_THRESHOLD   0x07
#define CMD_SET_DEVICE_ID   0x08   // Requires config mode on PIC
#define CMD_SET_ID_MASTER   0x09   // Requires config mode on PIC

// ─── Socket Identifiers ────────────────────────────────────
#define SOCKET_A            0x01
#define SOCKET_B            0x02

// ─── Packet Structure ───────────────────────────────────────
struct RFPacket {
    uint8_t sof;       // Start of Frame (0xAA)
    uint8_t target;    // Destination device ID
    uint8_t sender;    // Source device ID
    uint8_t command;   // Command code
    uint8_t dataH;     // Data high byte
    uint8_t dataL;     // Data low byte
    uint8_t crc;       // XOR checksum of bytes 1-5
    uint8_t eof;       // End of Frame (0xBB)
};

// ─── Protocol Utility Class ────────────────────────────────
class RFProtocol {
public:
    // Build a complete packet with CRC computed automatically
    static RFPacket build(uint8_t target, uint8_t sender,
                          uint8_t cmd, uint8_t dataH, uint8_t dataL);

    // Compute CRC (XOR of target, sender, cmd, dataH, dataL)
    static uint8_t computeCRC(const RFPacket& pkt);

    // Verify a received packet (checks SOF, EOF, and CRC)
    static bool verify(const RFPacket& pkt);

    // Convert a raw 8-byte buffer into an RFPacket struct
    static RFPacket fromBuffer(const uint8_t* buffer);

    // Copy an RFPacket struct into a raw 8-byte buffer
    static void toBuffer(const RFPacket& pkt, uint8_t* buffer);

    // Print a packet in hex format to Serial (for debugging)
    static void printPacket(const RFPacket& pkt, const char* label = "PKT");

    // Get a human-readable command name string
    static const char* commandName(uint8_t cmd);
};

#endif // RF_PROTOCOL_H
