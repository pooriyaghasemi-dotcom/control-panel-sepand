#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <time.h>

// ============ WiFi Settings ============
const char* AP_SSID = "ESP32_FACTORY_AP";
const char* AP_PASSWORD = "12345678";
const char* AP_IP = "192.168.4.1";

char wifi_ssid[32] = "";
char wifi_password[64] = "";
char device_static_ip[16] = "192.168.1.100";
char device_gateway[16] = "192.168.1.1";
char device_subnet[16] = "255.255.255.0";

// ============ Web Panel Settings ============
char panel_username[32] = "admin";
char panel_password[32] = "12345";

// ============ Sensor and Relay Settings ============
#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

#define DOOR_SWITCH_PIN 5
#define ALARM_RELAY_PIN 12

const int RELAY_PINS[16] = {13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34};

// ============ System Settings ============
#define EEPROM_SIZE 4096
#define WIFI_CONFIG_ADDR 0
#define RELAY_CONFIG_ADDR 512
#define SECURITY_CONFIG_ADDR 1024
#define LOG_ADDR 1536

bool AP_MODE = true;
bool SECURITY_MODE = false;
bool WORK_MODE = true;

WebServer server(80);

struct RelayConfig {
  char name[32];
  bool active;
  int trigger_time;
};

struct SecurityEvent {
  uint32_t timestamp;
  char event_type[20];
  int device_id;
  int relay_id;
  int trigger_minutes;
};

RelayConfig relays[16];
SecurityEvent event_log[100];
int event_count = 0;

uint32_t alarm_end_time = 0;
bool alarm_active = false;

// ============ EEPROM Functions ============
void loadEEPROMData() {
  EEPROM.begin(EEPROM_SIZE);
  
  EEPROM.readString(WIFI_CONFIG_ADDR, wifi_ssid, 32);
  EEPROM.readString(WIFI_CONFIG_ADDR + 32, wifi_password, 64);
  EEPROM.readString(WIFI_CONFIG_ADDR + 96, device_static_ip, 16);
  
  for (int i = 0; i < 16; i++) {
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
  
  for (int i = 0; i < 16; i++) {
    int addr = RELAY_CONFIG_ADDR + (i * 64);
    EEPROM.writeString(addr, relays[i].name);
    EEPROM.write(addr + 32, relays[i].active);
    EEPROM.writeInt(addr + 33, relays[i].trigger_time);
  }
  
  EEPROM.commit();
}

// ============ WiFi Functions ============
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
}

void setupWiFiSTA() {
  if (strlen(wifi_ssid) == 0) {
    setupWiFiAP();
    AP_MODE = true;
    return;
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.config(
    IPAddress(192, 168, 1, 100),
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
    AP_MODE = true;
  }
}

// ============ Relay Functions ============
void setRelayState(int relay_id, bool state) {
  if (relay_id >= 0 && relay_id < 16) {
    digitalWrite(RELAY_PINS[relay_id], state ? HIGH : LOW);
  }
}

bool getRelayState(int relay_id) {
  if (relay_id >= 0 && relay_id < 16) {
    return digitalRead(RELAY_PINS[relay_id]) == HIGH;
  }
  return false;
}

void triggerAlarm(int minutes) {
  if (!SECURITY_MODE) return;
  
  alarm_active = true;
  alarm_end_time = millis() + (minutes * 60000);
  setRelayState(0, true);
  
  Serial.println("ALARM TRIGGERED FOR " + String(minutes) + " MINUTES");
}

// ============ Log Functions ============
void addEventLog(const char* event_type, int device_id, int relay_id, int trigger_minutes) {
  if (event_count >= 100) {
    for (int i = 0; i < 99; i++) {
      event_log[i] = event_log[i + 1];
    }
    event_count = 99;
  }
  
  event_log[event_count].timestamp = time(nullptr);
  strcpy(event_log[event_count].event_type, event_type);
  event_log[event_count].device_id = device_id;
  event_log[event_count].relay_id = relay_id;
  event_log[event_count].trigger_minutes = trigger_minutes;
  
  event_count++;
}

// ============ API Handlers ============
void handleRoot() {
  if (server.method() == HTTP_POST) {
    String username = server.arg("username");
    String password = server.arg("password");
    
    if (username == panel_username && password == panel_password) {
      server.sendHeader("Set-Cookie", "auth=true; Path=/");
      server.send(200, "text/plain", "Login Successful");
    } else {
      server.send(401, "text/plain", "Invalid Credentials");
    }
    return;
  }
  
  server.send(200, "text/html", getWebPanel());
}

void handleWiFiSetup() {
  if (server.method() == HTTP_POST) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    String ip = server.arg("ip");
    
    if (ssid.length() > 0) {
      ssid.toCharArray(wifi_ssid, 32);
      password.toCharArray(wifi_password, 64);
      ip.toCharArray(device_static_ip, 16);
      
      saveEEPROMData();
      
      server.send(200, "text/json", "{\"status\":\"success\",\"message\":\"WiFi configured, restarting...\"}");
      delay(1000);
      ESP.restart();
    }
  }
  
  String html = "<html><body>";
  html += "<h1>WiFi Setup</h1>";
  html += "<form method=\"POST\">";
  html += "<input type=\"text\" name=\"ssid\" placeholder=\"WiFi SSID\" required>";
  html += "<input type=\"password\" name=\"password\" placeholder=\"WiFi Password\" required>";
  html += "<input type=\"text\" name=\"ip\" placeholder=\"Static IP\" value=\"192.168.1.100\">";
  html += "<button type=\"submit\">Connect</button>";
  html += "</form></body></html>";
  
  server.send(200, "text/html", html);
}

void handleGetStatus() {
  DynamicJsonDocument doc(2048);
  
  doc["device_id"] = 1;
  doc["temperature"] = dht.readTemperature();
  doc["humidity"] = dht.readHumidity();
  doc["door_open"] = digitalRead(DOOR_SWITCH_PIN) == HIGH;
  doc["security_mode"] = SECURITY_MODE;
  doc["work_mode"] = WORK_MODE;
  doc["alarm_active"] = alarm_active;
  doc["ip"] = WiFi.localIP().toString();
  
  JsonArray relays_array = doc.createNestedArray("relays");
  for (int i = 0; i < 16; i++) {
    JsonObject relay = relays_array.createNestedObject();
    relay["id"] = i;
    relay["name"] = relays[i].name;
    relay["state"] = getRelayState(i);
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleSetRelay() {
  int relay_id = server.arg("id").toInt();
  int state = server.arg("state").toInt();
  
  if (relay_id >= 0 && relay_id < 16) {
    setRelayState(relay_id, state == 1);
    server.send(200, "text/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "text/json", "{\"status\":\"error\"}");
  }
}

void handleSetRelayName() {
  int relay_id = server.arg("id").toInt();
  String name = server.arg("name");
  
  if (relay_id >= 0 && relay_id < 16 && name.length() > 0) {
    name.toCharArray(relays[relay_id].name, 32);
    saveEEPROMData();
    server.send(200, "text/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "text/json", "{\"status\":\"error\"}");
  }
}

void handleSecuritySetup() {
  int relay_id = server.arg("id").toInt();
  int trigger_minutes = server.arg("minutes").toInt();
  int active = server.arg("active").toInt();
  
  if (relay_id >= 0 && relay_id < 16) {
    relays[relay_id].active = active == 1;
    relays[relay_id].trigger_time = trigger_minutes;
    saveEEPROMData();
    server.send(200, "text/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "text/json", "{\"status\":\"error\"}");
  }
}

void handleSetMode() {
  String mode = server.arg("mode");
  
  if (mode == "security") {
    SECURITY_MODE = true;
    WORK_MODE = false;
    addEventLog("security_mode_enabled", 1, -1, 0);
  } else if (mode == "work") {
    SECURITY_MODE = false;
    WORK_MODE = true;
    addEventLog("work_mode_enabled", 1, -1, 0);
  }
  
  server.send(200, "text/json", "{\"status\":\"success\"}");
}

void handleGetLog() {
  DynamicJsonDocument doc(4096);
  JsonArray logs = doc.createNestedArray("logs");
  
  for (int i = 0; i < event_count; i++) {
    JsonObject log = logs.createNestedObject();
    log["timestamp"] = event_log[i].timestamp;
    log["event_type"] = event_log[i].event_type;
    log["device_id"] = event_log[i].device_id;
    log["relay_id"] = event_log[i].relay_id;
    log["trigger_minutes"] = event_log[i].trigger_minutes;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleTriggerAlarm() {
  int minutes = server.arg("minutes").toInt();
  int device_id = server.arg("device_id").toInt();
  
  if (minutes > 0 && minutes <= 60) {
    triggerAlarm(minutes);
    addEventLog("alarm_triggered", device_id, 0, minutes);
    server.send(200, "text/json", "{\"status\":\"success\"}");
  } else {
    server.send(400, "text/json", "{\"status\":\"error\"}");
  }
}

// ============ Web Panel HTML ============
String getWebPanel() {
  String html = "<!DOCTYPE html>";
  html += "<html dir=\"rtl\" lang=\"fa\"><head>";
  html += "<meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  html += "<title>Factory Control Panel</title>";
  html += "<style>";
  html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
  html += "body { font-family: Tahoma, sans-serif; background: #f5f5f5; }";
  html += ".container { max-width: 1200px; margin: 0 auto; padding: 20px; }";
  html += "header { background: #2c3e50; color: white; padding: 20px; border-radius: 8px; margin-bottom: 20px; text-align: center; }";
  html += "header h1 { font-size: 28px; margin-bottom: 5px; }";
  html += ".status-bar { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-bottom: 20px; }";
  html += ".status-card { background: white; padding: 15px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
  html += ".status-card h3 { color: #2c3e50; margin-bottom: 10px; font-size: 14px; }";
  html += ".status-value { font-size: 24px; font-weight: bold; color: #27ae60; }";
  html += ".status-value.danger { color: #e74c3c; }";
  html += ".tabs { display: flex; gap: 10px; margin-bottom: 20px; flex-wrap: wrap; }";
  html += ".tab-btn { padding: 10px 20px; background: white; border: 2px solid #bdc3c7; border-radius: 5px; cursor: pointer; }";
  html += ".tab-btn.active { background: #3498db; color: white; border-color: #3498db; }";
  html += ".tab-content { display: none; background: white; padding: 20px; border-radius: 8px; }";
  html += ".tab-content.active { display: block; }";
  html += ".relay-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }";
  html += ".relay-card { background: #f9f9f9; border: 2px solid #bdc3c7; border-radius: 8px; padding: 15px; }";
  html += ".relay-card.on { background: #d5f4e6; border-color: #27ae60; }";
  html += ".relay-toggle { width: 50px; height: 30px; background: #bdc3c7; border: none; border-radius: 15px; cursor: pointer; }";
  html += "input, button { padding: 10px; margin: 5px; border: 1px solid #bdc3c7; border-radius: 5px; }";
  html += "button { background: #3498db; color: white; border: none; cursor: pointer; }";
  html += ".mode-btn { padding: 10px 20px; margin: 5px; border: 2px solid #bdc3c7; background: white; cursor: pointer; }";
  html += ".mode-btn.active { background: #27ae60; color: white; border-color: #27ae60; }";
  html += ".log-table { width: 100%; border-collapse: collapse; margin-top: 20px; }";
  html += ".log-table th, .log-table td { padding: 10px; text-align: right; border-bottom: 1px solid #bdc3c7; }";
  html += ".log-table th { background: #34495e; color: white; }";
  html += "</style></head><body>";
  html += "<div class=\"container\"><header>";
  html += "<h1>Factory Control Panel</h1>";
  html += "<p>Automation and Security System</p>";
  html += "</header>";
  
  html += "<div class=\"status-bar\">";
  html += "<div class=\"status-card\"><h3>Temperature</h3><div class=\"status-value\" id=\"temp\">--C</div></div>";
  html += "<div class=\"status-card\"><h3>Humidity</h3><div class=\"status-value\" id=\"humidity\">--%</div></div>";
  html += "<div class=\"status-card\"><h3>Door</h3><div class=\"status-value\" id=\"door_status\">Closed</div></div>";
  html += "<div class=\"status-card\"><h3>Mode</h3><div class=\"status-value\" id=\"mode_status\">Work</div></div>";
  html += "<div class=\"status-card\"><h3>Alarm</h3><div class=\"status-value\" id=\"alarm_status\">Off</div></div>";
  html += "<div class=\"status-card\"><h3>Connection</h3><div class=\"status-value\" id=\"connection_status\" style=\"color: #27ae60;\">Connected</div></div>";
  html += "</div>";
  
  html += "<div class=\"tabs\">";
  html += "<button class=\"tab-btn active\" onclick=\"showTab(event, 'dashboard')\">Dashboard</button>";
  html += "<button class=\"tab-btn\" onclick=\"showTab(event, 'relays')\">Relays</button>";
  html += "<button class=\"tab-btn\" onclick=\"showTab(event, 'security')\">Security</button>";
  html += "<button class=\"tab-btn\" onclick=\"showTab(event, 'sensors')\">Sensors</button>";
  html += "<button class=\"tab-btn\" onclick=\"showTab(event, 'log')\">Log</button>";
  html += "<button class=\"tab-btn\" onclick=\"showTab(event, 'settings')\">Settings</button>";
  html += "</div>";
  
  html += "<div id=\"dashboard\" class=\"tab-content active\">";
  html += "<h3>Select Mode</h3>";
  html += "<button class=\"mode-btn active\" id=\"work-mode-btn\" onclick=\"setMode(event, 'work')\">Work Mode</button>";
  html += "<button class=\"mode-btn\" id=\"security-mode-btn\" onclick=\"setMode(event, 'security')\">Security Mode</button>";
  html += "</div>";
  
  html += "<div id=\"relays\" class=\"tab-content\"><h3>Control Relays</h3><div class=\"relay-grid\" id=\"relay-grid\"></div></div>";
  html += "<div id=\"security\" class=\"tab-content\"><h3>Security Settings</h3><div id=\"security-grid\"></div></div>";
  html += "<div id=\"sensors\" class=\"tab-content\"><h3>Sensors</h3><div id=\"sensor-data\"></div></div>";
  
  html += "<div id=\"log\" class=\"tab-content\"><h3>Event Log</h3>";
  html += "<table class=\"log-table\"><thead><tr><th>Time</th><th>Event</th><th>Device</th><th>Relay</th><th>Duration</th></tr></thead>";
  html += "<tbody id=\"log-body\"></tbody></table></div>";
  
  html += "<div id=\"settings\" class=\"tab-content\"><h3>Settings</h3>";
  html += "<label>SSID:</label><input type=\"text\" id=\"wifi_ssid\" placeholder=\"WiFi Name\">";
  html += "<label>Password:</label><input type=\"password\" id=\"wifi_password\" placeholder=\"WiFi Password\">";
  html += "<label>Static IP:</label><input type=\"text\" id=\"device_ip\" placeholder=\"192.168.1.100\" value=\"192.168.1.100\">";
  html += "<button onclick=\"saveWiFiSettings(event)\">Save WiFi Settings</button>";
  html += "</div>";
  
  html += "</div><script>";
  html += "let relayStates = {};";
  html += "function showTab(evt, name) { ";
  html += "var i, tabcontent, tablinks; ";
  html += "tabcontent = document.getElementsByClassName('tab-content'); ";
  html += "for (i = 0; i < tabcontent.length; i++) { tabcontent[i].classList.remove('active'); } ";
  html += "tablinks = document.getElementsByClassName('tab-btn'); ";
  html += "for (i = 0; i < tablinks.length; i++) { tablinks[i].classList.remove('active'); } ";
  html += "document.getElementById(name).classList.add('active'); ";
  html += "evt.currentTarget.classList.add('active'); }";
  html += "function updateStatus() { ";
  html += "fetch('/api/status').then(r => r.json()).then(d => { ";
  html += "document.getElementById('temp').textContent = d.temperature.toFixed(1) + 'C'; ";
  html += "document.getElementById('humidity').textContent = d.humidity.toFixed(1) + '%'; ";
  html += "document.getElementById('door_status').textContent = d.door_open ? 'Open' : 'Closed'; ";
  html += "document.getElementById('mode_status').textContent = d.security_mode ? 'Security' : 'Work'; ";
  html += "document.getElementById('alarm_status').textContent = d.alarm_active ? 'Active' : 'Off'; ";
  html += "var rg = document.getElementById('relay-grid'); rg.innerHTML = ''; ";
  html += "d.relays.forEach(r => { ";
  html += "var rc = document.createElement('div'); ";
  html += "rc.className = 'relay-card' + (r.state ? ' on' : ''); ";
  html += "rc.innerHTML = '<h4>' + r.name + '</h4><p>Relay ' + (r.id+1) + '</p><button class=\"relay-toggle ' + (r.state ? 'on' : '') + '\" onclick=\"toggleRelay(' + r.id + ')\"></button>'; ";
  html += "rg.appendChild(rc); relayStates[r.id] = r.state; }); }); }";
  html += "function toggleRelay(id) { ";
  html += "var s = relayStates[id] ? 0 : 1; ";
  html += "fetch('/api/set_relay?id=' + id + '&state=' + s).then(r => r.json()).then(d => { updateStatus(); }); }";
  html += "function setMode(evt, m) { ";
  html += "fetch('/api/set_mode?mode=' + m).then(r => r.json()).then(d => { ";
  html += "document.getElementById('work-mode-btn').classList.remove('active'); ";
  html += "document.getElementById('security-mode-btn').classList.remove('active'); ";
  html += "if(m === 'work') document.getElementById('work-mode-btn').classList.add('active'); ";
  html += "else document.getElementById('security-mode-btn').classList.add('active'); ";
  html += "updateStatus(); evt.currentTarget.classList.add('active'); }); }";
  html += "function saveWiFiSettings(evt) { ";
  html += "var s = document.getElementById('wifi_ssid').value; ";
  html += "var p = document.getElementById('wifi_password').value; ";
  html += "var i = document.getElementById('device_ip').value; ";
  html += "if(!s || !p) { alert('Enter SSID and password'); return; } ";
  html += "fetch('/api/wifi_setup', { method: 'POST', headers: {'Content-Type': 'application/x-www-form-urlencoded'}, body: 'ssid=' + s + '&password=' + p + '&ip=' + i }).then(r => r.json()).then(d => { alert('Settings saved'); }); }";
  html += "function loadLog() { ";
  html += "fetch('/api/log').then(r => r.json()).then(d => { ";
  html += "var lb = document.getElementById('log-body'); lb.innerHTML = ''; ";
  html += "d.logs.forEach(l => { var tr = document.createElement('tr'); ";
  html += "var dt = new Date(l.timestamp*1000).toLocaleString(); ";
  html += "tr.innerHTML = '<td>' + dt + '</td><td>' + l.event_type + '</td><td>Device ' + l.device_id + '</td><td>' + (l.relay_id >= 0 ? 'Relay ' + (l.relay_id+1) : '-') + '</td><td>' + l.trigger_minutes + ' min</td>'; ";
  html += "lb.appendChild(tr); }); }); }";
  html += "updateStatus(); loadLog(); setInterval(updateStatus, 2000); setInterval(loadLog, 10000);";
  html += "</script></body></html>";
  
  return html;
}

// ============ Setup ============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n===== Factory Control System Starting =====");
  
  pinMode(DOOR_SWITCH_PIN, INPUT_PULLUP);
  for (int i = 0; i < 16; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }
  
  dht.begin();
  loadEEPROMData();
  setupWiFiSTA();
  
  server.on("/", handleRoot);
  server.on("/api/status", handleGetStatus);
  server.on("/api/set_relay", handleSetRelay);
  server.on("/api/set_relay_name", handleSetRelayName);
  server.on("/api/set_mode", handleSetMode);
  server.on("/api/log", handleGetLog);
  server.on("/api/trigger_alarm", handleTriggerAlarm);
  server.on("/api/security_setup", handleSecuritySetup);
  server.on("/api/wifi_setup", handleWiFiSetup);
  
  server.begin();
  Serial.println("Web Server started");
  
  addEventLog("system_started", 1, -1, 0);
}

// ============ Loop ============
void loop() {
  server.handleClient();
  
  if (alarm_active && millis() > alarm_end_time) {
    alarm_active = false;
    setRelayState(0, false);
    Serial.println("ALARM STOPPED");
  }
  
  static bool last_door_state = false;
  bool current_door_state = digitalRead(DOOR_SWITCH_PIN) == HIGH;
  if (current_door_state != last_door_state) {
    if (current_door_state && SECURITY_MODE) {
      addEventLog("door_opened", 1, -1, 0);
      triggerAlarm(1);
    }
    last_door_state = current_door_state;
  }
  
  delay(100);
}
