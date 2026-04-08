# E2E Status

Date: 2026-04-08

## What was run

- `python -m platformio run -e m5stack-cores3`
- `python -m platformio test -e native`

## Results

- **Firmware build:** PASS
  - RAM: 6.6% (`21592 / 327680`)
  - Flash: 6.4% (`422037 / 6553600`)
- **Native unit tests:** BLOCKED
  - Error: `gcc`/`g++` not found in PATH on this Windows machine.

## Practical E2E conclusion

For an embedded touch UI project, true E2E also requires physical hardware validation on `M5Stack CoreS3`:

- touch gesture detection (two-finger long press)
- settings navigation and persistence through reboot
- MIDI behavior with a real device and channel filtering

The software build is healthy, but **hardware-in-the-loop E2E remains pending**.

## Unblock native tests

Install a local GCC toolchain (for example MSYS2 `mingw-w64`) and ensure `gcc`/`g++` are available in terminal PATH, then re-run:

```bash
python -m platformio test -e native
```
