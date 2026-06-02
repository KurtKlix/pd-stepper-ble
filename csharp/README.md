# PDStepperClient — C# SDK

A .NET 8 client library for the PD-Stepper API. No external NuGet packages required — uses only the BCL (`System.Net.Http`, `System.Net.WebSockets`, `System.Text.Json`).

## Projects

| Project | Description |
|---------|-------------|
| `PDStepperClient/` | Library — add to your application |
| `PDStepperTest/` | Console test program (scan → connect → run moves → verify) |

## Adding to Your Project

Copy `PDStepperClient.cs` and `Models.cs` into your project, or reference the `.csproj`:

```xml
<ItemGroup>
  <ProjectReference Include="../PDStepperClient/PDStepperClient.csproj" />
</ItemGroup>
```

Target frameworks: **.NET 8.0** and **.NET 10.0** (multi-targeted).

---

## Quick Start

```csharp
using PDStepper;

// Ensure the API service is running (starts the exe if the Windows service isn't installed)
PDStepperClient.EnsureApiRunning("PD_Stepper_API.exe");

using var client = new PDStepperClient("http://localhost:8000");

// Subscribe to events before connecting
client.OnPositionReached += e => Console.WriteLine($"Reached {e.PosMm:F2} mm");
client.OnStall           += e => Console.WriteLine($"Step loss at {e.PosMm:F2} mm");
client.OnStatusUpdate    += e => Console.Write($"\r{e.PosMm,8:F3} mm  {e.VoltageV,5:F2} V   ");

// Scan and connect
var devices = await client.ScanAsync();
await client.ConnectAsync(devices[0].Address);

// Start WebSocket event stream (runs on a background thread)
await client.StartEventStreamAsync();

// Configure belt (GT2 20-tooth = 40 mm/rev)
await client.ConfigureBeltAsync(40.0);

// Move to 100 mm and wait for completion
bool ok = await client.MoveAndWaitMmAsync(100.0, TimeSpan.FromSeconds(15));
Console.WriteLine(ok ? "Done" : "Timed out or stalled");

// Cleanup
await client.StopEventStreamAsync();
await client.DisconnectAsync();
```

---

## API Reference

### Constructor

```csharp
new PDStepperClient(string baseUrl = "http://localhost:8000")
```

### Discovery & Connection

```csharp
Task<List<DeviceInfo>> ScanAsync(double durationSeconds = 5.0, CancellationToken ct)
Task ConnectAsync(string address, CancellationToken ct)
Task DisconnectAsync(CancellationToken ct)
Task<StatusResponse?> GetStatusAsync(CancellationToken ct)
Task<JsonElement> GetConfigAsync(CancellationToken ct)
```

### Motion — Millimetre-based (recommended)

```csharp
Task MoveAbsoluteMmAsync(double mm, CancellationToken ct)
Task MoveRelativeMmAsync(double mm, CancellationToken ct)
Task SetVelocityMmAsync(double mmPerS, CancellationToken ct)   // negative = reverse, 0 = stop
Task<bool> MoveAndWaitMmAsync(double mm, TimeSpan timeout, CancellationToken ct)
```

`MoveAndWaitMmAsync` returns `true` when `position_reached` fires, `false` on stall or timeout.

### Motion — Degree-based

```csharp
Task MoveAbsoluteAsync(double degrees, CancellationToken ct)
Task MoveRelativeAsync(double degrees, CancellationToken ct)
Task SetVelocityAsync(double dps, CancellationToken ct)
Task<bool> MoveAndWaitAsync(double degrees, TimeSpan timeout, CancellationToken ct)
Task StopAsync(CancellationToken ct)
Task HomeAsync(CancellationToken ct)
```

### Configuration

```csharp
Task ConfigureAsync(ConfigureRequest config, CancellationToken ct)
Task SetEnabledAsync(bool enabled, CancellationToken ct)
Task ConfigureBeltAsync(double mmPerRev, CancellationToken ct)
Task<(double MmPerRev, double MmPerDeg)> GetBeltConfigAsync(CancellationToken ct)
```

**ConfigureRequest** — all fields nullable (omit to leave unchanged):
```csharp
new ConfigureRequest
{
    CurrentMa        = 800,   // RMS current in mA
    Microsteps       = 16,    // 1/2/4/8/16/32/64/256
    SpeedSps         = 800,   // position move speed (steps/sec)
    ClosedLoopType   = 1,     // 0=open loop, 1=encoder closed
    MappingDirection = 1,     // 1=normal, -1=reversed
}
```

### Real-time Events

```csharp
Task StartEventStreamAsync(CancellationToken ct)
Task StopEventStreamAsync()

event Action<StepperEvent> OnStatusUpdate      // fires every 200 ms
event Action<StepperEvent> OnPositionReached   // motor settled at target
event Action<StepperEvent> OnStall             // step loss detected
event Action<StepperEvent> OnEndstop           // endstop switch triggered
event Action<StepperEvent> OnHomeComplete      // homing finished
event Action<StepperEvent> OnAck               // command acknowledged
event Action<StepperEvent> OnError             // firmware error
event Action<StepperEvent> OnAnyEvent          // all of the above
```

Events fire on a background thread — marshal to UI thread as needed (e.g. `Dispatcher.Invoke` in WPF).

### Static Helper

```csharp
[SupportedOSPlatform("windows")]
static void EnsureApiRunning(string exePath = "PD_Stepper_API.exe")
```

Checks for the `PDStepperAPI` Windows service; if not installed, launches the exe directly. Call once at application startup before constructing `PDStepperClient`.

---

## StepperEvent Fields

```csharp
record StepperEvent(
    StepperEventType Type,
    double?  PosDeg,      // current position (degrees)
    double?  TargetDeg,   // commanded target (degrees)
    double?  VelDps,      // velocity (degrees/sec)
    double?  VoltageV,    // VBUS voltage (V)
    double?  PosMm,       // current position (mm) — requires belt config
    double?  TargetMm,    // commanded target (mm)
    double?  VelMmS,      // velocity (mm/s)
    string?  Message,     // error message text
    string?  Cmd,         // command name (in ack events)
    bool?    Ok,          // command success (in ack events)
    long     Timestamp    // firmware millis() timestamp
)
```

---

## Running the Test Program

The test program exercises all major features against a running API and connected motor:

```powershell
cd csharp
dotnet run --project PDStepperTest
```

What it does:
1. Scans for devices and prints name/address/RSSI
2. Connects and prints firmware version + current config
3. Starts WebSocket event stream (live position display)
4. **Test 1** — Move to 90°, await `position_reached`
5. **Test 2** — Jog −45° relative
6. **Test 3** — Velocity 120°/s for 2 seconds
7. **Test 4** — Return to 0°
8. **Test 5** — Write `current_ma = 900`, read back to confirm round-trip

Prerequisites: API server running (`python main.py`), board powered and firmware flashed.

---

## Building the Solution

```powershell
cd csharp
dotnet build PDStepper.sln
```

Projects: `PDStepperClient` (library) + `PDStepperTest` (console app).
