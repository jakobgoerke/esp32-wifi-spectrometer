#include <Arduino.h>
#include <WiFi.h>
#include "wifi.h"

void setupWifi(const char* hostname, const char* ssid, const char* password) {
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.print("\nESP32 IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("ESP32 HostName: ");
  Serial.println(WiFi.getHostname());
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());
}

bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  Serial.println("WiFi disconnected, reconnecting...");
  WiFi.disconnect();
  WiFi.begin();
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi reconnection failed, retrying next loop");
    return false;
  }
  Serial.println("WiFi reconnected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  return true;
}
