# MIDI I/O stack (current)

## Transports

Firmware routes MIDI over:

- **USB MIDI device class** (`usbMidiWrite` / `usbMidiRead`) via TinyUSB MIDI interface enablement in firmware.
- **BLE MIDI** (`bleMidiWrite` / `m5ChordBleMidiRead`) on standard BLE MIDI service/characteristic UUIDs.
- **DIN** ingress via UART (`m5ChordDinMidiRead`) for physical MIDI-IN pins.

## Implemented

- Note on/off, control change (used by X–Y pad), and program change.
- Outgoing MIDI fan-out to USB + BLE transport writers.
- Shared parser pipeline with source tagging for USB / BLE / DIN ingress.
- Outgoing realtime transport emission (Clock/Start/Continue/Stop) on selected MIDI transport route.
- **Play surface:** surround pads and tonic/center (as **I** chord) and **surprise** chords use `ChordModel` spell, **`arpeggiatorOrderedIntervals`** (`Chord notes` vs `Voicing arp`), **`g_chordVoicing`**, and **global transpose** (`AppSettings::transposeSemitones`).
- **NVS:** `schema` key (version **1**) written on migration and on settings save (`prefsMigrateOnBoot` in `setup()`).

## Remaining limitations

- USB MIDI class enumeration and host compatibility need additional hardware matrix validation across phone/DAW hosts.
- Final on-device validation matrix for all transports is still tracked in `docs/HARDWARE_E2E_CHECKLIST.md`.
