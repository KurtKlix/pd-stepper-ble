# Flashing PD-Stepper BLE Firmware

## Step 1 — Install Arduino IDE 2

Download and install **Arduino IDE 2.x** from https://www.arduino.cc/en/software

---

## Step 2 — Add ESP32 Board Support

1. Open Arduino IDE → **File → Preferences**
2. In **Additional boards manager URLs**, paste:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. Click OK
4. Open **Tools → Board → Boards Manager**
5. Search `esp32`, install **"esp32 by Espressif Systems"** — use the latest **3.x** release

---

## Step 3 — Install Libraries

Open **Tools → Manage Libraries** and install:

| Library | Author | Version |
|---------|--------|---------|
| **NimBLE-Arduino** | h2zero | latest (2.x for core 3.x) |
| **ArduinoJson** | Benoit Blanchon | **7.x** (not 6.x) |
| **TMCStepper** | teemuatlut | latest |

> NimBLE-Arduino version must match ESP32 core version:
> - ESP32 core 2.x → NimBLE-Arduino 1.x
> - ESP32 core 3.x → NimBLE-Arduino 2.x

---

## Step 4 — Open the Sketch

**File → Open** → `firmware/PD_Stepper_BLE/PD_Stepper_BLE.ino`

Arduino opens the whole folder as a multi-file sketch automatically.

---

## Step 5 — Configure Board Settings

**Tools** menu — set exactly these:

| Setting | Value |
|---------|-------|
| Board | **ESP32S3 Dev Module** |
| USB CDC On Boot | **Enabled** |
| USB Mode | **Hardware CDC and JTAG** |
| Upload Speed | **921600** |
| Flash Mode | **QIO 80MHz** |
| Flash Size | **4MB (32Mb)** |
| Partition Scheme | **Default 4MB with spiffs** |
| Core Debug Level | **None** |
| PSRAM | **Disabled** |

> Board search tip: type `S3 Dev` (not the full name) to find it.
> If you only see "ESP32S3 Dev Module Octal (WROOM2)", scroll — the plain "ESP32S3 Dev Module" is nearby.

---

## Step 6 — Connect & Enter Boot Mode

Plug the PD-Stepper in via USB-C.

**First-time flash — enter bootloader mode manually:**
1. Hold **BOOT** button
2. While holding BOOT, press and release **RESET**
3. Release **BOOT**

Select the COM port: **Tools → Port**

> After first successful flash, subsequent uploads are automatic — no BOOT+RESET needed.

---

## Step 7 — Flash

Click **Upload** (→) or press `Ctrl+U`.

Expected output:
```
Connecting........
Chip is ESP32-S3 ...
Uploading stub...
Wrote XXXXXX bytes ... in X.X seconds
Hash of data verified.
Hard resetting via RTS pin...
```

If `Failed to connect` → board isn't in bootloader mode, redo Step 6.

---

## Step 8 — Verify with Serial Monitor

1. **Tools → Serial Monitor**, baud rate **115200**
2. Press **RESET** on the board

Expected output:
```
[PD-Stepper BLE] starting...
[BLE] advertising as: PD-Stepper
[PD-Stepper BLE] ready
```

---

## Step 9 — Verify BLE with nRF Connect

Install **nRF Connect for Mobile** (Android/iOS):

1. Scanner tab → **Scan**
2. **"PD-Stepper"** should appear within a few seconds
3. Connect → find service `6E400001-...`
4. Subscribe (notify) on characteristic `6E400003`
5. Write `{"cmd":"status"}` (UTF-8) to `6E400002` → status JSON returns on the notify characteristic

---

## Common Issues

| Symptom | Fix |
|---------|-----|
| COM port not visible | Enter BOOT mode (Step 6) or try different USB cable |
| `NimBLE not found` compile error | Install "NimBLE-Arduino by h2zero" from Library Manager |
| `JsonDocument not found` | ArduinoJson is v6 — must install v7 |
| `TMC2209Stepper not found` | Install TMCStepper library |
| Serial Monitor shows garbage | Confirm 115200 baud |
| BLE device not visible | Check "USB CDC On Boot: Enabled" and reflash |
| `onWrite does not override` | NimBLE version mismatch — see firmware troubleshooting notes |
