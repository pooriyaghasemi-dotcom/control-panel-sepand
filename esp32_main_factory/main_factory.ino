#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <time.h>

// ============ تنظیمات WiFi ============
const char* AP_SSID = "ESP32_FACTORY_AP";
const char* AP_PASSWORD = "12345678";
const char* AP_IP = "192.168.4.1";

char wifi_ssid[32] = "";
char wifi_password[64] = "";
char device_static_ip[16] = "192.168.1.100";
char device_gateway[16] = "192.168.1.1";
char device_subnet[16] = "255.255.255.0";

// ============ تنظیمات Web Panel ============
char panel_username[32] = "admin";
char panel_password[32] = "12345";

// ============ تنظیمات سنسورها و رله‌ها ============
#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

#define DOOR_SWITCH_PIN 5
#define ALARM_RELAY_PIN 12  // رله ۱ - آژیر

// رله‌های دیگر (پین‌های ۱۶ کانالی)
const int RELAY_PINS[16] = {13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34};

// ============ تنظیمات سیستم ============
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
  int trigger_time; // به دقیقه
};

struct SecurityEvent {
  uint32_t timestamp;
  char event_type[20]; // door_open, laser_trigger
  int device_id; // 1: factory, 2: office, 3: yard
  int relay_id;
  int trigger_minutes;
};

RelayConfig relays[16];
SecurityEvent event_log[100];
int event_count = 0;

uint32_t alarm_end_time = 0;
bool alarm_active = false;

// ============ توابع EEPROM ============
void loadEEPROMData() {
  EEPROM.begin(EEPROM_SIZE);
  
  // بارگذاری تنظیمات WiFi
  EEPROM.readString(WIFI_CONFIG_ADDR, wifi_ssid, 32);
  EEPROM.readString(WIFI_CONFIG_ADDR + 32, wifi_password, 64);
  EEPROM.readString(WIFI_CONFIG_ADDR + 96, device_static_ip, 16);
  
  // بارگذاری نام‌های رله‌ها
  for (int i = 0; i < 16; i++) {
    int addr = RELAY_CONFIG_ADDR + (i * 64);
    EEPROM.readString(addr, relays[i].name, 32);
    relays[i].active = EEPROM.read(addr + 32);
    relays[i].trigger_time = EEPROM.readInt(addr + 33);
    
    // نام پیش‌فرض
    if (strlen(relays[i].name) == 0) {
      sprintf(relays[i].name, "Relay %d", i + 1);
    }
  }
}

void saveEEPROMData() {
  EEPROM.begin(EEPROM_SIZE);
  
  // ذخیره تنظیمات WiFi
  EEPROM.writeString(WIFI_CONFIG_ADDR, wifi_ssid);
  EEPROM.writeString(WIFI_CONFIG_ADDR + 32, wifi_password);
  EEPROM.writeString(WIFI_CONFIG_ADDR + 96, device_static_ip);
  
  // ذخیره نام‌های رله‌ها
  for (int i = 0; i < 16; i++) {
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

// ============ توابع رله ============
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
  setRelayState(0, true); // رله ۱ - آژیر
  
  Serial.println("ALARM TRIGGERED FOR " + String(minutes) + " MINUTES");
}

// ============ توابع Log ============
void addEventLog(const char* event_type, int device_id, int relay_id, int trigger_minutes) {
  if (event_count >= 100) {
    // حذف رویداد قدیمی‌ترین
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
  
  // بازگرداندن صفحه اصلی
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
  html += "<form method='POST'>";
  html += "<input type='text' name='ssid' placeholder='WiFi SSID' required>";
  html += "<input type='password' name='password' placeholder='WiFi Password' required>";
  html += "<input type='text' name='ip' placeholder='Static IP' value='192.168.1.100'>";
  html += "<button type='submit'>Connect</button>";
  html += "</form></body></html>";
  
  server.send(200, "text/html", html);
}

void handleGetStatus() {
  DynamicJsonDocument doc(2048);
  
  doc["device_id"] = 1; // سالن تولید
  doc["temperature"] = dht.readTemperature();
  doc["humidity"] = dht.readHumidity();
  doc["door_open"] = digitalRead(DOOR_SWITCH_PIN) == HIGH;
  doc["security_mode"] = SECURITY_MODE;
  doc["work_mode"] = WORK_MODE;
  doc["alarm_active"] = alarm_active;
  doc["ip"] = WiFi.localIP().toString();
  
  // وضعیت رله‌ها
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
  String html = R"(
<!DOCTYPE html>
<html dir="rtl" lang="fa">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>کنترل پنل کارخانه سپند</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: 'Tahoma', sans-serif; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        header { background: #2c3e50; color: white; padding: 20px; border-radius: 8px; margin-bottom: 20px; text-align: center; }
        header h1 { font-size: 28px; margin-bottom: 5px; }
        .status-bar { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin-bottom: 20px; }
        .status-card { background: white; padding: 15px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .status-card h3 { color: #2c3e50; margin-bottom: 10px; font-size: 14px; }
        .status-value { font-size: 24px; font-weight: bold; color: #27ae60; }
        .status-value.danger { color: #e74c3c; }
        .status-value.warning { color: #f39c12; }
        .tabs { display: flex; gap: 10px; margin-bottom: 20px; flex-wrap: wrap; }
        .tab-btn { padding: 10px 20px; background: white; border: 2px solid #bdc3c7; border-radius: 5px; cursor: pointer; font-size: 14px; }
        .tab-btn.active { background: #3498db; color: white; border-color: #3498db; }
        .tab-content { display: none; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .tab-content.active { display: block; }
        .relay-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
        .relay-card { background: #f9f9f9; border: 2px solid #bdc3c7; border-radius: 8px; padding: 15px; }
        .relay-card.on { background: #d5f4e6; border-color: #27ae60; }
        .relay-card h4 { margin-bottom: 10px; color: #2c3e50; }
        .relay-toggle { width: 50px; height: 30px; background: #bdc3c7; border: none; border-radius: 15px; cursor: pointer; position: relative; transition: 0.3s; }
        .relay-toggle.on { background: #27ae60; }
        .relay-toggle::after { content: ''; position: absolute; width: 26px; height: 26px; background: white; border-radius: 50%; top: 2px; left: 2px; transition: 0.3s; }
        .relay-toggle.on::after { left: 22px; }
        .sensor-data { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; }
        .sensor-card { background: #ecf0f1; padding: 15px; border-radius: 8px; }
        .sensor-card h4 { margin-bottom: 10px; color: #2c3e50; }
        .sensor-value { font-size: 18px; font-weight: bold; color: #3498db; }
        input, select, button { padding: 10px; margin: 5px; border: 1px solid #bdc3c7; border-radius: 5px; font-size: 14px; }
        button { background: #3498db; color: white; border: none; cursor: pointer; }
        button:hover { background: #2980b9; }
        .mode-selector { margin-bottom: 20px; }
        .mode-btn { padding: 10px 20px; margin: 5px; border: 2px solid #bdc3c7; background: white; border-radius: 5px; cursor: pointer; font-size: 14px; }
        .mode-btn.active { background: #27ae60; color: white; border-color: #27ae60; }
        .log-table { width: 100%; border-collapse: collapse; margin-top: 20px; }
        .log-table th, .log-table td { padding: 10px; text-align: right; border-bottom: 1px solid #bdc3c7; }
        .log-table th { background: #34495e; color: white; }
        .security-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; }
        .security-card { background: #fef5e7; border-left: 4px solid #f39c12; padding: 15px; border-radius: 5px; }
        .security-card label { display: block; margin: 10px 0; }
        .security-card input { width: 100%; }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>[PLANT] Control Panel</h1>
            <p>Factory Automation and Security System</p>
        </header>

        <div class="status-bar">
            <div class="status-card">
                <h3>Temperature</h3>
                <div class="status-value" id="temp">--C</div>
            </div>
            <div class="status-card">
                <h3>Humidity</h3>
                <div class="status-value" id="humidity">--%</div>
            </div>
            <div class="status-card">
                <h3>Door</h3>
                <div class="status-value" id="door_status">Closed</div>
            </div>
            <div class="status-card">
                <h3>Mode</h3>
                <div class="status-value" id="mode_status">Work Mode</div>
            </div>
            <div class="status-card">
                <h3>Alarm</h3>
                <div class="status-value" id="alarm_status">Off</div>
            </div>
            <div class="status-card">
                <h3>Connection</h3>
                <div class="status-value" id="connection_status" style="color: #27ae60;">Connected</div>
            </div>
        </div>

        <div class="tabs">
            <button class="tab-btn active" onclick="showTab('dashboard')">Dashboard</button>
            <button class="tab-btn" onclick="showTab('relays')">Control Relays</button>
            <button class="tab-btn" onclick="showTab('security')">Security Settings</button>
            <button class="tab-btn" onclick="showTab('sensors')">Sensors</button>
            <button class="tab-btn" onclick="showTab('log')">Log</button>
            <button class="tab-btn" onclick="showTab('settings')">Settings</button>
        </div>

        <div id="dashboard" class="tab-content active">
            <div class="mode-selector">
                <h3>Select Mode</h3>
                <button class="mode-btn active" id="work-mode-btn" onclick="setMode('work')">Work Mode</button>
                <button class="mode-btn" id="security-mode-btn" onclick="setMode('security')">Security Mode</button>
            </div>
        </div>

        <div id="relays" class="tab-content">
            <h3>Control Relays</h3>
            <div class="relay-grid" id="relay-grid"></div>
        </div>

        <div id="security" class="tab-content">
            <h3>Security Settings</h3>
            <p>Select which relays trigger on security alert:</p>
            <div class="security-grid" id="security-grid"></div>
        </div>

        <div id="sensors" class="tab-content">
            <h3>Sensor Data</h3>
            <div class="sensor-data" id="sensor-data"></div>
        </div>

        <div id="log" class="tab-content">
            <h3>Event Log</h3>
            <table class="log-table" id="log-table">
                <thead>
                    <tr>
                        <th>Time</th>
                        <th>Event Type</th>
                        <th>Device</th>
                        <th>Relay</th>
                        <th>Duration</th>
                    </tr>
                </thead>
                <tbody id="log-body"></tbody>
            </table>
        </div>

        <div id="settings" class="tab-content">
            <h3>Settings</h3>
            <h4>WiFi Settings</h4>
            <div>
                <label>SSID:</label>
                <input type="text" id="wifi_ssid" placeholder="WiFi Network Name">
            </div>
            <div>
                <label>Password:</label>
                <input type="password" id="wifi_password" placeholder="WiFi Password">
            </div>
            <div>
                <label>Static IP:</label>
                <input type="text" id="device_ip" placeholder="192.168.1.100" value="192.168.1.100">
            </div>
            <button onclick="saveWiFiSettings()">Save WiFi Settings</button>
        </div>
    </div>

    <script>
        let relayStates = {};

        function showTab(tabName) {
            const contents = document.querySelectorAll('.tab-content');
            const buttons = document.querySelectorAll('.tab-btn');
            
            contents.forEach(content => content.classList.remove('active'));
            buttons.forEach(btn => btn.classList.remove('active'));
            
            document.getElementById(tabName).classList.add('active');
            event.target.classList.add('active');
        }

        function updateStatus() {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('temp').textContent = data.temperature.toFixed(1) + 'C';
                    document.getElementById('humidity').textContent = data.humidity.toFixed(1) + '%';
                    document.getElementById('door_status').textContent = data.door_open ? 'Open' : 'Closed';
                    document.getElementById('door_status').className = data.door_open ? 'status-value danger' : 'status-value';
                    document.getElementById('mode_status').textContent = data.security_mode ? 'Security' : 'Work';
                    document.getElementById('alarm_status').textContent = data.alarm_active ? 'Active' : 'Off';
                    document.getElementById('alarm_status').className = data.alarm_active ? 'status-value danger' : 'status-value';

                    const relayGrid = document.getElementById('relay-grid');
                    relayGrid.innerHTML = '';
                    data.relays.forEach(relay => {
                        const relayCard = document.createElement('div');
                        relayCard.className = 'relay-card' + (relay.state ? ' on' : '');
                        relayCard.innerHTML = `
                            <h4>${relay.name}</h4>
                            <p>Relay #${relay.id + 1}</p>
                            <button class="relay-toggle ${relay.state ? 'on' : ''}" onclick="toggleRelay(${relay.id})"></button>
                        `;
                        relayGrid.appendChild(relayCard);
                        relayStates[relay.id] = relay.state;
                    });
                })
                .catch(error => console.error('Error:', error));
        }

        function toggleRelay(id) {
            const newState = relayStates[id] ? 0 : 1;
            fetch(`/api/set_relay?id=${id}&state=${newState}`)
                .then(response => response.json())
                .then(data => {
                    updateStatus();
                })
                .catch(error => console.error('Error:', error));
        }

        function setMode(mode) {
            fetch(`/api/set_mode?mode=${mode}`)
                .then(response => response.json())
                .then(data => {
                    document.getElementById('work-mode-btn').classList.remove('active');
                    document.getElementById('security-mode-btn').classList.remove('active');
                    if (mode === 'work') {
                        document.getElementById('work-mode-btn').classList.add('active');
                    } else {
                        document.getElementById('security-mode-btn').classList.add('active');
                    }
                    updateStatus();
                })
                .catch(error => console.error('Error:', error));
        }

        function saveWiFiSettings() {
            const ssid = document.getElementById('wifi_ssid').value;
            const password = document.getElementById('wifi_password').value;
            const ip = document.getElementById('device_ip').value;

            if (!ssid || !password) {
                alert('Please enter SSID and password');
                return;
            }

            fetch('/api/wifi_setup', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: `ssid=${ssid}&password=${password}&ip=${ip}`
            })
            .then(response => response.json())
            .then(data => {
                alert('Settings saved. Device is restarting...');
            })
            .catch(error => console.error('Error:', error));
        }

        function loadLog() {
            fetch('/api/log')
                .then(response => response.json())
                .then(data => {
                    const logBody = document.getElementById('log-body');
                    logBody.innerHTML = '';
                    data.logs.forEach(log => {
                        const row = document.createElement('tr');
                        const date = new Date(log.timestamp * 1000).toLocaleString();
                        row.innerHTML = `
                            <td>${date}</td>
                            <td>${log.event_type}</td>
                            <td>Device ${log.device_id}</td>
                            <td>${log.relay_id >= 0 ? 'Relay ' + (log.relay_id + 1) : '-'}</td>
                            <td>${log.trigger_minutes} min</td>
                        `;
                        logBody.appendChild(row);
                    });
                })
                .catch(error => console.error('Error:', error));
        }

        updateStatus();
        loadLog();
        setInterval(updateStatus, 2000);
        setInterval(loadLog, 10000);
    </script>
</body>
</html>
  )";
  return html;
}

// ============ Setup ============
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n===== Factory Control System Starting =====");
  
  // تنظیم پین‌ها
  pinMode(DOOR_SWITCH_PIN, INPUT_PULLUP);
  for (int i = 0; i < 16; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
  }
  
  // شروع سنسور DHT22
  dht.begin();
  
  // بارگذاری داده‌های EEPROM
  loadEEPROMData();
  
  // تنظیم WiFi
  setupWiFiSTA();
  
  // تنظیم Web Server
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
  
  // اضافه کردن رویداد شروع
  addEventLog("system_started", 1, -1, 0);
}

// ============ Loop ============
void loop() {
  server.handleClient();
  
  // بررسی آژیر
  if (alarm_active && millis() > alarm_end_time) {
    alarm_active = false;
    setRelayState(0, false);
    Serial.println("ALARM STOPPED");
  }
  
  // بررسی درب
  static bool last_door_state = false;
  bool current_door_state = digitalRead(DOOR_SWITCH_PIN) == HIGH;
  if (current_door_state != last_door_state) {
    if (current_door_state && SECURITY_MODE) {
      addEventLog("door_opened", 1, -1, 0);
      triggerAlarm(1); // آژیر برای ۱ دقیقه
    }
    last_door_state = current_door_state;
  }
  
  delay(100);
}
