# Packaging & Installation

Instructions for building the standalone Python API executable and creating a Windows installer that can be bundled with a C# application.

## Overview

```
build.bat
  └── PyInstaller → dist/PD_Stepper_API.exe   (self-contained, no Python needed)

PD_Stepper_Setup.iss
  └── Inno Setup  → Output/PD_Stepper_Setup.exe
        ├── Installs PD_Stepper_API.exe
        ├── Installs nssm.exe
        └── Registers PDStepperAPI as a Windows auto-start service
```

---

## Step 1 — Build the Python EXE

### Prerequisites

- Python 3.10+ with pip
- Dependencies already installed (`pip install -r api/requirements.txt`)

### Build

```powershell
cd api
build.bat
```

Output: `api/dist/PD_Stepper_API.exe`

The EXE is fully self-contained — it embeds Python, all libraries, and the browser UI. No Python installation required on target machines.

**Minimum target OS:** Windows 10 version 1803+ (WinRT BLE stack dependency)

### Test the EXE

```powershell
.\api\dist\PD_Stepper_API.exe
```

Navigate to http://localhost:8000 to confirm the UI loads.

---

## Step 2 — Download NSSM

NSSM (Non-Sucking Service Manager) wraps the Python EXE as a proper Windows service so it starts automatically with Windows.

1. Download the **64-bit** `nssm.exe` from https://nssm.cc/download
2. Place it in the `installer/` directory alongside the `.iss` file

---

## Step 3 — Build the Installer

### Prerequisites

- [Inno Setup 6](https://jrsoftware.org/isinfo.php) installed

### Build

1. Open `installer/PD_Stepper_Setup.iss` in Inno Setup
2. Click **Compile** (or press F9)

Output: `installer/Output/PD_Stepper_Setup.exe`

---

## What the Installer Does

1. Installs `PD_Stepper_API.exe` to `C:\Program Files\PD Stepper API\`
2. Installs `nssm.exe` alongside it
3. Registers the service: `nssm install PDStepperAPI "...PD_Stepper_API.exe"`
4. Configures auto-start: `nssm set PDStepperAPI Start SERVICE_AUTO_START`
5. Starts the service immediately
6. Offers to open http://localhost:8000 after install

**Uninstall** (via Windows Add/Remove Programs or `PD_Stepper_Setup.exe /uninstall`) stops and removes the service cleanly.

---

## Integrating with a C# Application Installer

### Option A — Run the PD-Stepper installer as a prerequisite

In your C# Inno Setup script:

```ini
[Files]
Source: "PD_Stepper_Setup.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Run]
Filename: "{tmp}\PD_Stepper_Setup.exe"; Parameters: "/silent /norestart"; \
    StatusMsg: "Installing PD-Stepper API service..."; \
    Flags: waituntilterminated
```

### Option B — Bundle the EXE directly (no service, simpler)

If you prefer to launch the API from C# code rather than a service:

```ini
[Files]
Source: "api\dist\PD_Stepper_API.exe"; DestDir: "{app}"; Flags: ignoreversion
```

Then in your C# application startup:

```csharp
PDStepperClient.EnsureApiRunning(
    Path.Combine(AppContext.BaseDirectory, "PD_Stepper_API.exe")
);
```

`EnsureApiRunning` first checks for the `PDStepperAPI` Windows service; if that isn't running it launches the EXE directly as a background process. Either way, the API is available at `http://localhost:8000`.

---

## Service Management

```powershell
# Check service status
Get-Service PDStepperAPI

# Start / stop manually
Start-Service PDStepperAPI
Stop-Service PDStepperAPI

# View logs (if NSSM configured log output)
Get-Content "$env:AppData\nssm\PDStepperAPI.log" -Tail 50
```

---

## Changing the Port

By default the API listens on port 8000. To change it, edit the NSSM service parameters after install:

```powershell
nssm set PDStepperAPI AppParameters "--port 9000"
Restart-Service PDStepperAPI
```

Update `PDStepperClient` base URL accordingly:
```csharp
new PDStepperClient("http://localhost:9000")
```
