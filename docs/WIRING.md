# Detailed Wiring Guide

## Components Required

### Main Factory Board
- 1x ESP32 Dev Module
- 1x DHT22 Temperature/Humidity Sensor
- 1x 10kΩ Resistor (DHT22 pull-up)
- 1x Micro Switch (Door sensor)
- 1x 16-Channel Relay Module (5V)
- 1x Power Supply (5V, 5A minimum)
- Jump wires
- 16x LED + 220Ω Resistor (optional, for testing)

### Office Client Board
- 1x ESP32 Dev Module
- 1x DHT22 Temperature/Humidity Sensor
- 1x 10kΩ Resistor (DHT22 pull-up)
- 1x Micro Switch (Door sensor)
- 1x 4-Channel Relay Module (5V)
- 1x Power Supply (5V, 2A minimum)
- Jump wires

### Yard Client Board
- 1x ESP32 Dev Module
- 1x DHT22 Temperature/Humidity Sensor
- 1x 10kΩ Resistor (DHT22 pull-up)
- 2x Micro Switch (Door sensors)
- 3x Laser Sensor Module (normally open)
- 1x 4-Channel Relay Module (5V)
- 1x Power Supply (5V, 2A minimum)
- Jump wires

## Main Factory Board Wiring

### ESP32 Pinout Reference
```
          ┌─────────────────┐
          │   ESP32         │
    GND ──┤ GND             │
    3.3V─┤ 3.3V            │
    5V  ─┤ 5V              │
          │                 │
   D4   ─┤ IO4   (DHT22)   │
   D5   ─┤ IO5   (Door)    │
          │                 │
   D12  ─┤ IO12  (Relay1)  │
   D13  ─┤ IO13  (Relay2)  │
   D14  ─┤ IO14  (Relay3)  │
   D15  ─┤ IO15  (Relay4)  │
   D16  ─┤ IO16  (Relay5)  │
   D17  ─┤ IO17  (Relay6)  │
   D18  ─┤ IO18  (Relay7)  │
   D19  ─┤ IO19  (Relay8)  │
          │                 │
   D21  ─┤ IO21  (Relay9)  │
   D22  ─┤ IO22  (Relay10) │
   D23  ─┤ IO23  (Relay11) │
          │                 │
   D25  ─┤ IO25  (Relay12) │
   D26  ─┤ IO26  (Relay13) │
   D27  ─┤ IO27  (Relay14) │
          │                 │
   D32  ─┤ IO32  (Relay15) │
   D33  ─┤ IO33  (Relay16) │
   D34  ─┤ IO34  (Relay17) │
          └─────────────────┘
```

### DHT22 Sensor Wiring
```
DHT22 (Front View):
  1    2    3    4
  ├────┼────┼────┤
  │ +  │ Out│ NC │ GND │
  ├────┼────┼────┤

  Pin 1 (VCC)  → ESP32 3.3V
  Pin 2 (Data) → ESP32 IO4
  Pin 4 (GND)  → ESP32 GND

  ⚠️ Add 10kΩ pull-up resistor:
    ESP32 3.3V ──┤ 10k Ω ├── ESP32 IO4
                      │
                    DHT22 Data
```

### Micro Switch (Door Sensor) Wiring
```
Micro Switch:
  ┌─────────┐
  │ ○──┐    │
  │    ├──○─┤ Common
  │ ○──┘    │
  └─────────┘

  Normally Open (NO) side:
    One pin  → ESP32 IO5 (with 10kΩ pull-up to 3.3V)
    Other pin → ESP32 GND

  Wiring:
    ESP32 3.3V ──┤ 10k Ω ├── ESP32 IO5
                      │
                   Micro Switch NO pin 1

    Micro Switch NO pin 2 → ESP32 GND
```

### 16-Channel Relay Module Wiring
```
16-Channel Relay Module (Pins from left to right):
  GND  GND  IN1  IN2  IN3  ...  IN16  5V  5V

  For each relay:
    ESP32 IO(X) ─────► Relay IN(X)

  Power:
    ESP32 5V ───► Relay Module 5V
    ESP32 GND ──► Relay Module GND

  Relay Output Example (Relay 1):
    Relay Common (C)  ───┐
                         ├─ Load (Light, Motor, etc.)
    Relay Normally Open (NO) ───┘

  Connection Table:
    ESP32 IO12 → Relay IN1
    ESP32 IO13 → Relay IN2
    ESP32 IO14 → Relay IN3
    ESP32 IO15 → Relay IN4
    ESP32 IO16 → Relay IN5
    ESP32 IO17 → Relay IN6
    ESP32 IO18 → Relay IN7
    ESP32 IO19 → Relay IN8
    ESP32 IO21 → Relay IN9
    ESP32 IO22 → Relay IN10
    ESP32 IO23 → Relay IN11
    ESP32 IO25 → Relay IN12
    ESP32 IO26 → Relay IN13
    ESP32 IO27 → Relay IN14
    ESP32 IO32 → Relay IN15
    ESP32 IO33 → Relay IN16
```

## Office Client Board Wiring

### Similar to main, but simplified:

```
ESP32 Pinout:
    GND ──┤ GND
   3.3V ──┤ 3.3V
    5V  ──┤ 5V

    IO4 ──┤ DHT22 Data
    IO5 ──┤ Door Switch

    IO12 ─┤ Relay IN1
    IO13 ─┤ Relay IN2
    IO14 ─┤ Relay IN3
    IO15 ─┤ Relay IN4

DHT22: Same as main board (VCC to 3.3V, Data to IO4, GND to GND)
Door:  Same as main board (to IO5)
Relay: Same as main board (5V and GND, IN1-IN4 connected)
```

## Yard Client Board Wiring

### With Laser Sensors:

```
ESP32 Pinout:
    GND ──┤ GND
   3.3V ──┤ 3.3V
    5V  ──┤ 5V

    IO4 ──┤ DHT22 Data
    IO5 ──┤ Door 1 Switch
    IO6 ──┤ Door 2 Switch

    IO16 ─┤ Laser 1 Signal
    IO17 ─┤ Laser 2 Signal
    IO18 ─┤ Laser 3 Signal

    IO12 ─┤ Relay IN1
    IO13 ─┤ Relay IN2
    IO14 ─┤ Relay IN3
    IO15 ─┤ Relay IN4
```

### Laser Sensor Wiring
```
Laser Sensor Module (IR/Laser Pair):
  ┌──────────┐
  │ + - Out  │
  └──────────┘

  Power (5V):
    Laser + → 5V
    Laser - → GND

  Signal Output (Normally Open/Low):
    Laser Out → ESP32 IO16 (or IO17, IO18)

  Logic:
    - When beam is blocked: Output goes HIGH (triggered)
    - When beam is clear: Output stays LOW (normal)

  Optional Pull-down (if needed):
    Laser Out ──┤ 10k Ω ├── GND
```

## Power Supply

### ESP32 Power Requirements
- Operating Voltage: 3.3V
- Current Draw: 80-160 mA (typical)
- Peak Current: 300-500 mA (WiFi transmission)

### Relay Module Power Requirements
- Operating Voltage: 5V
- Current per Relay: 70-100 mA
- Total for 16 relays: ~1.2A (all on)

### Recommended Power Supply
- **Main Factory:** 5V 5A (25W) minimum
- **Office Client:** 5V 2A (10W) minimum
- **Yard Client:** 5V 2A (10W) minimum

## Complete Wiring Diagram (ASCII)

```
┌─────────────────────────────────────────┐
│         Main Factory Board               │
│                                          │
│    ┌─────────────────┐                  │
│    │     ESP32       │                  │
│    │                 │                  │
│    │ IO4─────┐       │                  │
│    │         │       │                  │
│    │ GND ─┐  │       │                  │
│    │      │  │       │                  │
│    └──────┼──┼───────┘                  │
│           │  │                          │
│         ┌─┴──┴────┐                     │
│         │ DHT22   │                     │
│         │ Sensor  │                     │
│         └─────────┘                     │
│                                          │
│    ┌─────────────────┐                  │
│    │ 16Ch Relay      │                  │
│    │ Module          │                  │
│    │ IN1-IN16        │                  │
│    └─────────────────┘                  │
│      ↑                                   │
│   ESP32 IO12-IO33                       │
│                                          │
│    ┌─────────────────┐                  │
│    │ Micro Switch    │                  │
│    │ (Door)          │                  │
│    └─────────────────┘                  │
│      ↑                                   │
│   ESP32 IO5                             │
│                                          │
└─────────────────────────────────────────┘
```

## Testing Connections

1. **Visual Inspection:**
   - Check all wires are connected correctly
   - Verify no short circuits
   - Check relay module has 5V power

2. **Multimeter Test:**
   - Measure voltage on DHT22: Should be 3.3V
   - Measure voltage on Relay: Should be 5V
   - Test continuity of door switch

3. **Serial Monitor Test:**
   - Upload code and open Serial Monitor (115200 baud)
   - Check for error messages
   - Verify sensor readings

4. **Functional Test:**
   - Turn on a relay from web panel
   - Verify relay clicks
   - Check sensor readings update
   - Test door switch trigger

## Troubleshooting Checklist

- [ ] All power connections correct (3.3V and 5V)
- [ ] All GND connections made
- [ ] DHT22 has 10k pull-up resistor
- [ ] Door switch wired to correct GPIO
- [ ] Relay module has separate 5V power supply
- [ ] No loose wires or short circuits
- [ ] Baud rate set to 115200
- [ ] Board selected correctly in Arduino IDE
- [ ] All required libraries installed

---

**Version:** 1.0.0  
**Last Updated:** 2026-06-12
