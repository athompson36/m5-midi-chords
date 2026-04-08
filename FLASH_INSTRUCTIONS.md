# M5Stack CoreS3 — chord firmware (prebuilt images)

**Firmware bundle date: 2026-04-08**

This package contains prebuilt images for an **M5Stack CoreS3** (ESP32-S3) running the chord-suggester firmware built with PlatformIO (`m5stack-cores3` environment).

## Contents

- **bootloader.bin** — ESP32-S3 second-stage bootloader  
- **partitions.bin** — partition table  
- **firmware.bin** — application image  

## What to expect after flashing

- Boot UI title: **M5 CoreS3 Chord Suggester**  
- Bottom soft buttons: **BACK**, **SELECT**, **FWD** (chord navigation)  
- **Settings**: on the main screen, touch **BACK** and **FWD** together with two fingers and hold about **0.8 s**  
- In settings, **BACK** / **FWD** move the row; short **SELECT** changes values or saves on the last row  

See `README.md` for the full behavior summary.

## Requirements

- Python 3.8+ recommended  
- USB cable with data lines (not charge-only)  
- [esptool](https://github.com/espressif/esptool) (`pip install esptool`)  

## Option 1 — Python flasher (recommended)

From the folder that contains `flash_firmware.py` and the three `.bin` files:

**Windows**

```bash
python flash_firmware.py
```

Or double-click `flash_firmware.bat` (runs the same script).

**macOS / Linux**

```bash
python3 flash_firmware.py
```

The script picks a serial port when possible and flashes at `0x0`, `0x8000`, and `0x10000`.

## Option 2 — Manual `esptool` command

Replace the port with your system’s serial device (for example `COM5` on Windows or `/dev/ttyACM0` on Linux).

**Windows (PowerShell / CMD)**

```text
python -m esptool --port COM5 --baud 921600 --before default_reset --after hard_reset write_flash 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

**macOS / Linux**

```bash
python3 -m esptool --port /dev/ttyACM0 --baud 921600 --before default_reset --after hard_reset write_flash \
  0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

If uploads time out, retry with `--baud 115200`.

### Finding the serial port

- Windows: Device Manager → Ports (COM & LPT), or `python -m esptool list-ports`  
- macOS: `ls /dev/cu.usbmodem*` or `ls /dev/cu.usbserial*`  
- Linux: `ls /dev/ttyACM*` / `ls /dev/ttyUSB*`  

## Download / bootloader mode (if the port does not appear)

On CoreS3, if the board does not enumerate for flashing:

1. Connect USB-C to the CoreS3.  
2. Hold **BOOT** (download).  
3. Press **RST** once, then release **BOOT** after a short moment.  

Exact button labels match the silkscreen on your unit; some revisions use **RESET** instead of **RST**.

## Option 3 — Build and upload from source

If you have the full repository:

```bash
pip install platformio
python -m platformio run -e m5stack-cores3 --target upload
```

This uses PlatformIO’s upload path and does not require copying `.bin` files by hand.

## Troubleshooting

**Port not found**

- Try another USB port and a known-good data cable.  
- Enter download mode (see above).  
- On Windows, install the USB serial driver your machine reports for the device (ESP32-S3 often appears as a USB CDC/serial device).  

**Flash errors or bad boot after flash**

```bash
python -m esptool --port YOUR_PORT erase_flash
```

Then flash the three images again.

**Wrong or stale binaries**

- Rebuild with `python -m platformio run -e m5stack-cores3` and copy fresh files from `.pio/build/m5stack-cores3/`, or compare sizes/MD5 with `BUILD_INFO.md`.

## Source

Clone this repository and use the `m5stack-cores3` environment in `platformio.ini`. Dependencies are pulled automatically by PlatformIO (`M5Unified`, etc.).
