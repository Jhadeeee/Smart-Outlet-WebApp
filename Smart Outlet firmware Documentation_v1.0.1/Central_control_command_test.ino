/*
 * Central_control_command_test.ino
 * 
 * ESP32 Master Controller for PIC16F88 Smart Outlet (v5.2)
 * 
 * Features:
 *   - Menu-driven: type 1-8 to send commands instantly
 *   - Two-step input for threshold (6), device ID (7), master ID (8)
 *   - Device selector: "d FE" to switch target PIC
 *   - Status display: "d status" to view current state
 *   - Raw hex fallback: paste "AA FE 00 02 00 01 FD BB" directly
 *   - AT command pass-through for HC-12 configuration
 * 
 * Hardware Connection (ESP32):
 *   HC12 TX -> ESP32 GPIO 16 (RX2)
 *   HC12 RX -> ESP32 GPIO 17 (TX2)
 *   GND -> GND
 *   VCC -> 5V
 */

#include <HardwareSerial.h>

// --- Configuration ---
#define HC12_RX_PIN 16
#define HC12_TX_PIN 17
#define BAUDRATE    9600

HardwareSerial HC12(2);

// --- State Tracking ---
uint8_t targetDevice = 0x01;  // Currently selected PIC
uint8_t senderID     = 0x00;  // ESP32 master ID

// Two-step input
bool    waitingForData = false;
uint8_t pendingCmd     = 0;

// Device state (tracked from ACKs)
int8_t  relayA           = -1;   // -1 = unknown, 0 = OFF, 1 = ON
int8_t  relayB           = -1;
int     lastThreshold    = -1;   // -1 = unknown
int     lastMasterID     = -1;   // -1 = unknown

// Pending values (stored on send, committed on ACK)
int     pendingThreshold = -1;
int     pendingMasterID  = -1;

// --- RX ---
uint8_t rxBuffer[8];
uint8_t rxIndex = 0;

// --- Function Prototypes ---
void buildAndSend(uint8_t cmd, uint8_t dataH, uint8_t dataL);
void handleInput(String input);
void handleDataInput(String input);
void parsePacket(uint8_t* frame);
void printStatus();
void printHelp();
void sendHexString(String hexStr);

void setup() {
  Serial.begin(115200);
  HC12.begin(BAUDRATE, SERIAL_8N1, HC12_RX_PIN, HC12_TX_PIN);
  
  delay(1000);
  printHelp();
  Serial.println("Listening for PIC response...\n");
}

void loop() {
  // 1. Read from PC Serial Monitor
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      if (waitingForData) {
        handleDataInput(input);
      } else {
        handleInput(input);
      }
    }
  }
  
  // 2. Read from HC12 -> Parse Packet
  if (HC12.available()) {
    while (HC12.available()) {
      uint8_t byte = HC12.read();
      
      // ASCII debug text passthrough
      if (rxIndex == 0 && byte != 0xAA) {
        if (byte >= 32 && byte <= 126) Serial.write(byte);
        else if (byte == 13 || byte == 10) Serial.write(byte);
        continue;
      }
      
      if (rxIndex == 0 && byte != 0xAA) continue;
      
      rxBuffer[rxIndex++] = byte;
      
      if (rxIndex >= 8) {
        parsePacket(rxBuffer);
        rxIndex = 0;
      }
    }
  }
}

// ============================================================
// BUILD & SEND PACKET
// ============================================================
void buildAndSend(uint8_t cmd, uint8_t dataH, uint8_t dataL) {
  uint8_t packet[8];
  packet[0] = 0xAA;                    // SOF
  packet[1] = targetDevice;            // Target
  packet[2] = senderID;                // Sender
  packet[3] = cmd;                     // Command
  packet[4] = dataH;                   // Data High
  packet[5] = dataL;                   // Data Low
  packet[6] = packet[1] ^ packet[2] ^ packet[3] ^ packet[4] ^ packet[5]; // CRC
  packet[7] = 0xBB;                    // EOF
  
  // Send via HC12
  for (int i = 0; i < 8; i++) HC12.write(packet[i]);
  
  // Print to Serial Monitor
  Serial.print("RAW: ");
  for (int i = 0; i < 8; i++) {
    if (packet[i] < 0x10) Serial.print("0");
    Serial.print(packet[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// ============================================================
// INPUT HANDLING
// ============================================================
void handleInput(String input) {
  // --- Single key commands (1-8) ---
  if (input.length() == 1 && input[0] >= '1' && input[0] <= '8') {
    char key = input[0];
    
    switch (key) {
      case '1':
        Serial.print("[TX] Relay A ON -> 0x");
        Serial.println(targetDevice, HEX);
        buildAndSend(0x02, 0x00, 0x01);
        break;
      case '2':
        Serial.print("[TX] Relay A OFF -> 0x");
        Serial.println(targetDevice, HEX);
        buildAndSend(0x03, 0x00, 0x01);
        break;
      case '3':
        Serial.print("[TX] Relay B ON -> 0x");
        Serial.println(targetDevice, HEX);
        buildAndSend(0x02, 0x00, 0x02);
        break;
      case '4':
        Serial.print("[TX] Relay B OFF -> 0x");
        Serial.println(targetDevice, HEX);
        buildAndSend(0x03, 0x00, 0x02);
        break;
      case '5':
        Serial.print("[TX] Read Sensors -> 0x");
        Serial.println(targetDevice, HEX);
        buildAndSend(0x04, 0x00, 0x00);
        break;
      case '6':
        Serial.print("Threshold (mA): ");
        waitingForData = true;
        pendingCmd = 6;
        break;
      case '7':
        Serial.print("New Device ID (hex): ");
        waitingForData = true;
        pendingCmd = 7;
        break;
      case '8':
        Serial.print("New Master ID (hex): ");
        waitingForData = true;
        pendingCmd = 8;
        break;
    }
    return;
  }
  
  // --- Device selector: "d XX" or "d status" ---
  if (input.startsWith("d ") || input.startsWith("D ")) {
    String arg = input.substring(2);
    arg.trim();
    
    if (arg.equalsIgnoreCase("status")) {
      printStatus();
      return;
    }
    
    // Parse hex device ID
    uint8_t newTarget = (uint8_t)strtol(arg.c_str(), NULL, 16);
    if (newTarget == 0 && arg != "0" && arg != "00") {
      Serial.println("Error: Invalid device ID");
      return;
    }
    targetDevice = newTarget;
    // Reset state for new device
    relayA = -1;
    relayB = -1;
    lastThreshold = -1;
    lastMasterID = -1;
    Serial.print("Target: 0x");
    if (targetDevice < 0x10) Serial.print("0");
    Serial.println(targetDevice, HEX);
    return;
  }
  
  // --- AT commands ---
  if (input.startsWith("AT") || input.startsWith("at")) {
    Serial.print("[AT] ");
    Serial.println(input);
    HC12.print(input);
    return;
  }
  
  // --- Raw hex mode (starts with "AA") ---
  if (input.startsWith("AA") || input.startsWith("aa")) {
    Serial.print("[TX] RAW: ");
    Serial.println(input);
    sendHexString(input);
    return;
  }
  
  // --- Help ---
  if (input.equalsIgnoreCase("help") || input == "?") {
    printHelp();
    return;
  }
  
  Serial.println("Unknown command. Type 'help' for options.");
}

// ============================================================
// TWO-STEP DATA INPUT (commands 6, 7, 8)
// ============================================================
void handleDataInput(String input) {
  waitingForData = false;
  
  switch (pendingCmd) {
    case 6: {
      // Threshold in mA (decimal)
      unsigned int mA = (unsigned int)input.toInt();
      if (mA == 0 && input != "0") {
        Serial.println("Error: Invalid threshold value");
        return;
      }
      uint8_t hi = (mA >> 8) & 0xFF;
      uint8_t lo = mA & 0xFF;
      Serial.print("[TX] Set Threshold ");
      Serial.print(mA);
      Serial.print("mA -> 0x");
      Serial.println(targetDevice, HEX);
      pendingThreshold = mA;
      buildAndSend(0x07, hi, lo);
      break;
    }
    case 7: {
      // Device ID in hex
      uint8_t id = (uint8_t)strtol(input.c_str(), NULL, 16);
      Serial.print("[TX] Set Device ID 0x");
      if (id < 0x10) Serial.print("0");
      Serial.print(id, HEX);
      Serial.print(" -> 0x");
      Serial.println(targetDevice, HEX);
      buildAndSend(0x08, 0x00, id);
      break;
    }
    case 8: {
      // Master ID in hex
      uint8_t id = (uint8_t)strtol(input.c_str(), NULL, 16);
      Serial.print("[TX] Set Master ID 0x");
      if (id < 0x10) Serial.print("0");
      Serial.print(id, HEX);
      Serial.print(" -> 0x");
      Serial.println(targetDevice, HEX);
      pendingMasterID = id;
      buildAndSend(0x09, 0x00, id);
      break;
    }
  }
  pendingCmd = 0;
}

// ============================================================
// PARSE INCOMING PACKET + STATE TRACKING
// ============================================================
void parsePacket(uint8_t* frame) {
  uint8_t sender = frame[2];
  uint8_t cmd    = frame[3];
  uint8_t dataH  = frame[4];
  uint8_t dataL  = frame[5];
  uint16_t val16 = (dataH << 8) | dataL;
  
  Serial.println("\n--- RX PACKET ---");
  
  // Print Raw
  Serial.print("RAW: ");
  for (int i = 0; i < 8; i++) {
    if (frame[i] < 0x10) Serial.print("0");
    Serial.print(frame[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  // Sender
  Serial.print("FROM: PIC 0x");
  if (sender < 0x10) Serial.print("0");
  Serial.println(sender, HEX);
  
  // Command type
  Serial.print("TYPE: ");
  if (cmd == 0x06) {
    Serial.println("ACK");
    
    // Socket
    Serial.print("  Socket: ");
    if (dataH == 0x01) Serial.println("A");
    else if (dataH == 0x02) Serial.println("B");
    else if (dataH == 0x00) Serial.println("System");
    else { Serial.print("0x"); Serial.println(dataH, HEX); }
    
    // Action
    Serial.print("  Action: ");
    switch (dataL) {
      case 0x02:
        Serial.println("Relay ON");
        // State tracking
        if (dataH == 0x01) relayA = 1;
        else if (dataH == 0x02) relayB = 1;
        break;
      case 0x03:
        Serial.println("Relay OFF");
        if (dataH == 0x01) relayA = 0;
        else if (dataH == 0x02) relayB = 0;
        break;
      case 0x07:
        Serial.println("Threshold Updated");
        if (pendingThreshold >= 0) {
          lastThreshold = pendingThreshold;
          pendingThreshold = -1;
        }
        break;
      case 0x08:
        Serial.println("Device ID Updated");
        break;
      case 0x09:
        Serial.println("Master ID Updated");
        if (pendingMasterID >= 0) {
          lastMasterID = pendingMasterID;
          pendingMasterID = -1;
        }
        break;
      case 0x01:
        Serial.println("Pong");
        break;
      default:
        Serial.print("CMD 0x");
        Serial.println(dataL, HEX);
        break;
    }
  }
  else if (cmd == 0x05) {
    Serial.println("DATA REPORT");
    if (val16 == 0xFFFF) {
      Serial.println("  >>> OVERLOAD TRIP! <<<");
    } else {
      Serial.print("  Current: ");
      Serial.print(val16);
      Serial.print(" mA (");
      Serial.print(val16 / 1000.0, 2);
      Serial.println(" A)");
    }
  }
  else {
    Serial.print("CMD 0x");
    Serial.println(cmd, HEX);
  }
  Serial.println("-----------------");
}

// ============================================================
// DEVICE STATUS
// ============================================================
void printStatus() {
  Serial.println("\n--- DEVICE STATUS ---");
  Serial.print("Target:    0x");
  if (targetDevice < 0x10) Serial.print("0");
  Serial.println(targetDevice, HEX);
  
  Serial.print("Socket A:  ");
  if (relayA == -1) Serial.println("---");
  else Serial.println(relayA ? "ON" : "OFF");
  
  Serial.print("Socket B:  ");
  if (relayB == -1) Serial.println("---");
  else Serial.println(relayB ? "ON" : "OFF");
  
  Serial.print("Threshold: ");
  if (lastThreshold == -1) Serial.println("---");
  else { Serial.print(lastThreshold); Serial.println(" mA"); }
  
  Serial.print("Master ID: ");
  if (lastMasterID == -1) Serial.println("---");
  else {
    Serial.print("0x");
    if (lastMasterID < 0x10) Serial.print("0");
    Serial.println(lastMasterID, HEX);
  }
  Serial.println("---------------------\n");
}

// ============================================================
// HELP MENU
// ============================================================
void printHelp() {
  Serial.println("\n========================================");
  Serial.println("  ESP32 HC12 Master v5.2");
  Serial.println("========================================");
  Serial.print("  Target: 0x");
  if (targetDevice < 0x10) Serial.print("0");
  Serial.println(targetDevice, HEX);
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

// ============================================================
// RAW HEX SENDER (legacy fallback)
// ============================================================
void sendHexString(String hexStr) {
  hexStr.replace(" ", "");
  
  int len = hexStr.length();
  if (len % 2 != 0) {
    Serial.println("Error: Hex string must have even number of characters.");
    return;
  }
  
  for (int i = 0; i < len; i += 2) {
    String byteStr = hexStr.substring(i, i + 2);
    uint8_t b = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
    HC12.write(b);
  }
}
