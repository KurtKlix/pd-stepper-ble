@echo off
setlocal

cd /d "%~dp0"

echo [1/3] Installing dependencies...
pip install -r requirements.txt
pip install pyinstaller
if errorlevel 1 goto :err

echo [2/3] Building executable...
pyinstaller PD_Stepper_API.spec --clean --noconfirm
if errorlevel 1 goto :err

echo [3/3] Done!
echo Output: %~dp0dist\PD_Stepper_API.exe
goto :eof

:err
echo Build failed.
exit /b 1
