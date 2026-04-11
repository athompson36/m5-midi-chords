# E2E Status

> **Note:** This file records **point-in-time** build/test runs, not live CI. Update the date and sections when re-validating; see [`DOCS_INDEX.md`](DOCS_INDEX.md).

**How to read older blocks:** Each dated section reflects the toolchain and hardware available that day. Conclusions (for example “native tests blocked”) are **environment-specific** and may be **superseded** by later entries on a machine with a working host compiler. Prefer the **latest** dated block for current expectations; use earlier blocks only as history.

---

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

---

Date: 2026-04-09 (follow-up MIDI/transport pass)

## What was run

- `py -m platformio run -e m5stack-cores3`
- `py -m platformio run -e m5stack-cores3 -t upload`
- `py -m platformio test -e native`
- Code + docs audit focused on `main.cpp`, `MidiOut.cpp`, `MidiIngress.cpp`, and `docs/MIDI_*`.

## Results

- **Firmware build:** PASS
  - RAM: 13.7% (`44868 / 327680`)
  - Flash: 12.4% (`814481 / 6553600`)
- **Flash to CoreS3:** PASS (`COM4`, esptool upload + RTS reset)
- **Native unit tests:** BLOCKED on host toolchain
  - Fails before test execution because `gcc` / `g++` are missing from PATH on this Windows host.

> **Superseded (environment):** The host-blocked result above applies only to that Windows setup. Later dated sections on hosts with `gcc`/`g++` show native tests passing.

## MIDI + transport findings from this pass

- Sequencer SELECT was not latched on tap; top-row tools depended on hold-state and could not stay toggled.
- Panic path only cleared tracked notes for channel 1, not all channels.
- External clock ticks were gated by an extra source check that could block transport follow even when receive-route matched.
- USB path remains raw byte stream transport (serial-style), not class-compliant USB MIDI device mode.

---

Date: 2026-04-09 (full MIDI stack integration pass)

## What was run

- `py -m platformio run -e m5stack-cores3`
- `py -m platformio run -e m5stack-cores3 -t upload`
- Extended `test/test_midi_ingress/test_midi_ingress.cpp` with USB provider-hook coverage.

## Results

- **Firmware build:** PASS
  - RAM: 17.3% (`56836 / 327680`)
  - Flash: 12.9% (`846429 / 6553600`)
- **Flash to CoreS3:** PASS (`COM4`, esptool upload + RTS reset)
- **Native unit tests:** host-blocked on this machine (`gcc`/`g++` missing in PATH)

## Stack integration notes

- USB transport now enables TinyUSB MIDI interface path in firmware (`UsbMidiTransport`), replacing USB raw-byte-only behavior for MIDI routing.
- BLE TX framing updated to emit timestamped per-message BLE-MIDI packets for chord bursts and realtime messages.
- Transport now emits outgoing realtime clock/start/continue/stop on configured route while avoiding external-clock echo.
- Hardware interoperability matrix remains pending across phone synth apps / DAWs and should be recorded in `docs/HARDWARE_E2E_CHECKLIST.md`.
