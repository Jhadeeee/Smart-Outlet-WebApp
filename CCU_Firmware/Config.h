/*
 * Config.h
 * --------
 * Global configuration constants for the CCU Firmware.
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

// ─── Serial ─────────────────────────────────────────────────
#define SERIAL_BAUD      115200

// ─── Cloud / Server ─────────────────────────────────────────
#define CLOUD_SEND_INTERVAL_MS  10000     // How often to send data to server
#define HTTP_TIMEOUT_MS         5000      // HTTP request timeout

#endif // CONFIG_H
