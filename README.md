# M5Stack CoreS3 MIDI Chords

Touchscreen **MIDI instrument** firmware for the **M5Stack CoreS3** (ESP32-S3): chord performance, 16-step sequencer, X–Y CC control, transport/clock, and settings—with USB, BLE, and optional DIN MIDI paths.

Chord surfaces remain key-aware (diatonic chords, harmonic roles, heart/surprise flow). See `docs/ORIGINAL_UX_SPEC.md` for design lineage from the [round-display prototype](https://www.youtube.com/watch?v=bHaTqlVzRXw).

## User Experience

### Startup

On power-on the firmware goes **straight to the Play surface** (no splash gate). A classic **“Hi! Let's Play”** splash may be reintroduced later as an option.

### Bezel navigation (main ring)

Short tap **BACK** or **FWD** cycles the **three main surfaces**:

**Play → Sequencer → X–Y → …**

- **Play**: chord pad surface, key/mode, MIDI note output.
- **Sequencer**: 16-step grid (3 lanes; pattern and `SeqExtras` in **NVS**; optional SD project).
- **X–Y**: two-axis pad (CC assignments in NVS; MIDI CC via active transports).

**Transport** (clock, play/stop/rec, metronome, count-in, BPM, project name) opens as a **bottom sheet**: swipe **up from the SELECT bezel** (or the existing transport entry gesture in firmware). It is **not** part of the BACK/FWD ring.

**Hold BACK + FWD** together ~**0.8 s** opens **Settings** from Play, Sequencer, or X–Y.

**Top drawers** (swipe down from the top edge on Play / Sequencer / X–Y) expose page-specific secondary controls when implemented; see `docs/UI_REFACTOR_MASTER_PLAN.md`.

### Play surface

- **SELECT** (single tap): toggles **key-edit** mode — center key highlighted; tap center again to open **Key & mode** (12 tonics × modes). *(Older “hold SELECT + key” combos may still be described in legacy docs.)*
- **Center square**: tonic name and mode (e.g. `C` / `maj`). Tap plays tonic or **heart** / surprise when available.
- **Eight chord pads** in a 3×3 ring (**I** … **♭VII**), color-coded by harmonic role.
- Chord and tonic taps emit **MIDI notes** on the configured channel/velocity over USB + BLE transport paths (see `docs/MIDI_STACK.md`).

### Sequencer

**16-step** grid (**4 bars × 4 beats**), three **lanes**, tool row (quantize, swing, step probability, chord random). **Tap a step** to open a **picker** (surround chords, tie, rest, surprise token). Playback sends **MIDI** per step (lanes, swing, probability, etc.); **Shift** (hold **SELECT**) exposes per-step division/arp controls. Patterns and `SeqExtras` persist in **NVS** (and optional SD backup). See `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md` and `docs/TODO_IMPLEMENTATION.md`.

### Heart / Surprise chord

After **5** registered chord plays, the center shows a **heart**. Tapping it plays a **surprise** harmony from the pool; the counter resets when consumed.

### Settings

Section-based UI: **MIDI**, **Display**, **Seq/Arp**, **SD/Backup**, etc. Long-press **FWD** ~0.8 s from Play/Seq/XY can also open Settings (see firmware).

| Examples | Notes |
|----------|--------|
| MIDI out / in / **transpose** (−24…+24) / transport routes / clock | Per `AppSettings`; transpose affects play MIDI |
| Brightness, theme | Display section |
| Velocity, arpeggiator mode, BPM, project name | Seq/Arp section |

**Save** persists to NVS. **Backup / Restore** use SD when present (`docs` / `SdBackup`).

### Planned (see `docs/TODO_IMPLEMENTATION.md`)

- Remaining MIDI transport hardening and hardware E2E sign-off (see `docs/TODO_IMPLEMENTATION.md` and `docs/HARDWARE_E2E_CHECKLIST.md`).

## Project Layout

- `platformio.ini` — build and test environments
- `src/main.cpp` — CoreS3 display + touch UI
- `src/ChordModel.h` / `.cpp` — key-aware chord generation, roles, surprise pool
- `src/AppSettings.h` / `.cpp` — settings data and cycling logic
- `src/SettingsStore.h` / `.cpp` — NVS persistence (`m5chord` namespace; see `docs/PERSISTENCE_KEYS.md`)
- `src/SdBackup.h` / `.cpp` — optional microSD backup/restore
- `src/MidiOut.h` / `.cpp` — MIDI send fan-out to USB + BLE transport layers
- `src/DinMidiTransport.h` / `.cpp` — DIN UART MIDI ingress/egress bridge (MIDI CHIP compatible)
- `test/` — native unit tests
- `variants/m5stack_cores3/` — board variant
- `docs/DOCS_INDEX.md` — which docs are authoritative vs historical
- `docs/TODO_IMPLEMENTATION.md` — remaining feature backlog
- `docs/DEV_ROADMAP.md` — milestone history (verify against code)
- `docs/PERSISTENCE_KEYS.md` — NVS key table
- `docs/HARDWARE_E2E_CHECKLIST.md` — manual hardware verification
- `docs/DIN_MIDI_CHIP_SETUP.md` — wiring + pin override guide for 5-pin DIN board integration

## Build

```bash
pio run -e m5stack-cores3
```

Output: `.pio/build/m5stack-cores3/firmware.bin`

## Upload

```bash
pio run -e m5stack-cores3 --target upload
```

Or: `make upload`

## Run Tests

```bash
pio test -e native
```

Runs on the host (requires `gcc`/`g++` on PATH).
