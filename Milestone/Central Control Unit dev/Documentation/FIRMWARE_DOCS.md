# Central Control Unit (CCU) Firmware — Developer Documentation

**MCU:** ESP32 · **Framework:** Arduino · **IDE:** Arduino IDE / PlatformIO  
**Firmware:** v3.0.0 · **Communication:** HC-12 433MHz RF + WiFi

---

## Architecture Overview

```
┌────────────────────────────────────────────────────────────┐
│                          ESP32 (CCU)                       │
│                                                            │
│  ┌──────────────┐   ┌──────────────┐   ┌───────────────┐  │
│  │ WiFiManager  │   │ CaptivePortal│   │ ConfigStorage │  │
│  │ AP / STA     │   │ DNS + HTTP   │   │ NVS (Flash)   │  │
│  └──────┬───────┘   └──────┬───────┘   └───────┬───────┘  │
│         │                  │                    │          │
│         └──────────────────┼────────────────────┘          │
│                            │                               │
│  ┌─────────────────────────▼──────────────────────────┐    │
│  │            State Machine (3 modes)                 │    │
│  │  SETUP → LOCAL_DASHBOARD → RUNNING                 │    │
│  └─────┬────────────┬─────────────────┬───────────┘    │
│        │            │                 │                │
│  ┌─────▼─────┐ ┌────▼──────┐  ┌──────▼──────┐         │
│  │ Dashboard │ │ SerialCLI │  │   Cloud     │         │
│  │ WebServer │ │ Debug CLI │  │ HTTP Client │         │
│  └───────────┘ └───────────┘  └─────────────┘         │
│                                                        │
│  ┌─────────────────────────────────────────────────┐   │
│  │            OutletManager                        │   │
│  │  HC-12 UART2 (GPIO 16/17) @ 9600 baud          │   │
│  │  RX assembly → parse → dispatch                 │   │
│  │  OutletDevice[0..7] state tracking              │   │
│  └──────────────────────┬──────────────────────────┘   │
│                         │                              │
│  StatusLED (GPIO 2)     │    BOOT Button (GPIO 0)      │
└─────────────────────────┼──────────────────────────────┘
                          │ 433MHz RF (8-byte packets)
                   ┌──────┴──────┐
                   │ PIC16F88    │  Smart Outlet #1
                   │ PIC16F88    │  Smart Outlet #2
                   │ PIC16F88    │  Smart Outlet #N
                   └─────────────┘
```

---

## Pin Map

| GPIO | Function         | Direction | Notes                          |
|:-----|:-----------------|:----------|:-------------------------------|
| 0    | BOOT / Reset Btn | Input     | Hold 3s for factory reset      |
| 2    | Status LED       | Output    | Built-in LED on most ESP32 boards |
| 16   | HC-12 RX         | Input     | UART2 RX ← HC-12 TX           |
| 17   | HC-12 TX         | Output    | UART2 TX → HC-12 RX           |

---

## State Machine

The firmware operates in **three modes**, chosen at boot based on saved credentials:

```
┌──────────────────────────────┐
│          Power On            │
│  StatusLED.begin()           │
│  ConfigStorage.begin()       │
│  checkFactoryReset()         │
└──────────────┬───────────────┘
               │
        hasSavedConfig()?
        ┌──YES──┴──NO──┐
        │              │
   connectToWiFi()     │
     ┌──OK──┴──FAIL──┐ │
     │               │ │
  RUNNING         SETUP ◄┘
     │               │
     │        isDashboardRequested()?
     │         ┌──YES──┘
     │         │
     │    LOCAL_DASHBOARD
     │         │
     ▼         ▼
  loop() processes active mode
```

### Mode Descriptions

| Mode               | WiFi     | Services                              | LED Pattern     |
|:-------------------|:---------|:--------------------------------------|:----------------|
| `SETUP`            | AP       | Captive Portal only                   | Slow Blink (1s) |
| `LOCAL_DASHBOARD`  | AP       | Dashboard + HC-12 + Serial CLI        | Solid           |
| `RUNNING`          | STA      | Dashboard + HC-12 + Serial CLI + Cloud| Solid           |

---

## Boot Sequence

```
1. Serial.begin(115200)
2. StatusLED.begin() — GPIO 2 as output
3. ConfigStorage.begin() — open NVS namespace "ccu-config"
4. checkFactoryReset() — hold GPIO 0 LOW for 3s → clear NVS + restart
5. Check NVS for saved WiFi credentials:
   a. YES → Attempt WiFi STA connection (15s timeout)
      - Connected → enterRunningMode()
      - Failed    → enterSetupMode()
   b. NO → enterSetupMode()
```

---

## Main Loop

The loop runs the active mode's subsystems:

### SETUP Mode
- `captivePortal.handleClient()` — serves WiFi setup page + DNS redirect
- Watches for "Local Dashboard" button press → transitions to LOCAL_DASHBOARD

### LOCAL_DASHBOARD Mode
- `dashboard.handleClient()` — serves web dashboard
- `outletManager.update()` — reads/parses HC-12 packets
- `serialCLI.update()` — reads serial CLI input

### RUNNING Mode
- All LOCAL_DASHBOARD tasks, plus:
- WiFi reconnection — if disconnected, retries connection or falls back to SETUP
- Cloud data push — sends JSON payload to server every 10 seconds
- Cloud failure tracking — logs first failure, then reminders every ~60s

---

## HC-12 RF Protocol

### Packet Format

```
[SOF 0xAA] [TARGET] [SENDER] [CMD] [DATA_H] [DATA_L] [CRC] [EOF 0xBB]
```

**CRC** = `TARGET ^ SENDER ^ CMD ^ DATA_H ^ DATA_L`

### Packet Structure (C++)

```cpp
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
```

### RX Byte Assembly

1. Wait for `0xAA` byte — ignore non-SOF bytes (but pass printable ASCII to Serial as debug passthrough)
2. Buffer bytes until 8 total received
3. Verify CRC via `RFProtocol::verify()` — drop on failure
4. Parse: `CMD_ACK` → update device state, `CMD_REPORT_DATA` → store current reading
5. `0xFFFF` in REPORT_DATA = overload trip indicator

---

## Command Reference

| Code   | Name                | Data_H         | Data_L         | Direction       |
|:-------|:--------------------|:---------------|:---------------|:----------------|
| `0x01` | `CMD_PING`          | —              | —              | CCU → PIC       |
| `0x02` | `CMD_RELAY_ON`      | —              | Socket (1/2)   | CCU → PIC       |
| `0x03` | `CMD_RELAY_OFF`     | —              | Socket (1/2)   | CCU → PIC       |
| `0x04` | `CMD_READ_CURRENT`  | —              | —              | CCU → PIC       |
| `0x05` | `CMD_REPORT_DATA`   | Current mA (H) | Current mA (L) | PIC → CCU       |
| `0x06` | `CMD_ACK`           | Socket         | CMD echoed     | PIC → CCU       |
| `0x07` | `CMD_SET_THRESHOLD` | mA (H)         | mA (L)         | CCU → PIC       |
| `0x08` | `CMD_SET_DEVICE_ID` | —              | New ID         | CCU → PIC (cfg) |
| `0x09` | `CMD_SET_ID_MASTER` | —              | New ID         | CCU → PIC (cfg) |

### Socket Identifiers

| Value  | Socket   |
|:-------|:---------|
| `0x01` | SOCKET_A |
| `0x02` | SOCKET_B |

---

## Device Management

The CCU manages up to **8 Smart Outlets** simultaneously via the `OutletManager` + `OutletDevice` classes.

### State Tracking (OutletDevice)

Each `OutletDevice` tracks:

| Field            | Type     | Default | Notes                              |
|:-----------------|:---------|:--------|:-----------------------------------|
| `_deviceId`      | uint8_t  | 0x00    | PIC device address                 |
| `_name`          | char[20] | ""      | User-assigned label                |
| `_relayA`        | int8_t   | -1      | -1=unknown, 0=OFF, 1=ON           |
| `_relayB`        | int8_t   | -1      | -1=unknown, 0=OFF, 1=ON           |
| `_currentA`      | int      | -1      | -1=unknown, else mA               |
| `_currentB`      | int      | -1      | -1=unknown, else mA               |
| `_threshold`     | int      | -1      | -1=unknown, else mA               |
| `_masterID`      | int      | -1      | -1=unknown, else hex ID           |

### Pending-ACK Pattern

Configuration commands (threshold, master ID) use a two-stage commit:

```
1. User sets value → stored in _pendingThreshold / _pendingMasterID
2. Command sent to PIC via HC-12
3. PIC processes and sends CMD_ACK
4. processACK() commits pending → confirmed value
```

This prevents showing unconfirmed values to the user.

### Device ID Change Detection

When a PIC's device ID is changed:
- The ACK arrives from the **new** ID (no longer the old one)
- `_lastAckSender` tracks the sender of the most recent ACK
- Dashboard checks `getLastAckSender() == newId` for confirmation

### Current Routing

PIC reports current with `sender_id` as the **socket identifier** (not device ID):
- `sender == 0x01` (SOCKET_A) → `setCurrentA()` on the active device
- `sender == 0x02` (SOCKET_B) → `setCurrentB()` on the active device

---

## Dashboard (Web UI)

Served on port 80, accessible via:
- **AP mode:** `http://192.168.4.1/dashboard`
- **STA mode:** `http://<LAN_IP>/dashboard`

### Features

- **Empty state** → "No devices yet. Tap + to add one."
- **Add Device** modal → name + hex ID
- **Expandable device rows** → tap to show controls
- **Toggle switches** for Relay A/B (CSS-only, no images)
- **Live current** → auto-polls every 2 seconds on expanded device
- **Threshold** input per device
- **Master ID** → global config, applies to all devices
- **Context menu** (⋮) → Rename, Change Device ID, Delete
- **WiFi Settings** link → `/settings` page

### REST API

| Method | Route                 | Action                   | Parameters               |
|:-------|:----------------------|:-------------------------|:-------------------------|
| GET    | `/dashboard`          | Serve dashboard HTML     | —                        |
| GET    | `/settings`           | Serve settings page      | —                        |
| POST   | `/settings/save`      | Save WiFi config + restart| ssid, password, serverUrl|
| GET    | `/api/devices`        | List all devices (JSON)  | —                        |
| POST   | `/api/devices/add`    | Add device               | name, id (hex)           |
| POST   | `/api/devices/rename` | Rename device label      | index, name              |
| POST   | `/api/devices/delete` | Remove device            | index                    |
| POST   | `/api/devices/changeId`| Change PIC device ID    | index, newId (hex)       |
| POST   | `/api/relay`          | Toggle relay             | index, socket, state     |
| POST   | `/api/threshold`      | Set threshold per device | index, value (mA)        |
| POST   | `/api/master`         | Set global Master ID     | value (hex)              |
| GET    | `/api/status`         | Get device state + current| index                   |
| POST   | `/api/sensors`        | Trigger sensor read      | index                    |

---

## Captive Portal (Setup Mode)

When no WiFi credentials exist, the ESP32 starts an open AP (`CCU-Setup`):

1. DNS server redirects **all** domains to `192.168.4.1`
2. Web form collects: SSID, Password, Server URL
3. On submit → saves to NVS → restarts in STA mode
4. "Local Dashboard" button → skips WiFi, enters LOCAL_DASHBOARD mode

---

## NVS (Non-Volatile Storage) Map

| Namespace    | Key        | Content    | Notes                         |
|:-------------|:-----------|:-----------|:------------------------------|
| `ccu-config` | `ssid`     | WiFi SSID  | Max 32 chars                  |
| `ccu-config` | `password` | WiFi pass  | —                             |
| `ccu-config` | `serverUrl`| Server URL | For cloud data push           |

> **Factory Reset:** Hold BOOT button (GPIO 0) for 3+ seconds during startup → clears all NVS keys → ESP32 restarts into SETUP mode.

---

## Status LED (GPIO 2) Patterns

| Pattern       | Behavior               | Meaning                     |
|:--------------|:-----------------------|:----------------------------|
| `OFF`         | Always LOW             | LED disabled                |
| `SOLID`       | Always HIGH            | Connected / Dashboard ready |
| `SLOW_BLINK`  | 1s ON / 1s OFF         | AP mode (waiting for setup) |
| `FAST_BLINK`  | 150ms ON / 150ms OFF   | Connecting to WiFi          |
| `PULSE`       | 3 quick blinks + pause | Sending data                |

---

## Serial CLI Reference

The Serial CLI runs at **115200 baud** and provides debug/test access. Type `help` or `?` for the menu.

### Quick Commands

| Key | Action          | Notes                           |
|:----|:----------------|:--------------------------------|
| `1` | Relay A ON      | Sends `CMD_RELAY_ON` Socket A   |
| `2` | Relay A OFF     | Sends `CMD_RELAY_OFF` Socket A  |
| `3` | Relay B ON      | Sends `CMD_RELAY_ON` Socket B   |
| `4` | Relay B OFF     | Sends `CMD_RELAY_OFF` Socket B  |
| `5` | Read Sensors    | Sends `CMD_READ_CURRENT`        |
| `6` | Set Threshold   | Two-step: prompts for mA value  |
| `7` | Set Device ID   | Two-step: prompts for hex ID    |
| `8` | Set Master ID   | Two-step: prompts for hex ID    |

### Extended Commands

| Command             | Action                                        |
|:--------------------|:----------------------------------------------|
| `d FE`              | Switch target to device `0xFE`                |
| `d status`          | Print current device state (relays, current, etc.) |
| `AT...`             | Passthrough AT commands to HC-12 module       |
| `AA FE 00 02 ...`   | Send raw hex bytes directly via HC-12         |
| `help` / `?`        | Show help menu                                |

---

## Cloud Communication

When in RUNNING mode, the CCU periodically sends data to the configured server:

- **Endpoint:** Server URL from NVS config
- **Method:** HTTP POST with JSON payload
- **Interval:** Every 10 seconds (`CLOUD_SEND_INTERVAL_MS`)
- **Timeout:** 5 seconds per request (`HTTP_TIMEOUT_MS`)
- **Failure handling:** Logs first failure, then silent retries with reminders every ~60s

---

## Configuration Constants (Config.h)

### Access Point Settings

| Define          | Value               | Notes                         |
|:----------------|:--------------------|:------------------------------|
| `AP_SSID`       | `"CCU-Setup"`       | Hotspot name during setup     |
| `AP_PASSWORD`   | `""`                | Open network (no password)    |
| `AP_IP`         | `192.168.4.1`       | Fixed AP IP address           |

### WiFi Settings

| Define                    | Value   | Notes                    |
|:--------------------------|:--------|:-------------------------|
| `WIFI_CONNECT_TIMEOUT_MS` | `15000` | 15s connection timeout   |
| `WIFI_RETRY_DELAY_MS`     | `500`   | Delay between retries    |

### HC-12 / RF Protocol

| Define            | Value    | Notes                               |
|:------------------|:---------|:------------------------------------|
| `HC12_RX_PIN`     | `16`     | ESP32 GPIO ← HC-12 TX              |
| `HC12_TX_PIN`     | `17`     | ESP32 GPIO → HC-12 RX              |
| `HC12_BAUD`       | `9600`   | HC-12 default baud rate             |
| `RF_SOF`          | `0xAA`   | Start of Frame                      |
| `RF_EOF`          | `0xBB`   | End of Frame                        |
| `RF_PACKET_SIZE`  | `8`      | Fixed packet size                   |
| `CCU_SENDER_ID`   | `0x01`   | Must match PIC's `DEFAULT_ID_MASTER`|
| `MAX_OUTLETS`     | `8`      | Maximum managed devices             |

### Serial & Cloud

| Define                  | Value    | Notes                    |
|:------------------------|:---------|:-------------------------|
| `SERIAL_BAUD`           | `115200` | USB serial monitor       |
| `CLOUD_SEND_INTERVAL_MS`| `10000` | 10s between cloud pushes |
| `HTTP_TIMEOUT_MS`       | `5000`  | HTTP request timeout     |

---

## Module API Reference

### RFProtocol — RF Packet Utilities

| Method | Description |
|:-------|:------------|
| `static RFPacket build(target, sender, cmd, dataH, dataL)` | Build a complete packet with CRC computed automatically. |
| `static uint8_t computeCRC(const RFPacket&)` | Compute XOR checksum of target, sender, cmd, dataH, dataL. |
| `static bool verify(const RFPacket&)` | Verify SOF, EOF, and CRC. Returns `true` if valid. |
| `static RFPacket fromBuffer(const uint8_t*)` | Convert a raw 8-byte buffer into an RFPacket struct. |
| `static void toBuffer(const RFPacket&, uint8_t*)` | Copy an RFPacket struct into a raw 8-byte buffer. |
| `static void printPacket(const RFPacket&, const char* label)` | Print packet in hex format to Serial for debugging. |
| `static const char* commandName(uint8_t cmd)` | Get a human-readable command name string. |

---

### OutletDevice — Per-Device State Tracker

| Method | Description |
|:-------|:------------|
| `void init(uint8_t deviceId)` | Initialize with a specific device ID. Sets `_active = true`, resets state. |
| `uint8_t getDeviceId() const` | Get the device ID. |
| `const char* getName() const` | Get the user-assigned label. |
| `int8_t getRelayA() / getRelayB() const` | Get relay state: -1=unknown, 0=OFF, 1=ON. |
| `int getCurrentA() / getCurrentB() const` | Get current reading in mA: -1=unknown. |
| `int getThreshold() const` | Get threshold in mA: -1=unknown. |
| `int getMasterID() const` | Get master ID: -1=unknown. |
| `void setName(const char*)` | Set user-assigned label (max 19 chars). |
| `void setCurrentA(int mA) / setCurrentB(int mA)` | Set current reading. |
| `void setPendingThreshold(int mA)` | Stage threshold for ACK confirmation. |
| `void setPendingMasterID(int id)` | Stage master ID for ACK confirmation. |
| `void processACK(uint8_t dataH, uint8_t dataL)` | Process an ACK packet — commits pending values, updates relay state. |
| `void resetState()` | Clear all tracked state (all fields → -1). |
| `void printStatus() const` | Print formatted device status to Serial. |
| `bool isActive() const` | Check if this device slot has a valid ID. |

---

### OutletManager — HC-12 Communication Coordinator

| Method | Description |
|:-------|:------------|
| `void begin()` | Initialize HC-12 on UART2 (GPIO 16/17, 9600 baud). |
| `void update()` | Read HC-12 bytes, assemble into packets, parse. **Call in loop().** |
| `void sendCommand(cmd, dataH, dataL)` | Send a command to the currently active device. |
| `void relayOn(socket) / relayOff(socket)` | Toggle relay on active device (SOCKET_A / SOCKET_B). |
| `void readSensors()` | Send `CMD_READ_CURRENT` to active device. |
| `void setThreshold(unsigned int mA)` | Send `CMD_SET_THRESHOLD` with pending staging. |
| `void setDeviceID(uint8_t newId)` | Send `CMD_SET_DEVICE_ID` (PIC must be in config mode). |
| `void setMasterID(uint8_t newId)` | Send `CMD_SET_ID_MASTER` (PIC must be in config mode). |
| `void ping()` | Send `CMD_PING` to active device. |
| `void selectDevice(uint8_t deviceId)` | Select device by ID (creates if not found). |
| `OutletDevice& getActiveDevice()` | Get a reference to the currently active device. |
| `uint8_t getActiveDeviceId() const` | Get the active device's ID. |
| `uint8_t getDeviceCount() const` | Get total number of registered devices. |
| `OutletDevice& getDevice(uint8_t index)` | Get device by array index (for dashboard list). |
| `bool removeDevice(uint8_t index)` | Remove a device by index, shift array. |
| `uint8_t getSenderID() const` | Get the CCU's sender ID (master ID). |
| `void setSenderID(uint8_t id)` | Set the CCU's sender ID. |
| `uint8_t getLastAckSender() const` | Get sender of the most recent ACK (for ID change detection). |
| `void sendATCommand(const String&)` | Passthrough AT commands to HC-12 module. |
| `void sendRawHex(const String&)` | Send raw hex bytes directly via HC-12. |
| `HardwareSerial& getHC12()` | Get HC-12 serial reference for advanced use. |

---

### Dashboard — Local Web Dashboard

| Method | Description |
|:-------|:------------|
| `Dashboard(OutletManager&, ConfigStorage&)` | Constructor — takes references to manager and config. |
| `void begin()` | Start WebServer on port 80, register all routes. |
| `void stop()` | Stop the web server. |
| `void handleClient()` | Handle incoming HTTP requests. **Call in loop().** |

---

### CaptivePortal — WiFi Setup Portal

| Method | Description |
|:-------|:------------|
| `CaptivePortal(ConfigStorage&)` | Constructor — takes reference to config storage. |
| `void begin()` | Start web server (port 80) + DNS server (port 53). |
| `void stop()` | Stop both servers. |
| `void handleClient()` | Handle DNS + HTTP requests. **Call in loop().** |
| `bool isSubmitted() const` | True if user submitted credentials. |
| `bool isDashboardRequested() const` | True if user clicked "Local Dashboard". |

---

### ConfigStorage — NVS Persistent Storage

| Method | Description |
|:-------|:------------|
| `void begin()` | Open NVS namespace `"ccu-config"`. |
| `void save(ssid, password, serverUrl)` | Save credentials to flash. |
| `bool load()` | Load credentials from flash into member variables. |
| `bool hasSavedConfig()` | Check if valid credentials exist. |
| `void clear()` | Clear all saved credentials (factory reset). |
| `String getSSID() / getPassword() / getServerUrl() const` | Getters for loaded credentials. |

---

### WiFiManager — WiFi Mode Controller

| Method | Description |
|:-------|:------------|
| `void startAP(ssid, password)` | Start Access Point mode. Defaults: `"CCU-Setup"`, open. |
| `void stopAP()` | Stop Access Point. |
| `bool connectToWiFi(ssid, password, timeoutMs)` | Connect in STA mode. Returns true on success. |
| `void disconnect()` | Disconnect from WiFi. |
| `bool isConnected()` | Check WiFi connection status. |
| `IPAddress getLocalIP()` | Get IP address (works in AP and STA mode). |
| `WiFiState getState() const` | Get current state enum. |
| `String getStateString() const` | Get state as human-readable string. |

**WiFiState enum:** `IDLE`, `AP_MODE`, `CONNECTING`, `CONNECTED`, `DISCONNECTED`

---

### StatusLED — Non-Blocking LED Indicator

| Method | Description |
|:-------|:------------|
| `StatusLED(uint8_t pin = LED_PIN)` | Constructor — default: GPIO 2. |
| `void begin()` | Initialize LED pin as OUTPUT. |
| `void setPattern(LEDPattern)` | Set the current blink pattern. |
| `void update()` | Update LED state based on pattern. **Call in loop().** |
| `String getPatternName() const` | Get current pattern as a string (for debug). |

---

### Cloud — HTTP Client

| Method | Description |
|:-------|:------------|
| `void begin(const String& serverUrl)` | Initialize with server URL. |
| `int sendData(const String& jsonPayload)` | Send JSON via HTTP POST. Returns response code. |
| `bool isReachable()` | Check if server is reachable (GET request). |
| `String getServerUrl() const` | Get configured server URL. |
| `int getLastResponseCode() const` | Get last HTTP response code. |
| `String getLastResponse() const` | Get last response body. |

---

### SerialCLI — Debug Serial Interface

| Method | Description |
|:-------|:------------|
| `SerialCLI(OutletManager&)` | Constructor — takes reference to manager. |
| `void begin()` | Print help menu + "Listening" message. |
| `void update()` | Read Serial input, dispatch commands. **Call in loop().** |
| `void printHelp()` | Print the formatted help menu. |

---

## File Structure

```
Central_Control_Unit_Firmware/
├── Central_Control_Unit_Firmware.ino    — Main sketch: boot, state machine, loop
├── Config.h                             — Global constants (pins, protocol, timing)
│
├── data/                                — SPIFFS data (if any)
└── src/
    ├── HC12_RF/
    │   ├── RFProtocol.h / .cpp          — 8-byte packet build, verify, CRC
    │   ├── OutletDevice.h / .cpp        — Per-device state tracking
    │   └── OutletManager.h / .cpp       — HC-12 serial link, RX assembly, dispatch
    ├── LocalDashboard/
    │   ├── Dashboard.h / .cpp           — Web dashboard (HTML + REST API)
    │   ├── SerialCLI.h / .cpp           — Debug serial CLI
    │   └── StatusLED.h / .cpp           — Non-blocking LED patterns
    ├── SetupPage/
    │   ├── CaptivePortal.h / .cpp       — DNS redirect + setup web form
    │   └── ConfigStorage.h / .cpp       — NVS persistent credential storage
    └── WiFiServer/
        ├── WiFiManager.h / .cpp         — AP / STA mode control
        └── Cloud.h / .cpp              — HTTP client for server communication
```

---

## Factory Reset

**Method:** Hold BOOT button (GPIO 0) LOW for 3+ seconds during startup.

**Actions:**
1. `ConfigStorage::clear()` — erases all NVS keys
2. ESP32 restarts
3. No saved config found → enters SETUP mode (AP hotspot)

> **Use Case:** If WiFi credentials are wrong or server URL is misconfigured, factory reset is the way to re-enter setup.
