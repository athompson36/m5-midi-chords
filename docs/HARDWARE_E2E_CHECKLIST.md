# Hardware E2E checklist (CoreS3)

Single place to record **manual** verification on a physical **M5Stack CoreS3**. Update dates and results inline or in `docs/E2E_STATUS.md`.

## Environment

- Firmware build ID / git SHA: _______________
- Date: _______________
- SD card present: yes / no

## Boot & navigation

- [ ] Cold boot: device reaches **Play** surface without fault.
- [ ] **BACK / FWD** bezel: cycles **Transport → Play → Sequencer → X–Y** (full ring).
- [ ] **Hold BACK + FWD** ~0.8 s: opens **Settings** from Play, Sequencer, Transport, X–Y.
- [ ] Settings **Save & exit**: returns to Play; values persist across reboot.

## Play surface

- [ ] Tonic + mode display; **SELECT** latch → tap center opens **Key & mode** picker; **Done** saves.
- [ ] Eight chord pads respond; **last-played** outline updates (no stuck rings after grid refresh).
- [ ] After 5 plays: **heart** state; tap consumes surprise (if applicable).
- [ ] Two-finger voicing gesture (if used): no crash; UI recovers.
- [ ] **Hit padding check**: tap near pad edges/corners (within ~5 px of border) still resolves to intended pad.
- [ ] **Tap debounce check**: very brief accidental brushes (<40 ms) do not trigger chord/key actions.

## Sequencer

- [ ] 16-step grid; step **cycle** (rest / tie / chords).
- [ ] Lane tabs; tool row (Q/S/P/Rnd) if enabled; pattern persists after reboot.

## Transport

- [ ] Play / Stop / Rec / Metro / Count-in behave as expected (speaker click if enabled).
- [ ] Count-in overlay on Play (if enabled).

## X–Y pad

- [ ] Touch moves crosshair / values; no permanent UI corruption.

## Persistence

- [ ] Change BPM, key, one seq step → reboot → values restored.
- [ ] Optional: SD backup / restore round-trip (if testing SD).

## MIDI (when implemented)

- [ ] USB or BLE MIDI OUT sends notes/CC from play/seq/XY as designed.
- [ ] Second controller / clock follow (when implemented).

## Soak

- [ ] **30 minutes** continuous use: no lockup, no heap exhaustion symptoms.

## Bright / dim usability

- [ ] Bright room/daylight: key labels, bezel labels, and hold-progress bar remain legible.
- [ ] Dim room/night: no excessive glare; selected/highlight states remain distinguishable.
- [ ] Brightness sweep (10%, 50%, 100%): verify readability and touch confidence at each level.
- [ ] Accent/theme sweep: switch themes and confirm selected states remain obvious in both bright and dim conditions.

## Sign-off

Tester: _______________  Result: PASS / FAIL  Notes: _______________
