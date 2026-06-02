# PD-Stepper BLE Control System

A complete Bluetooth Low Energy control stack for the [PD-Stepper](https://github.com/joshr120/PD-Stepper) motor controller — covering ESP32-S3 firmware, a Python REST/WebSocket API, a browser control UI, and a C# client library.

## What This Adds

The stock PD-Stepper firmware supports USB Serial and WiFi. This project adds:

- **BLE firmware** — full motor control, encoder feedback, endstop homing, and stall detection over Bluetooth
- **Python API service** — REST + WebSocket bridge between BLE and any HTTP client; runs as a Windows service
- **Browser UI** — scan, connect, jog, position moves, velocity, and live telemetry at `http://localhost:8000`
- **C# SDK** — `PDStepperClient.cs` wraps every endpoint with async methods and typed events
- **Linear (mm) units** — belt drive support: configure mm/rev once, command positions in millimetres

## Architecture

```
┌─────────────────────────────────────┐
│  ESP32-S3  (PD-Stepper board)       │
│  ┌────────────────────────────────┐ │
│  │ BLE Firmware                   │ │
│  │  • TMC2209 stepper driver      │ │
│  │  • AS5600 encoder              │ │
│  │  • Endstop (GPIO 13)           │ │
│  │  • StallGuard (DIAG pin)       │ │
│  │  • 20 V USB PD negotiation     │ │
│  └────────────────────────────────┘ │
└──────────────────┬──────────────────┘
                   │ Bluetooth Low Energy
                   │ (custom GATT service)
┌──────────────────▼──────────────────┐
│  Python API  (localhost:8000)        │
│  ┌──────────────────────────────┐   │
│  │ FastAPI + bleak              │   │
│  │  • REST endpoints            │   │
│  │  • WebSocket event stream    │   │
│  │  • Belt mm↔deg conversion    │   │
│  │  • Packaged as .exe          │   │
│  └──────────────────────────────┘   │
└──────┬────────────────┬─────────────┘
       │ HTTP/WS        │ HTTP/WS
┌──────▼──────┐  ┌──────▼──────────────┐
│ Browser UI  │  │ C# Application       │
│ (any device │  │ PDStepperClient.cs   │
│  on network)│  │ MoveAbsoluteMmAsync  │
└─────────────┘  │ OnPositionReached    │
                 └─────────────────────┘
```

## Hardware Requirements

- **PD-Stepper v1.1** board ([joshr120/PD-Stepper](https://github.com/joshr120/PD-Stepper))
- **USB PD charger** capable of 20 V (laptop PD adapter) — the firmware negotiates 20 V by default
- **PC with BLE** — Windows 10 1803+ (uses WinRT BLE stack)
- **Motor** — NEMA 17 with 6-pin JST PH connector (e.g. Hanpose 17HS3401S)

Optional:
- **Endstop switch** — NO or NC microswitch, wired to the AUX connector (pins 2 + 3)
- **GT2 belt + pulley** — for linear positioning in mm

## Quick Start

**1. Flash the firmware**
```
Open firmware/PD_Stepper_BLE/PD_Stepper_BLE.ino in Arduino IDE
Flash to ESP32-S3 Dev Module
```
→ See [`firmware/PD_Stepper_BLE/README.md`](firmware/PD_Stepper_BLE/README.md)

**2. Run the API**
```powershell
cd api
pip install -r requirements.txt
python main.py
```
→ See [`api/README.md`](api/README.md)

**3. Open the UI**

Navigate to **http://localhost:8000**, click **Scan**, connect to `PD-Stepper`.

**4. Use from C#**
```csharp
using PDStepper;

PDStepperClient.EnsureApiRunning("PD_Stepper_API.exe");
var client = new PDStepperClient("http://localhost:8000");

var devices = await client.ScanAsync();
await client.ConnectAsync(devices[0].Address);
await client.StartEventStreamAsync();

client.OnPositionReached += e => Console.WriteLine($"At {e.PosMm:F2} mm");
await client.MoveAbsoluteMmAsync(100.0);
```
→ See [`csharp/README.md`](csharp/README.md)

## Repository Layout

```
├── firmware/
│   └── PD_Stepper_BLE/       Arduino sketch (flash to board)
├── api/
│   ├── main.py               FastAPI REST + WebSocket server
│   ├── ble_manager.py        BLE connection lifecycle
│   ├── belt_config.py        mm↔degree conversion
│   ├── static/               Browser UI (HTML/CSS/JS)
│   ├── requirements.txt
│   └── PD_Stepper_API.spec   PyInstaller build spec
├── csharp/
│   └── PDStepperClient/      .NET 8 C# client library
├── installer/
│   └── PD_Stepper_Setup.iss  Inno Setup installer script
├── FLASHING.md               Step-by-step firmware flash guide
└── README.md                 This file
```

## BLE Protocol Summary

| Characteristic | UUID | Direction | Format |
|----------------|------|-----------|--------|
| Command | `6E400002-...` | Write → device | JSON |
| Status/Events | `6E400003-...` | Notify → client | JSON (200 ms + events) |
| Config | `6E400004-...` | Read | JSON |

See [`firmware/PD_Stepper_BLE/README.md`](firmware/PD_Stepper_BLE/README.md) for the full command and event reference.

## License

MIT
