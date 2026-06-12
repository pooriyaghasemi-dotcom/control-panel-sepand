#include <WiFi.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ============ تنظیمات برد ============
#define BOARD_ID 2 // دفتر
#define BOARD_NAME "Office Client"
#define SERVER_IP "192.168.1.100"
#define SERVER_PORT 80

// ============ تنظیمات WiFi ============
const char* AP_SSID = "ESP32_OFFICE_AP";
const char* AP_PASSWORD = "12345678";

char wifi_ssid[32] = "";
char wifi_password[64] = "";
char device_static_ip[16] = "192.168.1.101";
char device_gateway[16] = "192.168.1.1";
char device_subnet[16] = "255.255.255.0";

// ============ تنظیمات سنسورها و رله ============
#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

#define DOOR_SWITCH_PIN 5
const int RELAY_PINS[4] = {12, 13, 14, 15};

// ============ تنظیمات EEPROM ============
#define EEPROM_SIZE 2048
#define WIFI_CONFIG_ADDR 0
#define RELAY_CONFIG_ADDR 256

struct RelayConfig {
  char name[32];
  bool active;
  int trigger_time;
};

RelayConfig relays[4];
bool AP_MODE = true;
unsigned long last_sync = 0;
const unsigned long SYNC_INTERVAL = 5000; // هر 5 ثانیه

// ============ توابع EEPROM ============
void loadEEPROMData() {
  EEPROM.begin(EEPROM_SIZE);
  
  EEPROM.readString(WIFI_CONFIG_ADDR, wifi_ssid, 32);
  EEPROM.readString(WIFI_CONFIG_ADDR + 32, wifi_password, 64);
  EEPROM.readString(WIFI_CONFIG_ADDR + 96, device_static_ip, 16);
  
  for (int i = 0; i < 4; i++) {
    int addr = RELAY_CONFIG_ADDR + (i * 64);
    EEPROM.readString(addr, relays[i].name, 32);
    relays[i].active = EEPROM.read(addr + 32);
    relays[i].trigger_time = EEPROM.readInt(addr + 33);
    
    if (strlen(relays[i].name) == 0) {
      sprintf(relays[i].name, "Relay %d", i + 1);
    }
  }
}

void saveEEPROMData() {
  EEPROM.begin(EEPROM_SIZE);
  
  EEPROM.writeString(WIFI_CONFIG_ADDR, wifi_ssid);
  EEPROM.writeString(WIFI_CONFIG_ADDR + 32, wifi_password);
  EEPROM.writeString(WIFI_CONFIG_ADDR + 96, device_static_ip);
  
  for (int i = 0; i < 4; i++) {
    int addr = RELAY_CONFIG_ADDR + (i * 64);
    EEPROM.writeString(addr, relays[i].name);
    EEPROM.write(addr + 32, relays[i].active);
    EEPROM.writeInt(addr + 33, relays[i].trigger_time);
  }
  
  EEPROM.commit();
}

// ============ توابع WiFi ============
void setupWiFiAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(
    IPAddress(192, 168, 4, 1),
    IPAddress(192, 168, 4, 1),
    IPAddress(255, 255, 255, 0)
  );
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  Serial.println("WiFi AP Mode: " + String(AP_SSID));
  Serial.println("IP: 192.168.4.1");
  AP_MODE = true;
}

void setupWiFiSTA() {
  if (strlen(wifi_ssid) == 0) {
    setupWiFiAP();
    return;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.config(
    IPAddress(192, 168, 1, 101),
    IPAddress(192, 168, 1, 1),
    IPAddress(255, 255, 255, 0)
  );
  
  WiFi.begin(wifi_ssid, wifi_password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    AP_MODE = false;
    Serial.println("\nWiFi Connected: " + String(wifi_ssid));
    Serial.println("IP: " + WiFi.localIP().toString());
  } else {
    setupWiFiAP();
  }
}

// ============ توابع رله ============
void setRelayState(int relay_id, bool state) {
  if (relay_id >= 0 && relay_id < 4) {
    digitalWrite(RELAY_PINS[relay_id], state ? HIGH : LOW);
  }
}

bool getRelayState(int relay_id) {
  if (relay_id >= 0 && relay_id < 4) {
    return digitalRead(RELAY_PINS[relay_id]) == HIGH;
  }
  return false;
}

// ============ توابع HTTP ============
void sendDataToServer() {
  if (AP_MODE) return;
  
  HTTPClient http;
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/api/client_data";
  
  DynamicJsonDocument doc(512);
  doc["device_id"] = BOARD_ID;
  doc["temperature"] = dht.readTemperature();
  doc["humidity"] = dht.readHumidity();
  doc["door_open"] = digitalRead(DOOR_SWITCH_PIN) == HIGH;
  
  JsonArray relays_array = doc.createNestedArray("relays");
  for (int i = 0; i < 4; i++) {
    JsonObject relay = relays_array.createNestedObject();
    relay["id"] = i;
    relay["state"] = getRelayState(i);
  }
  
  String payload;
  serializeJson(doc, payload);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
    Serial.println("Data sent successfully. Code: " + String(httpResponseCode));
  } else {
    Serial.println("Error sending data: " + String(httpResponseCode));
  }
  
  http.end();
}

void receiveCommandsFromServer() {
  if (AP_MODE) return;
  
  HTTPClient http;
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/api/client_commands?device_id=" + String(BOARD_ID);
  
  http.begin(url);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error) {
      JsonArray commands = doc["commands"];
      for (JsonObject cmd : commands) {
        int relay_id = cmd["relay_id"];
        int state = cmd["state"];
        setRelayState(relay_id, state == 1);
        Serial.println("Relay " + String(relay_id) + " set to " + String(state));
      }
    }
  }
  
  http.end();
}

void handleWiFiSetup() {
  String ssid = WiFi.SSID();
  String password = "";
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not configured. Use WiFi AP to configure.");
  }
}

// ============ Setup ============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n===== Office Client Starting =====");
  
  // تنظیم پین‌ها
  pinMode(DOOR_SWITCH_PIN, INPUT_PULLUP);
  for (int i = 0; i < 4; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }
  
  // شروع سنسور DHT22
  dht.begin();
  
  // بارگذاری داده‌های EEPROM
  loadEEPROMData();
  
  // تنظیم WiFi
  setupWiFiSTA();
  
  Serial.println("Setup complete!");
}

// ============ Loop ============
void loop() {
  // ارسال داده هر 5 ثانیه
  if (millis() - last_sync > SYNC_INTERVAL) {
    sendDataToServer();
    receiveCommandsFromServer();
    last_sync = millis();
  }
  
  // بررسی اتصال WiFi
  if (!AP_MODE && WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Trying to reconnect...");
    WiFi.reconnect();
  }
  
  delay(100);
}
