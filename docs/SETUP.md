# Installation & Setup Guide

## Required Libraries

Add these libraries to Arduino IDE:

1. **DHT Sensor Library** - by Adafruit
2. **ArduinoJson** - by Benoit Blanchon
3. **ESP32 Core** - Official ESP32 support

### Installation Steps:

1. Open Arduino IDE
2. Go to: Sketch → Include Library → Manage Libraries
3. Search and install:
   - `DHT sensor library`
   - `ArduinoJson`
   - `ESP32` (if not already installed)

## Hardware Connections

### Main Factory Board (16 Relays)
```
Pin 4  → DHT22 Data
Pin 5  → Door Switch (Micro Switch)
Pin 12 → Relay 1 (Alarm)
Pin 13 → Relay 2
Pin 14 → Relay 3
Pin 15 → Relay 4
Pin 16 → Relay 5
Pin 17 → Relay 6
Pin 18 → Relay 7
Pin 19 → Relay 8
Pin 21 → Relay 9
Pin 22 → Relay 10
Pin 23 → Relay 11
Pin 25 → Relay 12
Pin 26 → Relay 13
Pin 27 → Relay 14
Pin 32 → Relay 15
Pin 33 → Relay 16
Pin 34 → Relay 17 (Not available, use external relay module)
```

### Office Client Board (4 Relays)
```
Pin 4  → DHT22 Data
Pin 5  → Door Switch
Pin 12 → Relay 1
Pin 13 → Relay 2
Pin 14 → Relay 3
Pin 15 → Relay 4
```

### Yard Client Board (4 Relays + Laser Sensors)
```
Pin 4  → DHT22 Data
Pin 5  → Door 1 Switch
Pin 6  → Door 2 Switch
Pin 12 → Relay 1
Pin 13 → Relay 2
Pin 14 → Relay 3
Pin 15 → Relay 4
Pin 16 → Laser Sensor 1
Pin 17 → Laser Sensor 2
Pin 18 → Laser Sensor 3
```

## Setup Instructions

### Step 1: Upload Code to Boards

1. Connect ESP32 to computer via USB
2. Select Board: `Tools → Board → ESP32 → ESP32 Dev Module`
3. Select Port: `Tools → Port → COM...`
4. Select baud rate: `115200`
5. Upload:
   - For Main Factory: Upload `main_factory.ino`
   - For Office: Upload `client_office.ino`
   - For Yard: Upload `client_yard.ino`

### Step 2: Initial WiFi Setup

#### First Boot (AP Mode):
Each board starts in WiFi Access Point mode:

**Main Factory:**
- SSID: `ESP32_FACTORY_AP`
- Password: `12345678`
- IP: `192.168.4.1`

**Office Client:**
- SSID: `ESP32_OFFICE_AP`
- Password: `12345678`
- IP: `192.168.4.1`

**Yard Client:**
- SSID: `ESP32_YARD_AP`
- Password: `12345678`
- IP: `192.168.4.1`

#### Configure WiFi:

1. Connect to the board's WiFi AP (from your phone/laptop)
2. Open browser and go to: `192.168.4.1`
3. Fill in the WiFi setup form:
   - **WiFi SSID:** Your factory WiFi network name
   - **WiFi Password:** Your WiFi password
   - **Static IP:** 
     - Main Factory: `192.168.1.100`
     - Office: `192.168.1.101`
     - Yard: `192.168.1.102`
4. Click "Connect"
5. Board will restart and connect to your WiFi

### Step 3: Access Web Panel

1. Once Main Factory board is connected to WiFi
2. Open browser and go to: `http://192.168.1.100`
3. Default credentials:
   - **Username:** `admin`
   - **Password:** `12345`

## Web Panel Features

### Dashboard Tab
- View live temperature and humidity
- View door status
- View current mode (Work/Security)
- View alarm status
- Select between Work Mode and Security Mode

### Relay Control Tab
- Turn relays on/off
- See relay status
- Rename relays

### Security Settings Tab
- Enable/disable security for each relay
- Set trigger time in minutes
- When a security trigger occurs, the relay activates for the specified time

### Sensors Tab
- View live sensor data from all boards
- Temperature and humidity from each location
- Door status from each location
- Laser sensor status from yard

### Log Tab
- View history of all events
- See when doors opened
- See when security triggered
- See when modes changed

### Settings Tab
- Configure WiFi settings
- Update static IP addresses

## Operation Modes

### Work Mode
- Relays can be controlled manually from web panel
- Doors and sensors are monitored but don't trigger alarms
- Normal operations

### Security Mode
- Relays are set to auto-trigger on sensors
- Door opening triggers alarm for 1 minute
- Laser sensors trigger alarm when breached
- All events are logged
- Web panel is still accessible for manual override

## Communication Protocol

### Main Server to Clients
- Main Factory board acts as server
- Office and Yard boards are clients
- Communication via HTTP REST API
- Sync interval: 5 seconds

### API Endpoints (Main Factory)

**GET /api/status**
- Returns current status of all relays, sensors, and modes

**POST /api/set_relay?id=X&state=Y**
- X: relay ID (0-15)
- Y: state (0 or 1)

**POST /api/set_mode?mode=X**
- X: "work" or "security"

**GET /api/log**
- Returns event log

**POST /api/trigger_alarm?minutes=X&device_id=Y**
- X: duration in minutes
- Y: device ID (1, 2, or 3)

**POST /api/client_data**
- Clients send their data here

**GET /api/client_commands?device_id=X**
- Clients poll for commands

## Troubleshooting

### Board not connecting to WiFi
1. Check WiFi credentials
2. Check static IP is not conflicting with other devices
3. Restart board and try again
4. Check serial monitor for errors

### Sensors not reading
1. Check DHT22 connection
2. DHT22 needs 5V power and GND
3. Add 10k pull-up resistor on data pin

### Relays not responding
1. Check relay module power
2. Check GPIO pins are not pulled low by mistake
3. Verify relay module is 5V or 3.3V compatible

### Web panel not loading
1. Check Main Factory board is powered and connected
2. Try accessing from different browser
3. Clear browser cache
4. Check firewall is not blocking port 80

## Default Credentials

- **Web Panel Username:** `admin`
- **Web Panel Password:** `12345`
- **WiFi AP Password:** `12345678`

**⚠️ IMPORTANT: Change these credentials in production!**

## File Structure

```
control-panel-sepand/
├── esp32_main_factory/
│   ├── main_factory.ino          # Main server code
│   └── config.h                  # Configuration
├── esp32_client_office/
│   ├── client_office.ino         # Office client code
│   └── config.h                  # Configuration
├── esp32_client_yard/
│   ├── client_yard.ino           # Yard client code
│   └── config.h                  # Configuration
├── web_panel/
│   ├── index.html                # Web panel (embedded in main_factory.ino)
│   ├── style.css                 # Styling (embedded)
│   └── script.js                 # JavaScript (embedded)
├── docs/
│   ├── SETUP.md                  # This file
│   ├── API.md                    # API documentation
│   └── WIRING.md                 # Detailed wiring guide
└── README.md                     # Project overview
```

## Next Steps

1. ✅ Read this setup guide
2. ✅ Install required libraries
3. ✅ Connect hardware according to wiring guide
4. ✅ Upload code to all three boards
5. ✅ Configure WiFi for each board
6. ✅ Access web panel at `192.168.1.100`
7. ✅ Test all features

## Support

For issues or questions, check:
- Serial monitor output (9600 baud)
- Event log in web panel
- GitHub issues

---

**Version:** v1.0.0  
**Last Updated:** 2026-06-12
