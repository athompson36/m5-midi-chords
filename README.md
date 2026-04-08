# M5Stack CoreS3 Chord Suggester

A key-aware chord suggestion firmware for the **M5Stack CoreS3** (ESP32-S3). Select a musical key and see its diatonic chords color-coded by harmonic importance — then discover surprise chords for creative depth.

Based on the [original round-display prototype](https://www.youtube.com/watch?v=bHaTqlVzRXw), adapted for the CoreS3's 320×240 rectangular touchscreen. See `docs/ORIGINAL_UX_SPEC.md` for the full design reference.

## User Experience

### Boot Splash
Power on → a centered **"Hi! Let's Play"** button appears. Tap to enter the play surface.

### Play Surface
- **Top center**: the current **key** (e.g. "C", "F#"). Tap to cycle through all 12 keys.
- **6 chord buttons** in a 3×2 grid, color-coded:
  - **Green** — principal chords (IV, V)
  - **Blue** — standard diatonic (ii, iii, vi)
  - **Red** — tension / diminished (vii°)
- Tap any chord to "play" it (visual feedback now; MIDI output in a future milestone).

### Heart / Surprise Chord
After tapping **5 chord buttons**, the key area transforms into a **heart (♥)**. Tapping it plays a surprise chord — a colorful voicing outside the standard diatonic set (modal interchange, Neapolitan, secondary dominant, etc.). The heart resets after use.

### Settings
From the play surface, place **two fingers** on the **left and right edges** of the screen and hold **~0.8 s** to open settings.

| Setting | Range | Default |
|---------|-------|---------|
| MIDI out channel | 1–16 | 1 |
| MIDI in channel | OMNI or 1–16 | OMNI |
| Brightness | 10–100% | 80% |
| Output velocity | 40–127 | 100 |

Navigate with **BACK** / **FWD** (row) and **SELECT** (change value). Last row: **Save & exit** (persists to NVS flash).

### Planned: MIDI transports, clock, BPM (not implemented yet)

Design is captured in **`docs/MIDI_INPUT_SPEC.md`**:

- **Independent MIDI IN channel filters** for **USB**, **Bluetooth**, and **DIN** (DIN reserved for a future hardware revision or add-on).
- **MIDI clock follow** with a **small corner BPM** readout that **flashes on the beat** (discrete, low-contrast).

## Project Layout

- `platformio.ini` — build and test environments
- `src/main.cpp` — CoreS3 display + touch UI (boot, play, settings screens)
- `src/ChordModel.h` / `.cpp` — key-aware chord generation, roles, surprise pool
- `src/AppSettings.h` / `.cpp` — settings data and cycling logic
- `src/SettingsStore.h` / `.cpp` — NVS persistence via ESP32 Preferences
- `test/test_chord_model/` — chord model unit tests
- `test/test_app_settings/` — settings unit tests
- `variants/m5stack_cores3/` — local board variant pin definitions
- `docs/ORIGINAL_UX_SPEC.md` — original UX reference spec
- `docs/MIDI_INPUT_SPEC.md` — MIDI transports, per-interface channels, clock/BPM UI
- `docs/DEV_ROADMAP.md` — development milestones

## Build

```bash
pio run -e m5stack-cores3
```

Output: `.pio/build/m5stack-cores3/firmware.bin`

## Upload

```bash
pio run -e m5stack-cores3 --target upload
```

## Run Tests

```bash
pio test -e native
```

Runs on the host machine (requires `gcc`/`g++` on PATH).
