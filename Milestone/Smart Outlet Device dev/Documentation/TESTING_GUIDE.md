# Smart Outlet Firmware — Testing Guide

**Device:** PIC16F88 | **Firmware:** v5.3.1
**Default IDs:** `DEVICE_ID = 0x01` · `ID_MASTER = 0x01` · **Threshold:** 3000mA

---

## 1. Simulation Testing (Proteus)

### Prerequisites

- Simulation mode is **commented out** by default — uncomment lines 159-162 in `__main__.c` for Proteus testing
- Add **push button** on RB3 (active LOW) and **LED** on RB4

### Keyboard Shortcuts

| Key   | Action                                        | SoftUART Output     |
| :---- | :-------------------------------------------- | :------------------ |
| `1` | Relay A **ON**                           | `R1+`             |
| `2` | Relay A **OFF**                          | `R1-`             |
| `3` | Relay B **ON**                           | `R2+`             |
| `4` | Relay B **OFF**                          | `R2-`             |
| `5` | Read Sensors                                  | `A:<val>\|B:<val>` |
| `6` | Set Threshold 10000mA                         | `Cfg:10000`       |
| `7` | Set Device ID → 0xFE *(requires config mode)* | `ID:FE` or `Cfg?` |
| `8` | Set Master ID → 0x0A *(requires config mode)* | `MA:0A` or `Cfg?` |

### Config Mode & Factory Reset

| Action                      | How                                   | Expected                             |
| :-------------------------- | :------------------------------------ | :----------------------------------- |
| **Enter Config Mode** | Hold RB3 LOW for 3 seconds            | SoftUART: `"Cfg!"`                  |
| **Set Device ID**     | While in config mode, press key `7` | RB4 → HIGH, config mode deactivates |
| **Set Master ID**     | While in config mode, press key `8` | RB4 → HIGH, config mode deactivates |
| **Factory Reset**     | Press RB3 3 times (short presses)     | All defaults restored, RB4 → LOW    |

### RB4 Status LED

| RB4 State      | Meaning                                             |
| :------------- | :-------------------------------------------------- |
| **LOW**  | All defaults (ID=0x01, Master=0x01, Threshold=3000) |
| **HIGH** | All values have been configured                    |

### Boot Messages

| Output       | Meaning                        |
| :----------- | :----------------------------- |
| `v5.3.1`   | Firmware booted                |
| `Cal`      | Sensor calibration in progress |
| `Rdy`      | System ready                   |

### Simulation Test Sequence

1. Boot → RB4 = LOW (defaults)
2. Press key `7` → `"Cfg?"` (rejected, not in config mode)
3. Hold RB3 3s → `"Cfg!"`
4. Press key `7` → `"ID:FE"` → config mode deactivates
5. Hold RB3 3s → `"Cfg!"` again
6. Press key `8` → `"MA:0A"`
7. Press RB3 3× → RB4 = LOW (factory reset)
8. Reset PIC → boots with all defaults → RB4 = LOW
9. Keys 1-6 → unchanged behavior

---

## 2. Hardware Testing (ESP32 + HC-12)

### Prerequisites

- Flash `Central_control_command_test.ino` to ESP32
- Open Serial Monitor at **115200 baud**
- HC-12 modules configured to same channel/baud
- PIC firmware with simulation mode **commented out**
- ESP32 `senderID` must match PIC's `id_master` (both default to `0x01`)

> **If PIC rejects all commands:** the `senderID` doesn't match `id_master`. Either update the ESP32's `senderID` or factory reset the PIC (press RB3 ×3).

### ESP32 Serial Monitor Commands

| Input        | Action                                  |
| :----------- | :-------------------------------------- |
| `1`        | Relay A ON                              |
| `2`        | Relay A OFF                             |
| `3`        | Relay B ON                              |
| `4`        | Relay B OFF                             |
| `5`        | Read Sensors                            |
| `6`        | Set Threshold (prompts for mA value)    |
| `7`        | Set Device ID (prompts for hex value)   |
| `8`        | Set Master ID (prompts for hex value)   |
| `d FE`     | Switch target device to 0xFE            |
| `m 0A`     | Switch sender ID to 0x0A (test auth)    |
| `d status` | Show target, sender, relay states       |
| `help`     | Show help menu                          |
| `AA ...`   | Send raw hex packet (CRC must be manual)|

### Device Selection

Switch target before sending commands:
```
> d FE
Target: 0xFE
> d FD
Target: 0xFD
```

### Device Status

Check current tracked state at any time:
```
> d status
--- DEVICE STATUS ---
Target:    0xFE
Sender ID: 0x01
Socket A:  ON
Socket B:  OFF
Threshold: 5000 mA
Master ID: 0x0A
---------------------
```

> **Note:** States show `---` until the first ACK confirms them.

### Hardware Test Sequence

1. Power on ESP32 → help menu displays
2. `d FE` → set target to PIC 1
3. `1` → Relay A ON → wait for ACK
4. `3` → Relay B ON → wait for ACK
5. `d status` → verify Socket A: ON, Socket B: ON
6. `2` → Relay A OFF → wait for ACK
7. `d status` → verify Socket A: OFF, Socket B: ON
8. `5` → Read Sensors → verify current readings
9. `6` → enter `5000` → threshold ACK
10. `d status` → verify Threshold: 5000 mA

### Config Mode Test (Hardware)

1. Hold physical RB3 for 3s → PIC enters config mode
2. `7` → enter `FE` → Set Device ID ACK received
3. `d FE` → switch ESP32 target to match new ID
4. `1` → Relay A ON → ACK from 0xFE confirms it worked
5. Send to old ID → no response (correct)

### Master ID Validation Test

1. `m 01` → ensure sender matches default master
2. `1` → Relay A ON → ACK received ✅ (authorized)
3. `m 05` → switch to wrong sender
4. `1` → Relay A ON → **no response** ❌ (rejected by PIC)
5. `m 01` → switch back to correct sender
6. `1` → ACK received again ✅

### Packet Format

**Command Packet** (ESP32 → PIC) — Example: Relay B ON for PIC 1
```
AA  FE  00  02  00  02  FE  BB
│   │   │   │   │   │   │   └── EOF (End of Frame)
│   │   │   │   │   │   └────── CRC = FE ^ 00 ^ 02 ^ 00 ^ 02
│   │   │   │   │   └────────── Data_L = 0x02 (Socket B)
│   │   │   │   └────────────── Data_H = 0x00 (unused)
│   │   │   └────────────────── Command = 0x02 (CMD_RELAY_ON)
│   │   └────────────────────── Sender = 0x00 (ESP32 / Master)
│   └────────────────────────── Target = 0xFE (PIC 1)
└────────────────────────────── SOF (Start of Frame)
```

**ACK Response** (PIC → ESP32) — Example: PIC 1 confirms Relay B ON
```
AA  00  FE  06  02  02  FA  BB
│   │   │   │   │   │   │   └── EOF (End of Frame)
│   │   │   │   │   │   └────── CRC = 00 ^ FE ^ 06 ^ 02 ^ 02
│   │   │   │   │   └────────── Data_L = 0x02 (CMD_RELAY_ON echoed)
│   │   │   │   └────────────── Data_H = 0x02 (Socket B)
│   │   │   └────────────────── Command = 0x06 (CMD_ACK)
│   │   └────────────────────── Sender = 0xFE (PIC 1)
│   └────────────────────────── Target = 0x00 (ESP32 / Master)
└────────────────────────────── SOF (Start of Frame)
```

**CRC** = XOR of bytes 1-5 (TARGET ^ SENDER ^ CMD ^ DATA_H ^ DATA_L)

### Command Codes

| Code     | Name                  | Data                     |
| :------- | :-------------------- | :----------------------- |
| `0x02` | `CMD_RELAY_ON`      | Socket ID (01=A, 02=B)   |
| `0x03` | `CMD_RELAY_OFF`     | Socket ID (01=A, 02=B)   |
| `0x04` | `CMD_READ_CURRENT`  | Unused (0x0000)          |
| `0x05` | `CMD_REPORT_DATA`   | Current in mA (response) |
| `0x06` | `CMD_ACK`           | Socket + Command echoed  |
| `0x07` | `CMD_SET_THRESHOLD` | Threshold in mA          |
| `0x08` | `CMD_SET_DEVICE_ID` | New Device ID (data_l)   |
| `0x09` | `CMD_SET_ID_MASTER` | New Master ID (data_l)   |

### Raw Hex Reference

For manual testing or debugging, raw hex packets can still be pasted directly:

**PIC 1 (0xFE):** `d FE` then use keys 1-8, or paste raw:

| Action               | Hex Packet                  |
| :------------------- | :-------------------------- |
| Relay A ON           | `AA FE 00 02 00 01 FD BB` |
| Relay A OFF          | `AA FE 00 03 00 01 FC BB` |
| Relay B ON           | `AA FE 00 02 00 02 FE BB` |
| Relay B OFF          | `AA FE 00 03 00 02 FF BB` |
| Read Sensors         | `AA FE 00 04 00 00 FA BB` |
| Set Threshold 3233mA | `AA FE 00 07 0C A1 54 BB` |

---

## 3. Overload Protection Test

1. Set a low threshold via `CMD_SET_THRESHOLD`
2. Connect a load exceeding the threshold
3. **Expected:** Relay trips OFF within 200ms
4. **After 5 seconds:** Auto-retry (relay ON)
5. If still overloaded → trips again immediately
6. During overload: `CMD_RELAY_ON` is blocked

---

## 4. EEPROM Map

| Addr     | Value               | Default  |
| :------- | :------------------ | :------- |
| `0x00` | Threshold high byte | `0x0B` |
| `0x01` | Threshold low byte  | `0xB8` |
| `0x02` | DEVICE_ID           | `0x01` |
| `0x03` | ID_MASTER           | `0x01` |
