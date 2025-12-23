#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_AS7341.h>

Adafruit_AS7341 as7341;

static const char* ssid = ENV_WIFI_NAME;
static const char* password = ENV_WIFI_PASS;
static const char* hostname = "esp32-wifi-spectrometer-01";

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
  float ppfd;
};

void setupWifi() {
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("ESP32 HostName: ");
  Serial.println(WiFi.getHostname());
  Serial.println("RSSI: ");
  Serial.println(WiFi.RSSI());
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
  
  data.ppfd = ppfd;
  return data;
}

void setup(){
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("ESP32 WiFi Spectrometer with AS7341");
  
  setupWifi();
  setupSensor();
  
  Serial.println("Setup complete!");
}

void loop(){
  static unsigned long lastReading = 0;
  unsigned long currentTime = millis();
  
  if (currentTime - lastReading >= 5000) {
    SpectralData data = getSpectralData();
    
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
    Serial.printf("PPFD: %.2f µmol/m²/s\n", data.ppfd);
    Serial.println("========================");
    
    lastReading = currentTime;
  }
  
  delay(100);
}