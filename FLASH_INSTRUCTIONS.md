# CrowPanel 1.28" ESP32S3 Chord Suggester Firmware

**Firmware Version: 2026-04-08**

This package contains the precompiled firmware for the CrowPanel 1.28" Rotary Display with ESP32S3 for the interactive MIDI chord suggestion system.

## Firmware Components

- **bootloader.bin** - ESP32S3 bootloader (15 KB)
- **partitions.bin** - partition table (3 KB)
- **firmware.bin** - main application (1028.5 KiB)

## Hardware Features

- Rotary ring menu with 5 pages (diatonic, functional, related keys, chromatic, history)
- WS2812B RGB LEDs (5 units, pin 48) - each page has its own color
- Suggested chord playback mirrors learned user strumming timing and alternates strum direction
- BLE MIDI peripheral output for wireless MIDI clients
- USB MIDI device output over the CrowPanel USB-C port
- MIDI input/output (DIN MIDI at 31250 baud)
- Capacitive touchscreen (CST816D)
- Real-time chord detection

## Installation

PRESS AND HOLD BOOT AND RESET, then let go.

### Option 1: Simple Python Script (recommended)

**Requirements:**
- Python 3.6+ installed
- USB-C cable connected to the CrowPanel
- Press the boot button, then power on (or keep boot pressed until upload starts)

**Step 1: Install esptool**
```bash
pip install esptool
```

**Step 2: Flash firmware**

**Windows:**
```bash
python flash_firmware.py
```

**Mac/Linux:**
```bash
python3 flash_firmware.py
```

The script automatically detects the serial port and flashes all components in the correct order.

---

### Option 2: Manual Flashing with esptool.py

If the script does not work, run the command directly:

**Windows (CMD):**
```cmd
esptool.py --port COM3 --baud 921600 --before default_reset --after hard_reset write_flash ^
  0x0 bootloader.bin ^
  0x8000 partitions.bin ^
  0x10000 firmware.bin
```

**Mac/Linux (Bash):**
```bash
esptool.py --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

**Find port numbers:**
- Windows: `esptool.py list-ports` or check Device Manager
- Mac: `ls /dev/tty.usbserial*`
- Linux: `ls /dev/ttyUSB*` or `dmesg | tail`

---

### Option 3: Arduino IDE (if available)

1. Start Arduino IDE
2. Tools -> Flash Size: **16MB (128Mb)**
3. Tools -> Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
4. Tools -> PSRAM: **OPI PSRAM**
5. Tools -> Port: CrowPanel USB port
6. Sketch -> Upload

> Use the `.elf` file (Sketch -> Export compiled Binary)

---

## Troubleshooting

### Problem: "Port not found" / No connection

- Check the USB-C cable (data + power)
- Restart the CrowPanel in boot mode:
  1. Hold the **boot button**
  2. Press the power button (or unplug and reconnect)
  3. Release the boot button
  4. The LED indicator should glow dimly (download mode)
- Windows driver: [CH34x USB Driver](https://github.com/Elecrow-RD/CrowPanel-1.28inch-HMI-ESP32-Rotary-Display-240-240-IPS-Round-Touch-Knob-Screen/blob/master/driver)

### Problem: "Baud rate error" / Timing issue

- Reduce baud to 115200 (if 921600 times out):
  ```bash
  esptool.py --port COM3 --baud 115200 write_flash ...
  ```

### Problem: Firmware flashed, but device does not start

- Erase flash memory completely:
  ```bash
  esptool.py --port COM3 erase_flash
  ```
  Then flash again.

### Problem: MIDI does not work

- Check pins: RX=44, TX=43 (see `platformio.ini`)
- Check DIN MIDI cable and device routing
- Open Serial Monitor (115200 baud) for diagnostics

### Problem: BLE MIDI does not work

- Enable Bluetooth on your host device and search for the CrowPanel BLE MIDI device again
- Ensure no other app is already connected to the BLE MIDI endpoint
- Reboot the CrowPanel and retry pairing/connection

---

## LED Colors per Menu Page

When rotating the ring, you will see:

| Page | Color | RGB |
|------|-------|-----|
| **Chord History** | Orange | `255, 120, 20` |
| **Diatonic** | Green | `0, 200, 80` |
| **Functional** | Blue | `0, 120, 255` |
| **Related Keys** | Magenta | `220, 60, 180` |
| **Chromatic/Altered** | Red | `255, 40, 40` |

---

## Source Code and Modification

The original project is available on GitHub:
[miditest PlatformIO Project](https://github.com/username/miditest) *(adjust as needed)*

To build it yourself:
```bash
git clone <repo>
cd miditest
pip install platformio
pio run -e crowpanel_esp32s3
pio run -e crowpanel_esp32s3 --target upload
```

**Dependencies:**
- LovyanGFX 1.2.7+
- Adafruit_NeoPixel 1.12.3+
- MIDI Library 5.0.2+

---

## Support and Questions

- **Elecrow CrowPanel Wiki**: https://www.elecrow.com/wiki/CrowPanel_1.28inch-HMI_ESP32_Rotary_Display_Arduino_lesson1.html
- **esptool.py Documentation**: https://github.com/espressif/esptool

---

**Enjoy your chord suggester firmware.**
