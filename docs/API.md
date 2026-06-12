# API Documentation

## Overview

The Main Factory board acts as a central server. Clients (Office and Yard) communicate with it via HTTP REST API.

## Authentication

Currently, there is no authentication for API endpoints. Add authentication in production!

## Main Server Endpoints (192.168.1.100:80)

### 1. Get System Status

**Endpoint:** `GET /api/status`

**Response:**
```json
{
  "device_id": 1,
  "temperature": 25.5,
  "humidity": 60.2,
  "door_open": false,
  "security_mode": true,
  "work_mode": false,
  "alarm_active": false,
  "ip": "192.168.1.100",
  "relays": [
    {
      "id": 0,
      "name": "Alarm",
      "state": false
    },
    {
      "id": 1,
      "name": "Light 1",
      "state": true
    },
    ...
  ]
}
```

### 2. Set Relay State

**Endpoint:** `POST /api/set_relay?id=X&state=Y`

**Parameters:**
- `id`: Relay ID (0-15)
- `state`: 0 (off) or 1 (on)

**Response:**
```json
{
  "status": "success"
}
```

**Example:**
```
POST /api/set_relay?id=1&state=1
```

### 3. Set Relay Name

**Endpoint:** `POST /api/set_relay_name?id=X&name=Y`

**Parameters:**
- `id`: Relay ID (0-15)
- `name`: New relay name (max 32 chars)

**Response:**
```json
{
  "status": "success"
}
```

### 4. Set Operation Mode

**Endpoint:** `POST /api/set_mode?mode=X`

**Parameters:**
- `mode`: "work" or "security"

**Response:**
```json
{
  "status": "success"
}
```

### 5. Configure Security

**Endpoint:** `POST /api/security_setup?id=X&minutes=Y&active=Z`

**Parameters:**
- `id`: Relay ID to trigger
- `minutes`: Duration in minutes
- `active`: 0 (disable) or 1 (enable)

**Response:**
```json
{
  "status": "success"
}
```

### 6. Trigger Alarm

**Endpoint:** `POST /api/trigger_alarm?minutes=X&device_id=Y`

**Parameters:**
- `minutes`: Duration in minutes (1-60)
- `device_id`: Device ID (1=factory, 2=office, 3=yard)

**Response:**
```json
{
  "status": "success"
}
```

### 7. Get Event Log

**Endpoint:** `GET /api/log`

**Response:**
```json
{
  "logs": [
    {
      "timestamp": 1686048000,
      "event_type": "door_opened",
      "device_id": 1,
      "relay_id": -1,
      "trigger_minutes": 0
    },
    {
      "timestamp": 1686048060,
      "event_type": "alarm_triggered",
      "device_id": 3,
      "relay_id": 0,
      "trigger_minutes": 5
    },
    ...
  ]
}
```

### 8. WiFi Setup

**Endpoint:** `POST /api/wifi_setup`

**Form Data:**
```
ssid=YourWiFiName
password=YourPassword
ip=192.168.1.100
```

**Response:**
```json
{
  "status": "success",
  "message": "WiFi configured, restarting..."
}
```

Board will restart after this call.

## Client Endpoints (Sent by Clients)

### 1. Send Client Data

**Endpoint:** `POST /api/client_data`

**Payload:**
```json
{
  "device_id": 2,
  "temperature": 22.5,
  "humidity": 55.0,
  "door_open": false,
  "relays": [
    {"id": 0, "state": true},
    {"id": 1, "state": false},
    ...
  ]
}
```

**For Yard (Device ID 3):**
```json
{
  "device_id": 3,
  "temperature": 28.0,
  "humidity": 45.0,
  "door1_open": false,
  "door2_open": true,
  "lasers": [
    {"id": 1, "triggered": false},
    {"id": 2, "triggered": true},
    {"id": 3, "triggered": false}
  ],
  "relays": [
    {"id": 0, "state": false},
    ...
  ]
}
```

### 2. Get Commands for Client

**Endpoint:** `GET /api/client_commands?device_id=X`

**Parameters:**
- `device_id`: 2 (office) or 3 (yard)

**Response:**
```json
{
  "commands": [
    {
      "relay_id": 1,
      "state": 1
    },
    {
      "relay_id": 3,
      "state": 0
    }
  ]
}
```

## Error Responses

### 400 Bad Request
```json
{
  "status": "error",
  "message": "Invalid relay ID"
}
```

### 401 Unauthorized
```json
{
  "status": "error",
  "message": "Invalid credentials"
}
```

### 404 Not Found
```json
{
  "status": "error",
  "message": "Endpoint not found"
}
```

### 500 Internal Server Error
```json
{
  "status": "error",
  "message": "Internal server error"
}
```

## Communication Flow

```
┌─────────────────────────────────────────┐
│     Main Factory (192.168.1.100)        │
│  - Web Panel (HTTP Server)              │
│  - 16 Relays                            │
│  - Event Log                            │
└────────────┬────────────────────────────┘
             │
     ┌───────┴────────┐
     ▼                ▼
┌──────────────┐  ┌──────────────┐
│   Office     │  │    Yard      │
│192.168.1.101 │  │192.168.1.102 │
│  4 Relays    │  │  4 Relays    │
│  1 Door      │  │  2 Doors     │
│              │  │  3 Lasers    │
└──────────────┘  └──────────────┘

Clients send data every 5 seconds
Clients receive commands every 5 seconds
```

## Usage Examples

### cURL Examples

**Get Status:**
```bash
curl http://192.168.1.100/api/status
```

**Turn on Relay 1:**
```bash
curl -X POST "http://192.168.1.100/api/set_relay?id=1&state=1"
```

**Set Security Mode:**
```bash
curl -X POST "http://192.168.1.100/api/set_mode?mode=security"
```

**Trigger Alarm:**
```bash
curl -X POST "http://192.168.1.100/api/trigger_alarm?minutes=5&device_id=3"
```

**Get Log:**
```bash
curl http://192.168.1.100/api/log
```

### Python Examples

```python
import requests
import json

BASE_URL = "http://192.168.1.100"

# Get status
response = requests.get(f"{BASE_URL}/api/status")
print(response.json())

# Turn on relay
response = requests.post(f"{BASE_URL}/api/set_relay?id=1&state=1")
print(response.json())

# Set security mode
response = requests.post(f"{BASE_URL}/api/set_mode?mode=security")
print(response.json())

# Get log
response = requests.get(f"{BASE_URL}/api/log")
log_data = response.json()
for entry in log_data['logs']:
    print(f"{entry['timestamp']}: {entry['event_type']}")
```

## Rate Limiting

- Clients sync every 5 seconds
- No artificial rate limiting (implement in production)
- Server timeout: 5 seconds per request

## Security Considerations

⚠️ **This implementation has NO security measures. For production:**

1. ✅ Add authentication (username/password or API tokens)
2. ✅ Use HTTPS instead of HTTP
3. ✅ Implement rate limiting
4. ✅ Add input validation
5. ✅ Implement access control per device
6. ✅ Add encryption for sensitive data
7. ✅ Implement request signing

## Version

**API Version:** 1.0.0  
**Last Updated:** 2026-06-12
