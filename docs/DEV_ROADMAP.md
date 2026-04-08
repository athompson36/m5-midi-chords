# Development Roadmap

This roadmap is organized into milestones with concrete TODO checklists.

## Milestone 1 - Foundation Stabilization

Goal: lock in project structure, settings flow, and testability baseline.

- [x] Install GCC toolchain on dev machine and pass `platformio test -e native`
- [x] Add tests for settings row navigation and wrap behavior
- [ ] Add tests for dual-corner settings-entry gesture timing logic
- [x] Refactor touch handling into smaller functions (`main`, `settings`, `gesture`)
- [ ] Add a `make`/script helper for `build`, `test`, and `upload`
- [ ] Add CI build job (at minimum compile CoreS3 firmware on push)

## Milestone 2 - Settings System Expansion

Goal: support all app-adjustable values from one consistent settings UX.

- [ ] Add transport settings: tempo/BPM, default strum mode, swing/humanize
- [ ] Add MIDI settings per `docs/MIDI_INPUT_SPEC.md`: **independent receive channels** for USB, Bluetooth, and (future) DIN; MIDI clock follow toggle; BPM corner display preferences
- [ ] Add MIDI settings: output channel, thru mode, transpose (global or per-destination TBD)
- [ ] Add UI settings: brightness, theme/accent, touch hold thresholds
- [ ] Add audio settings: default velocity and dynamics response curve
- [ ] Add factory reset option in settings page
- [ ] Add version/build info row in settings page
- [ ] Validate NVS key schema and add migration strategy for new versions

## Milestone 3 - MIDI Engine Integration

Goal: move from UI scaffold to full MIDI behavior.

- [ ] Implement MIDI IN parser and active-note tracking (tagged by transport: USB / BLE / DIN)
- [ ] Implement chord detection pipeline from incoming notes
- [ ] Implement suggestion generation model and ranking
- [ ] Implement MIDI OUT playback with configurable channel/velocity
- [ ] Implement **per-transport** channel filtering (see `docs/MIDI_INPUT_SPEC.md`)
- [ ] Implement **MIDI clock** receive, BPM derivation (24 PPQN), and **discrete corner BPM** with tempo-synchronized flash
- [ ] Add all-notes-off safety path on mode transitions/errors
- [ ] Add debug overlay page for incoming/outgoing MIDI diagnostics

## Milestone 4 - CoreS3 UX Completion

Goal: complete touch-first UX replacing encoder-era assumptions.

- [x] Finalize main page layout for CoreS3 aspect ratio and readability
- [x] Implement boot splash ("Hi! Let's Play") and play surface with key selector + 6-chord grid
- [x] Implement heart / surprise chord mechanic after N plays
- [ ] Add explicit pages for history/diatonic/functional/related/chromatic
- [ ] Implement fast page cycling via BACK/FWD long-press
- [ ] Add confirmation prompts for destructive actions
- [ ] Add visual feedback for gesture states (hold progress indicator)
- [ ] Tune button hitboxes and debounce for reliable fingertip use
- [ ] Validate usability in bright and dim lighting

## Milestone 5 - Hardware E2E and Release

Goal: hardware-verified release candidate with repeatable test evidence.

- [ ] Create hardware E2E checklist document (single source of truth)
- [ ] Execute gesture tests on real CoreS3 (single touch + multi-touch)
- [ ] Execute persistence tests (save, reboot, verify values)
- [ ] Execute MIDI interoperability tests with at least 2 controllers
- [ ] Run 30-minute stability soak test (no leaks/crashes)
- [ ] Tag release (`v0.1.0`) and publish flashing instructions
- [ ] Attach known-issues + recovery steps to release notes

## Suggested execution order

1. Milestone 1
2. Milestone 2
3. Milestone 3
4. Milestone 4
5. Milestone 5
