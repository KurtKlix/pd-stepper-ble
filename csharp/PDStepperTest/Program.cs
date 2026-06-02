using PDStepper;

Console.WriteLine("=== PD-Stepper Client Test ===");
Console.WriteLine("API must be running at http://localhost:8000");
Console.WriteLine();

using var client = new PDStepperClient("http://localhost:8000");

// ── Wire up event handlers before connecting ──────────────────────────────────
client.OnStatusUpdate    += e => Console.Write($"\r  pos: {e.PosDeg,8:F2}°   vel: {e.VelDps,7:F2}°/s   {e.VoltageV,5:F2} V   ");
client.OnPositionReached += e => Console.WriteLine($"\n[OK] Position reached: {e.PosDeg:F2}°");
client.OnStall           += e => Console.WriteLine($"\n[!!] Step loss at {e.PosDeg:F2}°");
client.OnEndstop         += e => Console.WriteLine($"\n[--] Endstop triggered at {e.PosDeg:F2}°");
client.OnHomeComplete    += e => Console.WriteLine($"\n[OK] Homing complete, zeroed at {e.PosDeg:F2}°");
client.OnError           += e => Console.WriteLine($"\n[ERR] {e.Message}");

// ── 1. Scan ───────────────────────────────────────────────────────────────────
Console.Write("Scanning for PD-Stepper devices (5 s)...");
var devices = await client.ScanAsync(5.0);

if (devices.Count == 0)
{
    Console.WriteLine("\nNo devices found. Make sure the board is powered and firmware is running.");
    return;
}

Console.WriteLine($"\nFound {devices.Count} device(s):");
for (int i = 0; i < devices.Count; i++)
    Console.WriteLine($"  [{i}] {devices[i].Name}  {devices[i].Address}  {devices[i].Rssi} dBm");

// Pick device (default 0, or let user choose if multiple)
int idx = 0;
if (devices.Count > 1)
{
    Console.Write("Select device index: ");
    idx = int.TryParse(Console.ReadLine(), out var n) ? n : 0;
}

// ── 2. Connect ────────────────────────────────────────────────────────────────
Console.Write($"\nConnecting to {devices[idx].Name} ({devices[idx].Address})...");
await client.ConnectAsync(devices[idx].Address);
Console.WriteLine(" connected.");

var cfg = await client.GetConfigAsync();
Console.WriteLine($"Firmware: {cfg.GetProperty("fw_ver")}  microsteps: {cfg.GetProperty("microsteps")}  current: {cfg.GetProperty("current_ma")} mA");

// Start WebSocket event stream
await client.StartEventStreamAsync();
Console.WriteLine("Event stream active. Starting tests...\n");
await Task.Delay(500);

// ── 3. Enable driver ──────────────────────────────────────────────────────────
await client.SetEnabledAsync(true);

// ── 4. Move to 90° and wait ───────────────────────────────────────────────────
Console.WriteLine("Test 1: Move to 90°");
bool ok = await client.MoveAndWaitAsync(90.0, TimeSpan.FromSeconds(15));
Console.WriteLine(ok ? "  PASS" : "  FAIL (timeout or stall)");
await Task.Delay(1000);

// ── 5. Jog -45° relative ─────────────────────────────────────────────────────
Console.WriteLine("\nTest 2: Jog -45° relative");
ok = await client.MoveAndWaitAsync(45.0, TimeSpan.FromSeconds(15));  // 90-45=45
Console.WriteLine(ok ? "  PASS" : "  FAIL");
await Task.Delay(1000);

// ── 6. Velocity mode for 2 s ─────────────────────────────────────────────────
Console.WriteLine("\nTest 3: Velocity 120°/s for 2 s");
await client.SetVelocityAsync(120.0);
await Task.Delay(2000);
await client.StopAsync();
Console.WriteLine("  PASS (stopped)");
await Task.Delay(500);

// ── 7. Return to 0° ───────────────────────────────────────────────────────────
Console.WriteLine("\nTest 4: Return to 0°");
ok = await client.MoveAndWaitAsync(0.0, TimeSpan.FromSeconds(15));
Console.WriteLine(ok ? "  PASS" : "  FAIL");
await Task.Delay(1000);

// ── 8. Configuration round-trip ───────────────────────────────────────────────
Console.WriteLine("\nTest 5: Configuration update");
await client.ConfigureAsync(new ConfigureRequest { CurrentMa = 900, Microsteps = 16 });
await Task.Delay(400); // wait for firmware's 200ms config refresh cycle
var cfg2 = await client.GetConfigAsync();
int ma = cfg2.GetProperty("current_ma").GetInt32();
Console.WriteLine(ma == 900 ? "  PASS (current_ma = 900)" : $"  FAIL (got {ma})");

// ── Done ──────────────────────────────────────────────────────────────────────
Console.WriteLine("\n=== All tests complete. Press Enter to disconnect. ===");
Console.ReadLine();

await client.StopEventStreamAsync();
await client.DisconnectAsync();
Console.WriteLine("Disconnected.");

