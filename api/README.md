# PD-Stepper API

A Python FastAPI service that bridges Bluetooth Low Energy to HTTP/WebSocket. Any language that can make HTTP requests can control the motor.

## Requirements

- Python 3.10+
- Windows 10 version 1803+ (BLE uses WinRT)
- Bluetooth adapter with BLE support

---

## Setup

```powershell
cd api
pip install -r requirements.txt
python main.py
```

API starts at **http://localhost:8000**

Optional flags:
```
python main.py --host 0.0.0.0 --port 8000
```

---

## Web UI

Navigate to **http://localhost:8000** in any browser.

### Connection Panel
- **Scan** — discovers nearby `PD-Stepper` BLE devices (5 second scan)
- Click a device in the list, then **Connect**
- **Bus voltage** shows live VBUS reading once connected

### Position Panel
- Primary display in **mm** (requires belt config), secondary in degrees
- **Absolute move** — type a target mm value and click Move
- **Jog buttons** — ±0.1 / ±1 / ±10 mm relative moves

### Velocity Panel
- Slider: −200 to +200 mm/s
- **Set Velocity** — engages continuous motion; **Stop** halts it

### Configuration Panel
- Motor current, microsteps, speed, closed-loop mode, direction
- **Belt travel (mm/rev)** — set once for your pulley (GT2 20-tooth = 40 mm)
- **Endstop type** — NO or NC, applied immediately without reflashing
- **Apply** saves all settings

### Event Log
- Color-coded real-time events from the motor:
  - Green: `position_reached`, `home_complete`
  - Red: `stall` (step loss)
  - Yellow: `endstop`

### Homing
- **Home (Endstop)** button — drives motor toward endstop, zeros encoder on contact

---

## REST API Reference

All endpoints return JSON. Errors return HTTP 503 with `{"detail": "..."}`.

### Discovery & Connection

| Method | Path | Body | Description |
|--------|------|------|-------------|
| GET | `/api/scan` | — | Scan 5 s for PD-Stepper devices |
| POST | `/api/connect` | `{"address": "XX:XX:XX:XX:XX:XX"}` | Connect to device |
| POST | `/api/disconnect` | — | Disconnect |
| GET | `/api/status` | — | Connection state + last motor snapshot |
| GET | `/api/config` | — | Read CONFIG characteristic from device |

**Scan response:**
```json
[{"address": "F0:9E:9E:74:E5:B1", "name": "PD-Stepper", "rssi": -62}]
```

**Status response:**
```json
{
  "connected": true,
  "state": "CONNECTED",
  "address": "F0:9E:9E:74:E5:B1",
  "last_status": {
    "type": "status",
    "pos_deg": 90.12,
    "pos_mm": 10.013,
    "vel_dps": 0.0,
    "vel_mm_s": 0.0,
    "voltage_v": 19.96,
    "mode": "idle"
  }
}
```

### Motion — Degree-based

| Method | Path | Body | Description |
|--------|------|------|-------------|
| POST | `/api/move/absolute` | `{"degrees": 180.0}` | Move to absolute angle |
| POST | `/api/move/relative` | `{"degrees": -45.0}` | Relative move |
| POST | `/api/velocity` | `{"dps": 120.0}` | Velocity mode (°/s); 0 = stop |
| POST | `/api/stop` | — | Emergency stop |
| POST | `/api/home` | — | Start endstop homing |

### Motion — Millimetre-based

| Method | Path | Body | Description |
|--------|------|------|-------------|
| POST | `/api/move/absolute_mm` | `{"mm": 100.0}` | Move to absolute position (mm) |
| POST | `/api/move/relative_mm` | `{"mm": -10.0}` | Relative move (mm) |
| POST | `/api/velocity_mm` | `{"mm_per_s": 25.0}` | Velocity mode (mm/s); 0 = stop |

All mm endpoints convert to degrees internally using the current belt config.

### Configuration

| Method | Path | Body | Description |
|--------|------|------|-------------|
| POST | `/api/configure` | See below | Update motor settings |
| POST | `/api/enable` | `{"enabled": true}` | Enable/disable driver |
| POST | `/api/endstop_mode` | `{"normally_closed": false}` | Set switch polarity |
| GET | `/api/belt` | — | Get mm/rev config |
| POST | `/api/belt` | `{"mm_per_rev": 40.0}` | Set belt travel; persists across restarts |

**Configure body** (all fields optional):
```json
{
  "current_ma": 800,
  "microsteps": 16,
  "speed_sps": 800,
  "closed_loop_type": 1,
  "mapping_direction": 1
}
```

---

## WebSocket

Connect to `ws://localhost:8000/ws` to receive all motor events in real time.

Every message is a JSON object. The `type` field identifies the message:

| Type | Description | Key fields |
|------|-------------|------------|
| `status` | Periodic position/velocity (200 ms) | `pos_deg`, `pos_mm`, `vel_dps`, `vel_mm_s`, `voltage_v`, `mode` |
| `position_reached` | Motor settled at target | `pos_deg`, `pos_mm`, `target_deg`, `target_mm` |
| `stall` | Step loss detected | `pos_deg`, `pos_mm` |
| `endstop` | Switch triggered | `pos_deg`, `pos_mm` |
| `home_complete` | Homing done, encoder zeroed | `pos_deg` (0.0) |
| `ack` | Command acknowledged | `cmd`, `ok` |
| `error` | Firmware error | `msg` |

The `pos_mm`, `target_mm`, and `vel_mm_s` fields are injected by the API using the belt config — the firmware only sends degrees.

**Example WebSocket client (Python):**
```python
import asyncio, websockets, json

async def listen():
    async with websockets.connect("ws://localhost:8000/ws") as ws:
        async for msg in ws:
            data = json.loads(msg)
            if data["type"] == "status":
                print(f"Position: {data['pos_mm']:.3f} mm")
            elif data["type"] == "position_reached":
                print("Done!")

asyncio.run(listen())
```

---

## Belt Configuration

The API stores the belt mm/rev setting in `belt_config.json` (created automatically). This persists across restarts.

```
GT2 belt formula: mm_per_rev = teeth × pitch
  20 teeth × 2 mm = 40 mm/rev  (GT2 20-tooth — default)
  16 teeth × 2 mm = 32 mm/rev  (GT2 16-tooth)
```

Set via API:
```bash
curl -X POST http://localhost:8000/api/belt \
     -H "Content-Type: application/json" \
     -d '{"mm_per_rev": 40.0}'
```

---

## Packaging as a Standalone EXE

No Python installation required on the target machine.

```powershell
cd api
build.bat
```

Output: `api/dist/PD_Stepper_API.exe`

The EXE embeds all dependencies including the WinRT BLE backend. Requires Windows 10 1803+.

To run the packaged EXE:
```
PD_Stepper_API.exe --port 8000
```

---

## BLE Reconnection

If the BLE connection drops (board reset, out of range), the API automatically reconnects using exponential backoff (1 s → 2 s → 4 s → … 30 s max). Connected WebSocket clients stay connected; they'll receive new events once BLE is re-established.

The motor **stops immediately** when BLE disconnects (safety behaviour in firmware).

---

## Swagger / OpenAPI Docs

Interactive API docs are available at **http://localhost:8000/docs** while the server is running.
