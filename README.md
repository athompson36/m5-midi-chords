# M5Stack CoreS3 Chord Suggester

A key-aware chord suggestion firmware for the **M5Stack CoreS3** (ESP32-S3). Select a musical key and see its diatonic chords color-coded by harmonic importance — then discover surprise chords for creative depth.

Based on the [original round-display prototype](https://www.youtube.com/watch?v=bHaTqlVzRXw), adapted for the CoreS3's 320×240 rectangular touchscreen. See `docs/ORIGINAL_UX_SPEC.md` for the full design reference.

## User Experience

### Boot Splash
Power on → a centered **"Hi! Let's Play"** button appears. Tap to enter the play surface.

### Play Surface
- **Bottom bezel**: **BACK** or **FWD** (short tap) opens the **Sequencer** page (4×4 step grid). **SELECT** tap is reserved for future use on that row. From the sequencer, **BACK** or **FWD** returns to the chord play surface. **Hold BACK+FWD** ~0.8 s still opens **Settings** (from play or sequencer).
- **Center square**: shows **tonic name** and **mode** (e.g. `C` / `maj`). **Hold SELECT** (bezel) **and** press the **key center** together to open the **Key & mode** picker: choose one of **12 tonics**, a **mode** (Major, Natural minor, Harmonic minor, Dorian, Mixolydian), then **Done** (or **Cancel**). A normal tap on the key alone plays the tonic (or the surprise heart when lit).
- **8 square chord buttons** arranged in a ring around the key (3×3 layout with the key in the middle): **I**, **ii**, **iii**, **IV**, **V**, **vi**, **vii°**, and **♭VII** (borrowed), color-coded:
  - **Green** — principal (I, IV, V)
  - **Blue** — standard diatonic (ii, iii, vi)
  - **Red** — tension (vii°)
  - **Pink** — borrowed **♭VII**
- Tap any chord to "play" it (visual feedback now; MIDI output in a future milestone).

### Sequencer (basic UI)
A **16-step** grid (**4 bars × 4 beats**) is available from the play surface via the **BACK** or **FWD** bezel. **Tap a step** to **cycle** its value: **rest** (`-`) → **tie** (`~`) → **surround chords** in order → rest (placeholder until the chord dropdown from `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md` is implemented). The pattern is in RAM only until NVS storage is added. Playback and Shift modifier are not implemented yet.

### Heart / Surprise Chord
After tapping **5 chord buttons**, the key area transforms into a **heart (♥)**. Tapping it plays a surprise chord — a colorful voicing outside the standard diatonic set (modal interchange, Neapolitan, secondary dominant, etc.). The heart resets after use.

### Settings
From the **play** or **sequencer** surface, hold **BACK** and **FWD** on the bottom bezel together **~0.8 s** to open settings.

| Setting | Range | Default |
|---------|-------|---------|
| MIDI out channel | 1–16 | 1 |
| MIDI in channel | OMNI or 1–16 | OMNI |
| Brightness | 10–100% | 80% |
| Output velocity | 40–127 | 100 |

Navigate with **BACK** / **FWD** (row) and **SELECT** (change value). Rows **Backup to SD card** / **Restore from SD card** copy or load **`/m5chord_backup.txt`** on a FAT32 microSD (CoreS3 slot). Last row: **Save & exit** (persists settings, key, and sequencer pattern to NVS flash).

### Planned: Sequencer completion & Shift layer

The basic **sequencer grid** and **bezel paging** (Play ↔ Sequencer) are in the firmware; remaining work includes a dedicated **X–Y MIDI page** (freely assignable CCs, e.g. filter/FX), **chord dropdown** per step, **MIDI playback**, **Shift** = long-press **SELECT**, **per-step clock division**, **per-step arpeggiator**, **Shift+BACK/FWD** parameter edits, and expanded **NVS/SD** schema — all in **`docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`**.

### Planned: MIDI transports, clock, BPM (not implemented yet)

Design is captured in **`docs/MIDI_INPUT_SPEC.md`**:

- **Independent MIDI IN channel filters** for **USB**, **Bluetooth**, and **DIN** (DIN reserved for a future hardware revision or add-on).
- **MIDI clock follow** with a **small corner BPM** readout that **flashes on the beat** (discrete, low-contrast).

## Project Layout

- `platformio.ini` — build and test environments
- `src/main.cpp` — CoreS3 display + touch UI (boot, play, settings screens)
- `src/ChordModel.h` / `.cpp` — key-aware chord generation, roles, surprise pool
- `src/AppSettings.h` / `.cpp` — settings data and cycling logic
- `src/SettingsStore.h` / `.cpp` — NVS persistence via ESP32 Preferences (settings, key/mode, 16-step pattern)
- `src/SdBackup.h` / `.cpp` — optional microSD backup/restore (`/m5chord_backup.txt`, CoreS3 SPI pins)
- `test/test_chord_model/` — chord model unit tests
- `test/test_app_settings/` — settings unit tests
- `variants/m5stack_cores3/` — local board variant pin definitions
- `docs/ORIGINAL_UX_SPEC.md` — original UX reference spec
- `docs/MIDI_INPUT_SPEC.md` — MIDI transports, per-interface channels, clock/BPM UI
- `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md` — **planned** 4×4 sequencer, bezel paging, Shift modifier (long-press SELECT)
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
