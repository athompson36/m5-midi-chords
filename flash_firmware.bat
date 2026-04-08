@echo off
REM M5Stack CoreS3 firmware flasher (Windows batch)
REM Requires: Python 3.x with esptool installed

setlocal enabledelayedexpansion

title M5 CoreS3 Firmware Flasher

echo.
echo ============================================================
echo  M5Stack CoreS3 Chord Suggester - Firmware Installer
echo ============================================================
echo.

REM Check Python
python --version >nul 2>&1
if errorlevel 1 (
    echo Error: Python 3 not found!
    echo Download: https://www.python.org/downloads/
    echo.
    pause
    exit /b 1
)

REM Check esptool
python -m esptool version >nul 2>&1
if errorlevel 1 (
    echo Installing esptool...
    python -m pip install esptool
    if errorlevel 1 (
        echo Error: Could not install esptool!
        pause
        exit /b 1
    )
)

echo Found Python and esptool.
echo.

REM Use Python script
echo Starting Firmware Flasher...
echo.

python flash_firmware.py

if errorlevel 0 (
    echo.
    echo Firmware installation complete!
) else (
    echo.
    echo Firmware installation failed. See errors above.
)

echo.
pause
