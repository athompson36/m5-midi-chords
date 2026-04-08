# M5Stack CoreS3 Chord Firmware (from scratch)

This project is a clean PlatformIO firmware rebuild targeting `M5Stack CoreS3`.

Current behavior:

- CoreS3 display UI with adaptive width/height (not tied to CrowPanel round layout).
- Bottom soft-touch controls:
  - left: `BACK` (previous chord in the list)
  - center: `SELECT`
  - right: `FWD` (next chord in the list)
- **Settings**: on the main screen, place **two fingers** on **BACK** and **FWD** at the same time and hold about **0.8 s**. Requires multi-touch (CoreS3 reports up to 3 points via M5Unified).
- **Settings page**: `BACK` / `FWD` move the highlighted row; short `SELECT` changes the value (or **Save & exit** on the last row). Values are stored in NVS flash (`Preferences`).
- Adjustable items today: MIDI out channel (1–16), MIDI in (OMNI or CH 1–16), brightness (10–100%), suggested-note velocity (40–127).
- Chord selection model with wrap-around navigation.
- Unit tests for chord list and settings math (native build needs `gcc`/`g++` on PATH).

## Project Layout

- `platformio.ini` - build/test environments
- `src/main.cpp` - CoreS3 display + touch UI
- `src/ChordModel.h` / `src/ChordModel.cpp` - simple chord navigation model
- `test/test_chord_model.cpp` - model tests
- `variants/m5stack_cores3/*` - local variant override used to fix missing `pins_arduino.h` in the installed framework package

## Build

```bash
python -m platformio run -e m5stack-cores3
```

Output firmware:

- `.pio/build/m5stack-cores3/firmware.bin`

## Upload

```bash
python -m platformio run -e m5stack-cores3 --target upload
```

## Run tests

```bash
python -m platformio test -e native
```

Note: on Windows, native tests need a local GCC toolchain in PATH (`gcc`, `g++`).
