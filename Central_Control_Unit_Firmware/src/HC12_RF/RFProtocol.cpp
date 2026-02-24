/*
 * RFProtocol.cpp
 * ---------------
 * Implementation of RF packet utilities.
 */

#include "RFProtocol.h"

// ─── Build a Packet ─────────────────────────────────────────
RFPacket RFProtocol::build(uint8_t target, uint8_t sender,
                            uint8_t cmd, uint8_t dataH, uint8_t dataL) {
    RFPacket pkt;
    pkt.sof     = RF_SOF;
    pkt.target  = target;
    pkt.sender  = sender;
    pkt.command = cmd;
    pkt.dataH   = dataH;
    pkt.dataL   = dataL;
    pkt.crc     = computeCRC(pkt);
    pkt.eof     = RF_EOF;
    return pkt;
}

// ─── Compute CRC ────────────────────────────────────────────
uint8_t RFProtocol::computeCRC(const RFPacket& pkt) {
    return pkt.target ^ pkt.sender ^ pkt.command ^ pkt.dataH ^ pkt.dataL;
}

// ─── Verify Packet ──────────────────────────────────────────
bool RFProtocol::verify(const RFPacket& pkt) {
    if (pkt.sof != RF_SOF) return false;
    if (pkt.eof != RF_EOF) return false;
    if (pkt.crc != computeCRC(pkt)) return false;
    return true;
}

// ─── Convert Buffer → Packet ────────────────────────────────
RFPacket RFProtocol::fromBuffer(const uint8_t* buffer) {
    RFPacket pkt;
    pkt.sof     = buffer[0];
    pkt.target  = buffer[1];
    pkt.sender  = buffer[2];
    pkt.command = buffer[3];
    pkt.dataH   = buffer[4];
    pkt.dataL   = buffer[5];
    pkt.crc     = buffer[6];
    pkt.eof     = buffer[7];
    return pkt;
}

// ─── Convert Packet → Buffer ────────────────────────────────
void RFProtocol::toBuffer(const RFPacket& pkt, uint8_t* buffer) {
    buffer[0] = pkt.sof;
    buffer[1] = pkt.target;
    buffer[2] = pkt.sender;
    buffer[3] = pkt.command;
    buffer[4] = pkt.dataH;
    buffer[5] = pkt.dataL;
    buffer[6] = pkt.crc;
    buffer[7] = pkt.eof;
}

// ─── Print Packet (Hex Dump) ────────────────────────────────
void RFProtocol::printPacket(const RFPacket& pkt, const char* label) {
    uint8_t buf[RF_PACKET_SIZE];
    toBuffer(pkt, buf);

    Serial.print(label);
    Serial.print(": ");
    for (int i = 0; i < RF_PACKET_SIZE; i++) {
        if (buf[i] < 0x10) Serial.print("0");
        Serial.print(buf[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

// ─── Command Name Lookup ────────────────────────────────────
const char* RFProtocol::commandName(uint8_t cmd) {
    switch (cmd) {
        case CMD_PING:           return "PING";
        case CMD_RELAY_ON:       return "RELAY_ON";
        case CMD_RELAY_OFF:      return "RELAY_OFF";
        case CMD_READ_CURRENT:   return "READ_CURRENT";
        case CMD_REPORT_DATA:    return "REPORT_DATA";
        case CMD_ACK:            return "ACK";
        case CMD_SET_THRESHOLD:  return "SET_THRESHOLD";
        case CMD_SET_DEVICE_ID:  return "SET_DEVICE_ID";
        case CMD_SET_ID_MASTER:  return "SET_ID_MASTER";
        default:                 return "UNKNOWN";
    }
}
