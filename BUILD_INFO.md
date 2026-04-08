# Build Information

Firmware build date: April 8, 2026  
Target: **M5Stack CoreS3** (ESP32-S3, 16 MB flash)  
Platform: `espressif32` v6.7.0  
Framework: Arduino (ESP32 Arduino core via PlatformIO)

## Build configuration

Excerpt from `platformio.ini` (environment `m5stack-cores3`):

```ini
[env:m5stack-cores3]
platform = espressif32@~6.7.0
board = m5stack-cores3
framework = arduino
monitor_speed = 115200
board_build.variants_dir = variants
board_build.variant = m5stack_cores3
platform_packages =
  platformio/framework-arduinoespressif32@~3.20016.0
build_flags =
  -DARDUINO_USB_MODE=1
  -DARDUINO_USB_CDC_ON_BOOT=1
lib_deps =
  m5stack/M5Unified@^0.1.15
```

## Binary artifacts

These offsets are standard for this board profile. The same files are produced under `.pio/build/m5stack-cores3/` after a successful build.

| File             | Size (bytes) | Size (human) | Flash offset |
|------------------|-------------:|--------------|--------------|
| bootloader.bin   | 15,104       | 14.8 KiB     | 0x0          |
| partitions.bin   | 3,072        | 3.0 KiB      | 0x8000       |
| firmware.bin     | 422,400      | 412.5 KiB    | 0x10000      |

## MD5 checksums

Computed from the build synced to the repository root on 2026-04-08 (also matches `.pio/build/m5stack-cores3/` after `platformio run -e m5stack-cores3`).

| File           | MD5                              |
|----------------|----------------------------------|
| bootloader.bin | `56375b35ec88d866bcaf826644b86231` |
| partitions.bin | `118cbdbfce82eb12bbdeb7c59af2fce8` |
| firmware.bin   | `c8009b407fa4aa989a452d4cf0965a4c` |

## Memory usage (reference)

From the same build:

- RAM: about 6.6% (21,592 / 327,680 bytes)
- Flash: about 6.4% (422,037 / 6,553,600 bytes program storage)

## Rebuild

```bash
pip install platformio
python -m platformio run -e m5stack-cores3
python -m platformio run -e m5stack-cores3 --target upload
```

To refresh the prebuilt copies in the project root (for `flash_firmware.py` / `flash_firmware.bat`):

```bash
# After a successful build, from the project root:
cp .pio/build/m5stack-cores3/bootloader.bin .
cp .pio/build/m5stack-cores3/partitions.bin .
cp .pio/build/m5stack-cores3/firmware.bin .
```

Then recompute and update the MD5 table in this file if you publish a release.

## References

- [M5Stack CoreS3 docs](https://docs.m5stack.com/en/core/CoreS3)
- [PlatformIO board: m5stack-cores3](https://docs.platformio.org/en/latest/boards/espressif32/m5stack-cores3.html)
- [esptool](https://github.com/espressif/esptool)
