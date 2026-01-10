#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include <Adafruit_AS7341.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "ota.h"

Adafruit_AS7341 as7341;
WiFiClient wifiClient;
PubSubClient natsClient(wifiClient);

static const char* hostname = ENV_HOSTNAME;
const char* ota_password = ENV_OTA_PASSWORD;
static const char* wifi_ssid = ENV_WIFI_SSID;
static const char* wifi_password = ENV_WIFI_PASSWORD;
static const char* nats_username = ENV_NATS_MQTT_USERNAME;
static const char* nats_password = ENV_NATS_MQTT_PASSWORD;
static const char* nats_host = "nats.local";
static const int nats_port = 1883;
static const char* nats_subject = "ingress/growtent/spectrometer/readings";
static const char* ntp_server = "pool.ntp.org";
static const long gmt_offset_sec = 0;
static const int daylight_offset_sec = 0;

struct SensitivityFactors {
  float channel_415nm = 100.0;
  float channel_445nm = 120.0;
  float channel_480nm = 150.0;
  float channel_515nm = 180.0;
  float channel_555nm = 200.0;
  float channel_590nm = 190.0;
  float channel_630nm = 170.0;
  float channel_680nm = 140.0;
  float channel_clear = 1000.0;
  float channel_nir = 1000.0;
} sensitivity;

struct SpectralData {
  uint16_t channel_415nm;
  uint16_t channel_445nm;
  uint16_t channel_480nm;
  uint16_t channel_515nm;
  uint16_t channel_555nm;
  uint16_t channel_590nm;
  uint16_t channel_630nm;
  uint16_t channel_680nm;
  uint16_t channel_clear;
  uint16_t channel_nir;
  uint16_t ppfd;
};

void setupWifi() {
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(wifi_ssid, wifi_password);

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

String getISOTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00Z";
  }
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buffer);
}

void setupSensor() {
  if (!as7341.begin()) {
    Serial.println("Could not find AS7341 sensor, check wiring!");
    Serial.println("Restarting in 3 seconds...");
    delay(3000);
    ESP.restart();
  }
  
  as7341.setATIME(100);
  as7341.setASTEP(999);
  as7341.setGain(AS7341_GAIN_256X);
  
  Serial.println("AS7341 sensor initialized successfully!");
}

void setupNATS() {
  natsClient.setServer(nats_host, nats_port);
  natsClient.setBufferSize(512);
  natsClient.setKeepAlive(60);
  
  while (!natsClient.connected()) {
    Serial.println("Attempting NATS connection...");
  
    if (natsClient.connect(hostname, nats_username, nats_password)) {
      Serial.println("Connected to NATS server");
      Serial.printf("NATS Host: %s:%d\n", nats_host, nats_port);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(natsClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

String spectralDataToJson(const SpectralData& data) {
  JsonDocument doc;
  
  doc["device"] = hostname;
  doc["timestamp"] = getISOTimestamp();
  doc["channels"]["415nm"] = data.channel_415nm;
  doc["channels"]["445nm"] = data.channel_445nm;
  doc["channels"]["480nm"] = data.channel_480nm;
  doc["channels"]["515nm"] = data.channel_515nm;
  doc["channels"]["555nm"] = data.channel_555nm;
  doc["channels"]["590nm"] = data.channel_590nm;
  doc["channels"]["630nm"] = data.channel_630nm;
  doc["channels"]["680nm"] = data.channel_680nm;
  doc["channels"]["clear"] = data.channel_clear;
  doc["channels"]["nir"] = data.channel_nir;
  doc["ppfd"] = data.ppfd;
  
  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

SpectralData getSpectralData() {
  SpectralData data;
  uint16_t readings[12];
  
  if (!as7341.readAllChannels(readings)) {
    Serial.println("Error reading from sensor");
    return data;
  }
  
  data.channel_415nm = readings[0];
  data.channel_445nm = readings[1];
  data.channel_480nm = readings[2];
  data.channel_515nm = readings[3];
  data.channel_555nm = readings[4];
  data.channel_590nm = readings[5];
  data.channel_630nm = readings[6];
  data.channel_680nm = readings[7];
  data.channel_clear = readings[8];
  data.channel_nir = readings[9];
  
  float ppfd = 0.0;
  ppfd += data.channel_415nm / sensitivity.channel_415nm;
  ppfd += data.channel_445nm / sensitivity.channel_445nm;
  ppfd += data.channel_480nm / sensitivity.channel_480nm;
  ppfd += data.channel_515nm / sensitivity.channel_515nm;
  ppfd += data.channel_555nm / sensitivity.channel_555nm;
  ppfd += data.channel_590nm / sensitivity.channel_590nm;
  ppfd += data.channel_630nm / sensitivity.channel_630nm;
  ppfd += data.channel_680nm / sensitivity.channel_680nm;
  
  data.ppfd = (uint16_t)ceil(ppfd);
  return data;
}

void setup(){
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32 WiFi Spectrometer with AS7341");
  
  setupWifi();
  
  configTime(gmt_offset_sec, daylight_offset_sec, ntp_server);
  Serial.println("Synchronizing time with NTP server...");
  
  setupOTA(hostname);
  setupSensor();
  setupNATS();
  
  Serial.println("Setup complete!");
}

void loop(){
  static unsigned long lastReading = 0;
  unsigned long currentTime = millis();
  
  ArduinoOTA.handle();
  
  if (!natsClient.connected()) {
    setupNATS();
  }
  natsClient.loop();
  
  if (currentTime - lastReading >= 5000) {
    SpectralData data = getSpectralData();
    
    String jsonData = spectralDataToJson(data);
    bool published = natsClient.publish(nats_subject, jsonData.c_str());
    
    if (published) {
      Serial.println("Published data to NATS");
    } else {
      Serial.println("Failed publishing to NATS");
    }
    
    Serial.println("=== Spectral Reading ===");
    Serial.printf("415nm (F1): %d\n", data.channel_415nm);
    Serial.printf("445nm (F2): %d\n", data.channel_445nm);
    Serial.printf("480nm (F3): %d\n", data.channel_480nm);
    Serial.printf("515nm (F4): %d\n", data.channel_515nm);
    Serial.printf("555nm (F5): %d\n", data.channel_555nm);
    Serial.printf("590nm (F6): %d\n", data.channel_590nm);
    Serial.printf("630nm (F7): %d\n", data.channel_630nm);
    Serial.printf("680nm (F8): %d\n", data.channel_680nm);
    Serial.printf("Clear: %d\n", data.channel_clear);
    Serial.printf("NIR: %d\n", data.channel_nir);
    Serial.printf("PPFD: %d µmol/m²/s\n", data.ppfd);
    Serial.println("========================");
    
    lastReading = currentTime;
  }
  
  delay(100);
}