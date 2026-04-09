# E2E Status

Date: 2026-04-08

## What was run

- `pio run -e m5stack-cores3`
- `pio test -e native`

## Results

- **Firmware build:** PASS
  - RAM: 6.7% (`21840 / 327680`)
  - Flash: 6.5% (`422881 / 6553600`)
- **Native unit tests:** PASS (20/20)
  - `test_chord_model`: 11 tests — key cycling, diatonic generation, roles, heart mechanic, surprise pool
  - `test_app_settings`: 4 tests — row count, normalize clamps, MIDI out wrap, MIDI in OMNI
  - `test_settings_entry_gesture`: 5 tests — BACK+FWD bezel coverage, 800 ms hold, release/reset, need-release clear

## Practical E2E conclusion

Manual hardware runs should follow **`docs/HARDWARE_E2E_CHECKLIST.md`** and record results here or in that file.

For an embedded touch UI project, true E2E also requires physical hardware validation on `M5Stack CoreS3`:

- cold boot → **Play** surface (no splash gate in current firmware)
- key cycling and chord grid display
- heart / surprise chord after 5 plays
- two-finger long-press gesture to enter settings
- settings navigation and persistence through reboot
- MIDI behavior with a real device (future milestone)

The software build and model logic are healthy, but **hardware-in-the-loop E2E remains pending** (spare CoreS3 unit ready).

---

Date: 2026-04-09

## What was run

- `pio run -e m5stack-cores3`
- `pio test -e native`
- Reviewed `docs/HARDWARE_E2E_CHECKLIST.md` for manual CoreS3 pass logging.

## Results

- **Firmware build:** PASS
  - RAM: 13.7% (`44844 / 327680`)
  - Flash: 12.4% (`813045 / 6553600`)
- **Native unit tests:** PASS (43/43)

## Hardware checklist execution status

- Manual on-device checklist execution is still pending.
- Next manual run should record per-section PASS/FAIL in either:
  - `docs/HARDWARE_E2E_CHECKLIST.md` (inline checkboxes), or
  - this file under a new dated block with tester/sign-off.
