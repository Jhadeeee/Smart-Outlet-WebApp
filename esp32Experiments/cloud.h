/*
 * cloud.h — WiFi & Cloud Logging
 * 
 * Connects ESP32 to WiFi and sends HTTP POST requests
 * to the Django backend for event logging and sensor data.
 */

#ifndef CLOUD_H
#define CLOUD_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ============ CONFIGURATION ============
// Fill in your credentials below:

const char* WIFI_SSID     = "infinixty";
const char* WIFI_PASSWORD  = "defaultpass";
const char* SERVER_URL     = "http://10.221.26.107:8000";  // Django server (e.g. http://192.168.1.100:8000)

// ============ WiFi CONNECTION ============

void connectWiFi() {
  Serial.println();
  Serial.print("🌐 Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 40) {  // 20 second timeout
      Serial.println();
      Serial.println("❌ WiFi connection FAILED after 20s!");
      Serial.println("   Check SSID/password in cloud.h");
      Serial.println("   Continuing in OFFLINE mode (no cloud logging)");
      return;
    }
  }
  
  Serial.println();
  Serial.println("✅ WiFi connected!");
  Serial.println("   IP: " + WiFi.localIP().toString());
  Serial.println("   Server: " + String(SERVER_URL));
  Serial.println();
}

// Helper: check if WiFi is connected
bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// ============ POST EVENT LOG ============

void postEventLog(String deviceId, String eventType, String severity, float currentMA, String socketLabel) {
  if (!isWiFiConnected()) {
    Serial.println("☁️  [OFFLINE] Event not sent: " + eventType);
    return;
  }

  HTTPClient http;
  String url = String(SERVER_URL) + "/api/event-log/";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);  // 5 second timeout
  
  // Build JSON payload
  String json = "{";
  json += "\"device_id\":\"" + deviceId + "\",";
  json += "\"event_type\":\"" + eventType + "\",";
  json += "\"severity\":\"" + severity + "\",";
  json += "\"socket_label\":\"" + socketLabel + "\",";
  json += "\"message\":\"" + eventType + " detected. Current: " + String(currentMA, 0) + " mA\",";
  json += "\"current_reading\":" + String(currentMA, 1);
  json += "}";
  
  Serial.println("☁️  POST " + url);
  Serial.println("   📦 " + json);
  
  int httpCode = http.POST(json);
  
  if (httpCode > 0) {
    String response = http.getString();
    if (httpCode == 200) {
      Serial.println("   ✅ Event logged! (HTTP " + String(httpCode) + ")");
    } else {
      Serial.println("   ⚠️ Server responded HTTP " + String(httpCode));
      Serial.println("   📄 " + response);
    }
  } else {
    Serial.println("   ❌ POST failed: " + http.errorToString(httpCode));
  }
  
  http.end();
}

// ============ POST SENSOR DATA ============

void postSensorData(String deviceId, char socket, float mA) {
  if (!isWiFiConnected()) {
    return;  // Silently skip — sensor data is high-frequency
  }

  HTTPClient http;
  String url = String(SERVER_URL) + "/api/sensor-data/";
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);  // 3 second timeout (shorter for frequent data)
  
  // Convert mA to Amps for the existing API
  float amps = mA / 1000.0;
  
  // Build JSON payload matching existing receive_sensor_data API format
  String json = "{";
  json += "\"device_id\":\"" + deviceId + "\",";
  json += "\"voltage\":220.0,";                    // Placeholder — no voltage sensor yet
  json += "\"current\":" + String(amps, 3) + ",";  // Convert mA to A
  json += "\"power\":" + String(220.0 * amps, 1);  // Estimated power (V*I)
  json += "}";
  
  int httpCode = http.POST(json);
  
  if (httpCode == 200) {
    Serial.println("☁️  Sensor data sent ✓");
  } else if (httpCode > 0) {
    Serial.println("☁️  Sensor POST HTTP " + String(httpCode));
  } else {
    Serial.println("☁️  Sensor POST failed: " + http.errorToString(httpCode));
  }
  
  http.end();
}

#endif
