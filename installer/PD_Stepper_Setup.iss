; PD-Stepper API Installer
; Requires: Inno Setup 6.x (https://jrsoftware.org/isinfo.php)
; Before building: run api\build.bat to produce dist\PD_Stepper_API.exe
;                  place nssm.exe (64-bit) in this directory

#define AppName       "PD Stepper API"
#define AppVersion    "1.0.0"
#define AppPublisher  "PD Stepper"
#define ServiceName   "PDStepperAPI"
#define ExeName       "PD_Stepper_API.exe"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputBaseFilename=PD_Stepper_Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
MinVersion=10.0.17134   ; Windows 10 1803 (BLE WinRT stack minimum)
ArchitecturesInstallIn64BitMode=x64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; API executable (built by build.bat)
Source: "..\api\dist\{#ExeName}"; DestDir: "{app}"; Flags: ignoreversion

; NSSM service manager (bundle 64-bit nssm.exe in installer\ directory)
Source: "nssm.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\PD Stepper Control UI"; Filename: "http://localhost:8000"; IconFilename: "{app}\{#ExeName}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Run]
; Install as Windows auto-start service
Filename: "{app}\nssm.exe"; Parameters: "install {#ServiceName} ""{app}\{#ExeName}"""; \
    Flags: runhidden waituntilterminated; StatusMsg: "Registering service..."

Filename: "{app}\nssm.exe"; Parameters: "set {#ServiceName} DisplayName ""PD Stepper BLE API"""; \
    Flags: runhidden waituntilterminated

Filename: "{app}\nssm.exe"; Parameters: "set {#ServiceName} Description ""BLE-to-REST bridge for PD-Stepper motor controller"""; \
    Flags: runhidden waituntilterminated

Filename: "{app}\nssm.exe"; Parameters: "set {#ServiceName} Start SERVICE_AUTO_START"; \
    Flags: runhidden waituntilterminated

Filename: "{app}\nssm.exe"; Parameters: "start {#ServiceName}"; \
    Flags: runhidden waituntilterminated; StatusMsg: "Starting service..."

; Open browser to UI after install (optional)
Filename: "http://localhost:8000"; Description: "Open PD-Stepper Control UI"; \
    Flags: postinstall shellexec skipifsilent; StatusMsg: ""

[UninstallRun]
Filename: "{app}\nssm.exe"; Parameters: "stop {#ServiceName}"; \
    Flags: runhidden waituntilterminated

Filename: "{app}\nssm.exe"; Parameters: "remove {#ServiceName} confirm"; \
    Flags: runhidden waituntilterminated

[Code]
// Verify Windows 10 1803+ for WinRT BLE support
function InitializeSetup(): Boolean;
var
  Version: TWindowsVersion;
begin
  GetWindowsVersionEx(Version);
  if (Version.Major < 10) or
     ((Version.Major = 10) and (Version.Build < 17134)) then
  begin
    MsgBox('PD Stepper API requires Windows 10 version 1803 or later for Bluetooth support.',
           mbError, MB_OK);
    Result := False;
  end else
    Result := True;
end;
