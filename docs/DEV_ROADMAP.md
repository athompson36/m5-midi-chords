# Development Roadmap

This roadmap is organized into milestones with concrete TODO checklists.

## Milestone 1 - Foundation Stabilization

Goal: lock in project structure, settings flow, and testability baseline.

- [x] Install GCC toolchain on dev machine and pass `platformio test -e native`
- [x] Add tests for settings row navigation and wrap behavior
- [x] Add tests for dual-corner settings-entry gesture timing logic
- [x] Refactor touch handling into smaller functions (`main`, `settings`, `gesture`)
- [x] Add a `make`/script helper for `build`, `test`, and `upload`
- [x] Add CI build job (at minimum compile CoreS3 firmware on push)

## Milestone 2 - Settings System Expansion

Goal: support all app-adjustable values from one consistent settings UX.

- [ ] Add transport settings: tempo/BPM, default strum mode, swing/humanize
- [ ] Add MIDI settings per `docs/MIDI_INPUT_SPEC.md`: **independent receive channels** for USB, Bluetooth, and (future) DIN; MIDI clock follow toggle; BPM corner display preferences
- [ ] Add MIDI settings: output channel, thru mode, transpose (global or per-destination TBD)
- [ ] Add UI settings: brightness, theme/accent, touch hold thresholds
- [ ] Add audio settings: default velocity and dynamics response curve
- [x] Add factory reset option in settings page
- [x] Add version/build info row in settings page
- [ ] Validate NVS key schema and add migration strategy for new versions (seq blob `seq` 16 bytes added; settings namespace `m5chord`)

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

## Milestone 6 - Sequencer & Shift layer (planned)

Goal: **4×4** step sequencer page, **X–Y MIDI pad** page (assignable CCs), bezel paging (**Play ↔ Sequencer ↔ X–Y** when implemented), step editor (**chord / rest / tie**), **Shift** (long-press **SELECT**) for params including **per-step clock division**, **per-step arpeggiator** (patterns + divisions), and **Shift+BACK/FWD** tap/hold adjustment.

Full interaction and data model: **`docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`**.

- [x] Sequencer `Screen` + 16-step grid UI (no MIDI; pattern in NVS + optional SD backup file)
- [ ] Step tap → dropdown: current-key chords (`ChordModel`) + rest + tie (today: **cycle** placeholder)
- [x] Bezel BACK/FWD: page between Play and Sequencer (**future:** add **X–Y** in ring; see spec)
- [ ] Internal transport + pattern MIDI OUT (after Milestone 3 basics)
- [ ] Shift: long-press SELECT; alternate labels on pads + steps; **include clock division** as a sequencer Shift function
- [ ] Per-step **arpeggiator**: enable + pattern (up/down/…) + **arp clock division**; any step independently; chord steps only
- [ ] Shift + BACK/FWD: incremental parameter edit; hold for accelerated repeat (focus: global vs last step — see spec)
- [ ] NVS/SD **schema migration** for per-step arp + division fields
- [ ] **X–Y MIDI page**: dual assignable axes (CC/channel/curve), touch-to-MIDI, optional 14-bit; assignment editor + NVS + backup
- [ ] Document resolved open questions (tie wrap, surprise chords in seq, focus model, arp spill policy, X–Y release policy)

## Suggested execution order

1. Milestone 1
2. Milestone 2
3. Milestone 3 (prerequisite for audible sequencer)
4. Milestone 4
5. Milestone 6 (sequencer / Shift — UI shell can start once navigation and gesture rules are settled; overlaps M3/M4)
6. Milestone 5 (hardware E2E and tagged release after the feature set you intend to ship is stable)
