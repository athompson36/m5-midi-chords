# E2E Status

Date: 2026-04-08

## What was run

- `pio run -e m5stack-cores3`
- `pio test -e native`

## Results

- **Firmware build:** PASS
  - RAM: 6.7% (`21840 / 327680`)
  - Flash: 6.5% (`422881 / 6553600`)
- **Native unit tests:** PASS (14/14)
  - `test_chord_model`: 11 tests — key cycling, diatonic generation, roles, heart mechanic, surprise pool
  - `test_app_settings`: 3 tests — normalize clamps, MIDI out wrap, MIDI in OMNI

## Practical E2E conclusion

For an embedded touch UI project, true E2E also requires physical hardware validation on `M5Stack CoreS3`:

- boot splash → play surface transition
- key cycling and chord grid display
- heart / surprise chord after 5 plays
- two-finger long-press gesture to enter settings
- settings navigation and persistence through reboot
- MIDI behavior with a real device (future milestone)

The software build and model logic are healthy, but **hardware-in-the-loop E2E remains pending** (spare CoreS3 unit ready).
