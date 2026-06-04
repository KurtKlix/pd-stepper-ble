# PD-Stepper BLE Firmware

Arduino sketch for the ESP32-S3 that replaces the stock serial/WiFi interface with Bluetooth Low Energy.

## Features

- Full motor control (position, velocity, homing) over BLE
- Real-time encoder position + velocity telemetry (AS5600, 200 ms interval)
- `position_reached` event when target is achieved
- `stall` event via TMC2209 StallGuard (DIAG pin interrupt)
- Endstop homing with configurable NO/NC switch polarity — changeable at runtime, no reflash
- Standstill current reduction (motor runs cool when idle)
- 20 V USB PD negotiation at startup
- Live VBUS voltage monitoring

---

## Hardware Pin Reference

| Signal | GPIO | Notes |
|--------|------|-------|
| STEP | 5 | Step pulse to TMC2209 |
| DIR | 6 | Direction |
| ENABLE | 21 | Active LOW |
| DIAG | 16 | StallGuard output (interrupt, RISING) |
| MS1 | 1 | Microstepping |
| MS2 | 2 | Microstepping |
| TMC TX | 17 | UART to driver |
| TMC RX | 18 | UART from driver |
| I2C SDA | 8 | AS5600 encoder |
| I2C SCL | 9 | AS5600 encoder |
| Endstop | 13 | AUX connector pin 2, INPUT_PULLUP |
| VBUS ADC | 4 | 20 kΩ / 2.7 kΩ divider |
| CFG1 | 38 | CH224K PD trigger |
| CFG2 | 48 | CH224K PD trigger |
| CFG3 | 47 | CH224K PD trigger |

### AUX Connector (JST SH 1 mm, 3-pin)

Pin 1 is closest to the USB-C port.

```
Pin 1  →  GND      ← endstop GND wire   (switch C terminal)
Pin 2  →  GPIO 14  (unused in BLE mode)
Pin 3  →  GPIO 13  ← endstop signal wire (switch NO terminal)
```

Both NO and NC switches wire the same way. Polarity is set via the `endstop_mode` BLE command.

---

## Arduino IDE Setup

### Board Support

1. **File → Preferences → Additional boards manager URLs:**
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
2. **Tools → Board → Boards Manager** → install **esp32 by Espressif Systems** version **3.x**

### Required Libraries (Tools → Manage Libraries)

| Library | Author | Version |
|---------|--------|---------|
| NimBLE-Arduino | h2zero | 2.5+ |
| ArduinoJson | Benoit Blanchon | **7.x** (not 6.x) |
| TMCStepper | teemuatlut | latest |

### Board Settings (Tools menu)

| Setting | Value |
|---------|-------|
| Board | **ESP32S3 Dev Module** |
| USB CDC On Boot | **Enabled** |
| USB Mode | Hardware CDC and JTAG |
| Flash Mode | QIO 80 MHz |
| Flash Size | 4 MB (32 Mb) |
| Partition Scheme | Default 4 MB with spiffs |
| Upload Speed | 921600 |
| PSRAM | Disabled |

---

## Flashing

### First Flash

The ESP32-S3 uses native USB. The first flash requires manual bootloader entry:

1. Hold **BOOT** button on the board
2. Press and release **RESET** while holding BOOT
3. Release **BOOT**
4. Select the new COM port in **Tools → Port**
5. Click **Upload**

Subsequent flashes happen automatically (USB CDC re-enumerates).

### Verify

Open **Tools → Serial Monitor** at **115200 baud**, press RESET. You should see:

```
[PD-Stepper BLE] starting...
[BLE] advertising as: PD-Stepper
[PD-Stepper BLE] ready
```

Verify BLE with **nRF Connect** (iOS/Android): scan for `PD-Stepper`, connect, subscribe to the notify characteristic (`6E400003-...`), write `{"cmd":"status"}` to the command characteristic (`6E400002-...`).

---

## BLE GATT Profile

**Service UUID:** `6E400001-B5A3-F393-E0A9-E50E24DC4179`

| Characteristic | UUID | Properties |
|----------------|------|------------|
| Command | `6E400002-B5A3-F393-E0A9-E50E24DC4179` | Write / Write NR |
| Status | `6E400003-B5A3-F393-E0A9-E50E24DC4179` | Notify |
| Config | `6E400004-B5A3-F393-E0A9-E50E24DC4179` | Read |

---

## Command Reference

Write JSON strings to the **Command** characteristic.

### Motion

| Command | Example | Description |
|---------|---------|-------------|
| `deg` | `{"cmd":"deg","val":180.0}` | Move to absolute angle (degrees) |
| `deg_rel` | `{"cmd":"deg_rel","val":-45.0}` | Move relative (degrees) |
| `vel` | `{"cmd":"vel","val":120.0}` | Velocity mode (°/s); negative = reverse |
| `stop` | `{"cmd":"stop"}` | Immediate stop |
| `home` | `{"cmd":"home"}` | Drive toward endstop, zero encoder on trigger |

### Configuration

| Command | Example | Description |
|---------|---------|-------------|
| `speed` | `{"cmd":"speed","val":800}` | Position move speed (steps/sec) |
| `current` | `{"cmd":"current","val":800}` | RMS current (mA) |
| `microsteps` | `{"cmd":"microsteps","val":16}` | 1/2/4/8/16/32/64/256 |
| `enable` | `{"cmd":"enable","val":1}` | 1 = enable driver, 0 = disable |
| `closed_loop_type` | `{"cmd":"closed_loop_type","val":1}` | 0 = open loop, 1 = encoder closed |
| `mappingDirection` | `{"cmd":"mappingDirection","val":-1}` | 1 = normal, -1 = reversed |
| `endstop_mode` | `{"cmd":"endstop_mode","val":0}` | 0 = NO switch, 1 = NC switch |
| `status` | `{"cmd":"status"}` | Request immediate status push |

---

## Event Reference

The **Status** characteristic notifies JSON objects.

### Periodic Status (every 200 ms while connected)

```json
{
  "type": "status",
  "pos_deg": 90.12,
  "vel_dps": 0.0,
  "target_deg": 90.0,
  "enabled": true,
  "mode": "idle",
  "current_ma": 800,
  "microsteps": 16,
  "voltage_v": 19.96,
  "ts": 12345
}
```

`mode` values: `"idle"` `"position"` `"velocity"` `"homing"`

### Motion Events

| Type | Fired when | Key fields |
|------|-----------|------------|
| `position_reached` | Motor settled within 0.5° of target for 150 ms | `pos_deg`, `target_deg` |
| `stall` | TMC2209 StallGuard triggered (step loss) | `pos_deg` |
| `endstop` | Endstop switch triggered | `pos_deg` |
| `home_complete` | Homing finished, encoder zeroed | `pos_deg` (always 0.0) |
| `ack` | Command received and parsed | `cmd`, `ok` |
| `error` | JSON parse failure | `msg` |

### Config Read Characteristic

```json
{
  "fw_ver": "1.0.0",
  "device": "PD-Stepper",
  "current_ma": 800,
  "microsteps": 16,
  "speed_sps": 800,
  "closed_loop": 1,
  "mapping_dir": 1,
  "endstop_gpio": 13,
  "endstop_mode": "NO",
  "stallguard_threshold": 50,
  "voltage_v": 19.96,
  "pd_target_v": 20
}
```

---

## USB PD Voltage

The firmware requests **20 V** by default (CH224K CFG pins: CFG1=LOW, CFG2=HIGH, CFG3=LOW).

To change the requested voltage, edit `config.h`:

```cpp
#define PD_VOLTAGE 12   // 5, 9, 12, 15, or 20
```

And update the CFG pin states in `motor_control.cpp` `init()` accordingly:

| Voltage | CFG1 | CFG2 | CFG3 |
|---------|------|------|------|
| 9 V | LOW | LOW | LOW |
| 12 V | LOW | LOW | HIGH |
| 15 V | LOW | HIGH | HIGH |
| 20 V | LOW | HIGH | LOW |

**Note:** The PD charger must support the requested voltage or negotiation falls back to 5 V.

---

## Endstop Wiring

```
Microswitch ──────────┐
                       │
AUX Pin 2 (GPIO 13) ──┤
AUX Pin 3 (GND)    ───┘

NO switch: open at rest, closes when triggered
NC switch: closed at rest, opens when triggered
```

Set switch type via BLE command (no reflash needed):
```json
{"cmd": "endstop_mode", "val": 0}   // NO
{"cmd": "endstop_mode", "val": 1}   // NC
```

Or via the API: `POST /api/endstop_mode {"normally_closed": true}`
