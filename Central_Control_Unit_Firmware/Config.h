/*
 * Config.h
 * --------
 * Global configuration constants for the CCU Firmware v4.0.0.
 * Modify these values to match your hardware and preferences.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ─── Access Point (Hotspot) Settings ────────────────────────
#define AP_SSID         "CCU-Setup"       // Name of the setup hotspot
#define AP_PASSWORD     ""                // Open network (no password)
#define AP_IP           IPAddress(192, 168, 4, 1)
#define AP_GATEWAY      IPAddress(192, 168, 4, 1)
#define AP_SUBNET       IPAddress(255, 255, 255, 0)

// ─── WiFi Connection Settings ───────────────────────────────
#define WIFI_CONNECT_TIMEOUT_MS   15000   // 15 seconds to attempt WiFi connection
#define WIFI_RETRY_DELAY_MS       500     // Delay between connection checks

// ─── Web Server ─────────────────────────────────────────────
#define WEB_SERVER_PORT  80

// ─── NVS Storage Keys ───────────────────────────────────────
#define NVS_NAMESPACE    "ccu-config"
#define NVS_KEY_SSID     "ssid"
#define NVS_KEY_PASSWORD "password"
#define NVS_KEY_SERVER   "serverUrl"

// ─── Hardware Pins ──────────────────────────────────────────
#define LED_PIN          2                // Built-in LED on most ESP32 boards
#define RESET_BTN_PIN    0                // BOOT button for factory reset

// ─── HC-12 RF Module ───────────────────────────────────────
#define HC12_RX_PIN      16               // ESP32 GPIO 16 ← HC-12 TX
#define HC12_TX_PIN      17               // ESP32 GPIO 17 → HC-12 RX
#define HC12_BAUD        9600             // HC-12 default baud rate

// ─── RF Protocol ────────────────────────────────────────────
#define RF_SOF           0xAA             // Start of Frame byte
#define RF_EOF           0xBB             // End of Frame byte
#define RF_PACKET_SIZE   8                // Fixed 8-byte packet size
#define CCU_SENDER_ID    0x01             // Must match PIC's DEFAULT_ID_MASTER
#define MAX_OUTLETS      8                // Maximum number of smart outlets

// ─── Breaker Monitor (SCT013) ───────────────────────────────
#define BREAKER_ADC_PIN              34   // ESP32 ADC pin (input-only, no pull-up)
#define BREAKER_CT_TURNS             2000 // SCT013-100 turns ratio
#define BREAKER_BURDEN_RESISTOR      23   // Burden resistor in Ohms
#define BREAKER_LINE_FREQ            60   // Mains frequency (50 or 60 Hz)
#define BREAKER_DEFAULT_THRESHOLD_MA 15000 // Default overload threshold (15A)

// SCTSensor Calculation Constants (used by SCTSensor.cpp)
#define SCT_ADC_PIN                  BREAKER_ADC_PIN
#define SCT_SAMPLES                  500   // Number of ADC samples per cycle
#define SCT_ADC_RESOLUTION           4095.0 // 12-bit ADC (2^12 - 1)
#define SCT_VREF                     3.3    // ESP32 Operating Voltage
#define SCT_ADC_MIDPOINT             (SCT_ADC_RESOLUTION / 2.0) // ~1.65V DC offset
// Calibration = (CT_TURNS / BURDEN_RESISTOR)
#define SCT_CALIBRATION              ((double)BREAKER_CT_TURNS / BREAKER_BURDEN_RESISTOR)

// ─── Serial ─────────────────────────────────────────────────
#define SERIAL_BAUD      115200

// ─── Cloud / Server ─────────────────────────────────────────
#define CLOUD_SEND_INTERVAL_MS  2000      // How often to send data to server
#define HTTP_TIMEOUT_MS         5000      // HTTP request timeout

#endif // CONFIG_H
