# MIDI output stack (current)

## Transport

Firmware sends **classic MIDI 1.0 byte streams** on **`Serial`** (USB CDC on CoreS3 with `ARDUINO_USB_CDC_ON_BOOT`). The host typically sees a **virtual COM port**, not a class-compliant USB MIDI device.

- **Development:** use a **serial ↔ MIDI bridge** (e.g. **Hairless MIDI**) on macOS/Windows/Linux, or a small Max/Pure Data patch, to route bytes to a DAW virtual MIDI port.
- **Production / DAW plug-and-play:** integrate a **USB MIDI device** stack (e.g. TinyUSB MIDI device) and replace `Serial.write` in `src/MidiOut.cpp` — see `docs/TODO_IMPLEMENTATION.md` Phase 1.

## Implemented

- Note on/off, control change (used by X–Y pad).
- **Play surface:** surround pads and tonic/center (as **I** chord) and **surprise** chords use `ChordModel` spell, **`arpeggiatorOrderedIntervals`** (`Chord notes` vs `Voicing arp`), **`g_chordVoicing`**, and **global transpose** (`AppSettings::transposeSemitones`).
- **NVS:** `schema` key (version **1**) written on migration and on settings save (`prefsMigrateOnBoot` in `setup()`).

## Not implemented

- MIDI IN, clock, per-transport channels — `docs/MIDI_INPUT_SPEC.md`.
- Class-compliant USB MIDI device descriptor on the ESP32 USB peripheral.
