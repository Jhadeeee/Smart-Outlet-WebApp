# SmartOutlet Firmware — Developer Documentation

**MCU:** PIC16F88 · **Compiler:** XC8 · **IDE:** MPLAB X  
**Firmware:** v5.3.1 · **Flash:** ~99% (4073 / 4096 words)

---

## Architecture Overview

```
┌──────────────────────────────────────────────────┐
│                   PIC16F88                       │
│                                                  │
│  ┌──────────┐   ┌───────────┐   ┌─────────────┐  │
│  │ HC-12 RF │◄──│ Hard UART │──►│ RX State    │  │
│  │ Module   │   │ (RB2/RB5) │   │ Machine     │  │
│  └──────────┘   └───────────┘   └───────┬─────┘  │
│                                         │        │
│                                ┌────────▼──────┐ │
│                                │Process_Command│ │
│                                └───┬───┬───┬───┘ │
│                     ┌──────────────┘   │   └─────┤
│               ┌─────▼──────┐  ┌────────▼──────┐  │
│               │ Relay Ctrl │  │ Sensor Read   │  │
│               │ RA2 / RA3  │  │ ACS712 x2     │  │
│               └────────────┘  │ AN0 / AN1     │  │
│                               └───────────────┘  │
│  ┌──────────┐   ┌───────────┐                    │
│  │ SoftUART │◄──│ Debug Out │  RB6(TX) / RB7(RX) │
│  │ (Debug)  │   │ "R1+" etc │                    │
│  └──────────┘   └───────────┘                    │
│                                                  │
│  RB3 = Config Button     RB4 = Status LED        │
└──────────────────────────────────────────────────┘
```

---

## Pin Map

| Pin  | Function       | Direction | Notes                    |
|:-----|:---------------|:----------|:-------------------------|
| RA0  | ACS712 (A)     | Input     | AN0 — Socket A sensor   |
| RA1  | ACS712 (B)     | Input     | AN1 — Socket B sensor   |
| RA2  | Relay B        | Output    | Active LOW (NC wiring)   |
| RA3  | Relay A        | Output    | Active LOW (NC wiring)   |
| RB2  | UART RX        | Input     | HC-12 data in            |
| RB3  | Config Button  | Input     | Active LOW, internal pullup |
| RB4  | Status LED     | Output    | LOW = defaults, HIGH = configured |
| RB5  | UART TX        | Output    | HC-12 data out           |
| RB6  | Soft UART TX   | Output    | Debug serial out         |
| RB7  | Soft UART RX   | Input     | Debug serial in (simulation) |

---

## Relay Wiring: NC (Normally Closed)

The relays use **NC wiring**, which reverses the logic:

| Pin State | Relay Coil | NC Contact | Appliance |
|:----------|:-----------|:-----------|:----------|
| `0` (LOW) | Energized  | **OPEN**   | **OFF**   |
| `1` (HIGH)| De-energized| **CLOSED** | **ON**    |

> **Why NC?** Safety — if the PIC loses power or resets, the relay de-energizes, NC closes, and the appliance stays ON (fail-safe for essential loads).

---

## Boot Sequence

```
1. OSCCON = 8MHz, wait for stable
2. Pin setup (ANSEL, TRISA, TRISB)
3. Relays OFF (RA2=0, RA3=0 → energized → NC open)
4. Init: UART, SoftUART, Timer, ADC
5. Init: ACS712 sensors (AN0, AN1)
6. EEPROM load: threshold, device_id, id_master
7. Update CFG_LED based on is_configured()
8. SoftUART: "v5.3.1" → "Cal" → calibrate sensors → "Rdy"
9. Enter main loop
```

---

## Main Loop

The main loop runs **three tasks** in priority order:

### 1. UART Command Processing (Highest Priority)

Receives 8-byte packets from HC-12 via hardware UART:

```
[SOF 0xAA] [TARGET] [SENDER] [CMD] [DATA_H] [DATA_L] [CRC] [EOF 0xBB]
```

- Waits for `0xAA` sync byte
- Fills `rx_pkt.frame[]` byte by byte
- On 8 bytes: verify CRC → `Process_Command()`
- `Process_Command` validates **both** `target_id == device_id` AND `sender_id == id_master`
- Packets from unauthorized senders are silently dropped
- Invalid CRC prints `"CRC!"` on SoftUART

### 2. Button Handler (Config Mode + Factory Reset)

**Config Mode:**
- Hold RB3 LOW for 3 seconds → `config_mode = 1`, prints `"Cfg!"`
- Required for `CMD_SET_DEVICE_ID` and `CMD_SET_ID_MASTER`
- Auto-deactivates after a successful SET command

**Factory Reset:**
- Press RB3 3 times quickly (50ms debounce)
- Only counts presses when NOT in config mode
- Resets all EEPROM values to defaults, sets `CFG_LED = 0`

> **Recovery:** If master ID is misconfigured (no ESP32 can authenticate), factory reset via RB3 ×3 is the **only** way to recover.

### 3. Overload Protection (Every 200ms)

- Reads ACS712 current for each socket
- If current > `overload_threshold_ma` → trip relay OFF
- After 5-second cooldown → auto-retry (relay ON)
- During overload, `CMD_RELAY_ON` is still ACK'd but relay stays OFF

---

## Command Reference

| Code   | Name                | Data_H         | Data_L         | Needs Config Mode |
|:-------|:--------------------|:---------------|:---------------|:------------------|
| `0x01` | `CMD_PING`        | —              | —              | No                |
| `0x02` | `CMD_RELAY_ON`    | —              | Socket (1/2)   | No                |
| `0x03` | `CMD_RELAY_OFF`   | —              | Socket (1/2)   | No                |
| `0x04` | `CMD_READ_CURRENT`| —              | —              | No                |
| `0x05` | `CMD_REPORT_DATA` | Current mA (H) | Current mA (L) | — (response only) |
| `0x06` | `CMD_ACK`         | Socket         | CMD echoed     | — (response only) |
| `0x07` | `CMD_SET_THRESHOLD`| mA (H)        | mA (L)         | No                |
| `0x08` | `CMD_SET_DEVICE_ID`| —             | New ID         | **Yes**           |
| `0x09` | `CMD_SET_ID_MASTER`| —             | New ID         | **Yes**           |

---

## EEPROM Map

| Address | Content             | Default  | Notes                    |
|:--------|:--------------------|:---------|:-------------------------|
| `0x00`  | Threshold high byte | `0x0B`   | 3000 = `0x0BB8`          |
| `0x01`  | Threshold low byte  | `0xB8`   |                          |
| `0x02`  | Device ID           | `0x01`   | `0xFF` = uninitialized   |
| `0x03`  | Master ID           | `0x01`   | `0xFF` = uninitialized   |

> **Guard:** On boot, `0xFF` means EEPROM was never written — the code keeps the compile-time default instead.

---

## Status LED (RB4) Logic

`is_configured()` returns true only when **ALL THREE** values differ from defaults:

```c
return (device_id != 0x01 &&
        id_master != 0x01 &&
        overload_threshold_ma != 3000);
```

| RB4   | Meaning                                             |
|:------|:----------------------------------------------------|
| LOW   | At least one value is still at its default          |
| HIGH  | All values (device ID, master ID, threshold) changed|

---

## ACK Packet Structure

When the PIC processes a command, it responds with an ACK:

```
ACK.target_id = sender who sent the command
ACK.sender_id = device_id (this PIC)
ACK.command   = CMD_ACK (0x06)
ACK.data_h    = socket ID (for relay commands) or 0x00
ACK.data_l    = original command code (echoed back)
```

---

## Library API Reference

### UART_Lib.h — Hardware UART (HC-12 Communication)

| Function | Description |
|:---------|:------------|
| `void UART_Init()` | Initialize UART at 9600 baud (hardcoded for 8MHz). Sets up TXSTA/RCSTA registers. |
| `void UART_Write(char data)` | Send one byte over hardware UART. Blocks until TX buffer is free. |
| `char UART_Read()` | Read one byte from RX buffer. **Blocking** — waits until data is available. |
| `unsigned char UART_Data_Ready()` | Returns 1 if there is unread data in the RX buffer, 0 otherwise. Non-blocking. |
| `void UART_Write_Text(char *text)` | Send a null-terminated string over hardware UART. |
| `void UART_ISR(void)` | **Call from main ISR.** Captures incoming UART bytes into the ring buffer. Highest priority. |

---

### Soft_UART.h — Software UART (Debug Output)

| Function | Description |
|:---------|:------------|
| `void Soft_UART_Init(volatile unsigned char *port, unsigned char rx_pin, unsigned char tx_pin, unsigned long baud_rate, unsigned char inverted)` | Initialize software UART. Uses Timer2 for bit-bang timing. `port` = `&PORTB`, `inverted` = 0 for normal logic. |
| `void Soft_UART_Write(char data)` | Queue one character for transmission. Non-blocking — the ISR handles actual bit-banging. |
| `void Soft_UART_print(char *text)` | Queue a null-terminated string for transmission. |
| `void Soft_UART_println(char *text)` | Queue a string followed by `\r\n`. |
| `void Soft_UART_ISR(void)` | **Call from main ISR.** Handles Timer2-driven bit-bang transmission. |

**Pins used in firmware:** RB6 (TX), RB7 (RX), 9600 baud, non-inverted.

---

### Timer_lib.h — Millisecond Timer

| Function | Description |
|:---------|:------------|
| `void Time_Init(unsigned char mhz)` | Initialize Timer0 for timekeeping. Pass the oscillator frequency: `8` for 8MHz. |
| `void Timer_ISR(void)` | **Call from main ISR.** Increments the internal millisecond counter on each Timer0 overflow. |
| `unsigned long millis()` | Returns milliseconds elapsed since boot. Used for debounce, cooldown, and sensor check timing. |
| `unsigned long micros()` | Returns microseconds elapsed since boot. Higher resolution but rolls over faster. |

---

### ADC_Lib.h — Analog-to-Digital Converter

| Function | Description |
|:---------|:------------|
| `void ADC_Init()` | Initialize the ADC module. Configures ADCON0/ADCON1 for the PIC16F88. |
| `unsigned int ADC_Read(unsigned char channel)` | Read the ADC value from the specified channel (0-7). Returns 10-bit result (0-1023). Blocking. |

---

### ACS712.h — Current Sensor Driver

```c
typedef struct {
    unsigned char adc_channel;         // ADC pin (0-7)
    unsigned int  voltage_reference_mv; // Vref in mV (e.g. 5000)
    int           adc_resolution;       // e.g. 1023 (10-bit)
    unsigned int  zero_point;           // Calibrated zero (ADC value at 0A)
    unsigned int  sensitivity_mV_A;     // mV per Amp (100 for 20A module)
} ACS712_t;
```

| Function | Description |
|:---------|:------------|
| `void ACS712_Init(ACS712_t* sensor, unsigned char channel, unsigned int v_ref_mv, int adc_res)` | Initialize a sensor instance. Sets ADC channel, voltage reference, and resolution. Does NOT calibrate. |
| `void ACS712_SetSensitivity(ACS712_t* sensor, unsigned int sens_mv_a)` | Set the sensitivity in mV/A. **100** for ACS712-20A, 185 for 5A, 66 for 30A. |
| `void ACS712_Calibrate(ACS712_t* sensor)` | Sample the ADC at zero load to determine the quiescent output voltage (zero point). **Call with no current flowing.** |
| `unsigned int ACS712_ReadAC(ACS712_t* sensor, unsigned char frequency)` | Read AC RMS current. `frequency` = line frequency in Hz (e.g. 60). Returns current in **milliamps**. Blocking (~17ms per call at 60Hz). |

---

### HC12-RF_Protocol.h — RF Communication Protocol

```c
typedef union {
    struct {
        unsigned char sof;        // 0xAA (Start of Frame)
        unsigned char target_id;  // Destination device
        unsigned char sender_id;  // Source device
        unsigned char command;    // Command code
        unsigned char data_h;     // Data high byte
        unsigned char data_l;     // Data low byte
        unsigned char checksum;   // XOR of bytes 1-5
        unsigned char eof;        // 0xBB (End of Frame)
    } fields;
    unsigned char frame[8];       // Raw byte array view
} RF_Packet_t;
```

| Function / Macro | Description |
|:-----------------|:------------|
| `void RF_Init_Packet(RF_Packet_t *pkt)` | Zero all fields and set `sof = 0xAA`, `eof = 0xBB`. Always call before building a packet. |
| `RF_Set_Data(pkt, value)` | **Macro.** Splits a 16-bit value into `data_h` and `data_l`. Zero overhead (no function call). |
| `RF_Get_Data(pkt)` | **Macro.** Reconstructs a 16-bit value from `data_h` and `data_l`. |
| `void RF_Sign_Packet(RF_Packet_t *pkt)` | Compute CRC (XOR of bytes 1-5) and store in `checksum`. **Call before sending.** |
| `unsigned char RF_Verify_Packet(RF_Packet_t *pkt)` | Verify SOF, EOF, and CRC. Returns `1` if valid, `0` if corrupted. |

---

## File Structure

| File                    | Purpose                                   |
|:------------------------|:------------------------------------------|
| `__main__.c`            | Main firmware — all application logic     |
| `HC12-RF_Protocol.h/c`  | Packet structure, CRC, init/sign/verify  |
| `ACS712.h/c`            | Current sensor driver (init, calibrate, read AC) |
| `UART_Lib.h`            | Hardware UART (HC-12 communication)       |
| `Soft_UART.h`           | Software UART (debug output on RB6/RB7)  |
| `Timer_lib.h`           | Timer0 ISR for `millis()` timestamps      |
| `ADC_Lib.h`             | ADC initialization and read functions     |

---

## Function Reference

| Function                    | Description                                      |
|:----------------------------|:-------------------------------------------------|
| `main()`                    | Init hardware, EEPROM load, main loop            |
| `__interrupt() ISR()`       | Dispatches UART_ISR, Timer_ISR, Soft_UART_ISR    |
| `Process_Command(pkt)`      | Validates target + sender, routes to handler      |
| `Send_ACK(target, cmd, socket)` | Builds and sends 8-byte ACK packet           |
| `Perform_Read_And_Report(sender)` | Reads both ACS712 sensors, sends 2 data packets |
| `Process_Debug_Shortcut(key)` | Simulation mode: builds mock packets from keys 1-8 |
| `is_configured()`           | Returns 1 if all 3 values differ from defaults   |
| `print_int_to_uart(val, is_soft)` | Integer-to-ASCII print for either UART      |

---

## ISR Priority

```
void __interrupt() ISR(void) {
    UART_ISR();      // 1st — capture HC-12 bytes (highest priority)
    Timer_ISR();     // 2nd — millis() counter
    Soft_UART_ISR(); // 3rd — software serial TX bit-bang
}
```

---

## Memory Constraints

**Production Build** (simulation mode commented out):

| Resource    | Used         | Free      | Utilization |
|:------------|:-------------|:----------|:------------|
| Program     | 3,898 words  | 198 words | **95%**     |
| Data        | 234 bytes    | 134 bytes | **64%**     |

**With Simulation Mode** (debug keys 1-8 enabled):

| Resource    | Used         | Free      | Utilization |
|:------------|:-------------|:----------|:------------|
| Program     | ~4,073 words | ~23 words | **99%**     |
| Data        | ~234 bytes   | ~134 bytes| **64%**     |

> **Note:** `Process_Debug_Shortcut()` adds ~175 words. The XC8 compiler eliminates it as dead code when commented out.

> **Warning:** The firmware is at the Flash limit. Any new feature must remove an equivalent amount of code. Strategies used:
> - Shortened debug strings (`"Cfg!"` instead of `"Config Mode!"`)
> - Removed visual animations (RB4 toggle during reset)
> - Combined switch fall-through for keys 7/8
> - Used `#define` macros for zero-overhead data access

---

## Simulation Mode

For Proteus testing, uncomment lines 159-162 in `__main__.c`:

```c
if (byte >= '1' && byte <= '8') {
    Process_Debug_Shortcut(byte);
    continue;
}
```

This intercepts UART bytes as keyboard shortcuts instead of protocol packets.

---

## Common Debug Strings (SoftUART)

| String      | Meaning                                 |
|:------------|:----------------------------------------|
| `v5.3.1`    | Firmware booted                         |
| `Cal`       | Sensor calibration in progress          |
| `Rdy`       | System ready                            |
| `R1+` / `R1-` | Relay A ON / OFF                     |
| `R2+` / `R2-` | Relay B ON / OFF                     |
| `Cfg!`      | Config mode activated                   |
| `Cfg?`      | Config mode required but not active     |
| `CRC!`      | Packet failed CRC verification          |
| `A:OVL`     | Socket A is in overload state           |
| `B:OVL`     | Socket B is in overload state           |
| `A:Rty`     | Socket A overload cooldown retry        |
| `B:Rty`     | Socket B overload cooldown retry        |
