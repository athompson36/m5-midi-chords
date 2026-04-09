# M5Stack CoreS3 Chord Suggester

A key-aware chord suggestion firmware for the **M5Stack CoreS3** (ESP32-S3). Select a musical key and see its diatonic chords color-coded by harmonic importance — then discover surprise chords for creative depth.

Based on the [original round-display prototype](https://www.youtube.com/watch?v=bHaTqlVzRXw), adapted for the CoreS3's 320×240 rectangular touchscreen. See `docs/ORIGINAL_UX_SPEC.md` for the full design reference.

## User Experience

### Startup

On power-on the firmware goes **straight to the Play surface** (no splash gate). A classic **“Hi! Let's Play”** splash may be reintroduced later as an option.

### Bezel navigation (main ring)

Short tap **BACK** or **FWD** cycles pages in a fixed ring:

**Transport → Play → Sequencer → X–Y → …**

- **Transport**: internal clock, play/stop/rec, metronome, count-in, BPM/project name.
- **Play**: chord pad surface (below).
- **Sequencer**: 16-step grid (3 lanes; pattern stored in **NVS**).
- **X–Y**: two-axis control pad (CC assignments in NVS; sends MIDI CC via active transports).

**Hold BACK + FWD** together ~**0.8 s** opens **Settings** from Transport, Play, Sequencer, or X–Y.

### Play surface

- **SELECT** (single tap): toggles **key-edit** mode — center key highlighted; tap center again to open **Key & mode** (12 tonics × modes). *(Older “hold SELECT + key” combos may still be described in legacy docs.)*
- **Center square**: tonic name and mode (e.g. `C` / `maj`). Tap plays tonic or **heart** / surprise when available.
- **Eight chord pads** in a 3×3 ring (**I** … **♭VII**), color-coded by harmonic role.
- Chord and tonic taps emit **MIDI notes** on the configured channel/velocity over USB + BLE transport paths (see `docs/MIDI_STACK.md`).

### Sequencer (basic UI)

**16-step** grid (**4 bars × 4 beats**), three **lanes**, tool row (quantize, swing, step probability, chord random). **Tap a step** to **cycle**: rest (`-`) → tie (`~`) → surround chord indices → … (a full **dropdown** picker is planned — `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`). Patterns and extras persist in **NVS** (and optional SD backup). **Audible step MIDI** and **Shift** layer are not implemented yet.

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
- `docs/DEV_ROADMAP.md` — milestones
- `docs/TODO_IMPLEMENTATION.md` — remaining feature backlog
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
