# Development Roadmap

> **Status:** Historical milestone log. For the current backlog use [`TODO_IMPLEMENTATION.md`](TODO_IMPLEMENTATION.md). For doc routing see [`DOCS_INDEX.md`](DOCS_INDEX.md).

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

- [x] **Transport screen**: project **BPM** (NVS), large readout, tap → numeric edit, long-press → tap tempo; **step** 1–16 + beat grouping; Play/Stop/Rec row; Metro/CntIn under Play; **MIDI out / in / clock** quick **dropdowns** (scrollable, finger-sized rows)
- [x] Add transport/settings **remainder**: default strum mode, swing/humanize (global) — default strum quick dropdown shipped on Transport; global swing + humanize shipped (persisted, applied in transport timing)
- [~] Add MIDI settings per `docs/MIDI_INPUT_SPEC.md`: **independent receive channels** for USB, Bluetooth, and DIN; MIDI clock follow toggle; corner BPM flash (spec) — independent USB/BLE/DIN IN-channel rows shipped (Settings + migration), USB ingress + clock hooks landed, BLE stack + provider hook (`m5ChordBleMidiRead`) wired, DIN UART/provider hook (`m5ChordDinMidiRead`) wired, `clkFollow` + `clkFlashEdge` toggles shipped; Play IN monitor controls (on/off, clock-hold, compact/detailed) and BLE peak-decay setting (off/5s/10s/30s) shipped; remaining work is hardware UX and DIN pin validation
- [x] Add MIDI settings: **output channel**, **transpose** (global), and **MIDI thru source mask** (USB/BLE/DIN combinations) — in Settings + **Transport dropdowns**
- [x] Add UI settings: **brightness** (row + `applyBrightness()`), **theme** / accent (`g_uiTheme`, `UiTheme` palette)
- [x] Add UI settings **remainder**: configurable **touch hold** thresholds (settings entry, sequencer long-press, etc.) — shipped presets for settings-entry hold, seq clear hold, and transport BPM hold (Shift-specific timing remains Phase 7 scope)
- [x] Add audio settings: default velocity and dynamics response curve — default velocity + output curve (Linear/Soft/Hard) shipped for v1 scope
- [x] Add factory reset option in settings page
- [x] Add version/build info row in settings page
- [~] Validate NVS key schema and add migration strategy for new versions (seq blob `seq` 16 bytes added; settings namespace `m5chord`) — schema bumped to v2 for `inCh` -> `inUsb`/`inBle`/`inDin` migration

## Milestone 3 - MIDI Engine Integration

Goal: move from UI scaffold to full MIDI behavior.

- [~] Implement MIDI IN parser and active-note tracking (tagged by transport: USB / BLE / DIN) — USB parser + source-tagged dispatch + active-note tracking shipped (`MidiIngress`/`MidiInState`); BLE stack/provider hook (`m5ChordBleMidiRead`) and DIN UART/provider hook (`m5ChordDinMidiRead`) are wired through the same parser queue; initial panic-policy integration (all-notes-off on settings-entry/ring/stop paths) landed; hardware validation + broader policy table pending
- [ ] Implement chord detection pipeline from incoming notes
- [~] Implement suggestion generation model and ranking (first-pass ranked suggestions to current-key surround chords now shown on Play with host tests; tuning/advanced context still pending)
- [ ] Implement MIDI OUT playback with configurable channel/velocity
- [x] Implement **per-transport** channel filtering (see `docs/MIDI_INPUT_SPEC.md`) at dispatch/settings layer (USB/BLE/DIN filters)
- [~] Implement **MIDI clock** receive, BPM derivation (24 PPQN), and **discrete corner BPM** with tempo-synchronized flash — external Start/Stop/Continue/Clock/SPP hooks + rolling BPM landed; Play corner BPM + flash shipped with follow/edge settings; Play IN monitor age/color polish landed; final hardware validation pending
- [~] Add all-notes-off safety path on mode transitions/errors — expanded path wired (settings-entry gestures, ring page changes, transport stop/external stop, key-picker and project/restore transitions, factory reset, and SD restore/backup/list failure paths); required-vs-optional trigger matrix is documented in `MIDI_INPUT_SPEC.md`
- [~] Add debug overlay page for incoming/outgoing MIDI diagnostics — minimal `MIDI Debug` screen shipped (event ring, active-note totals, clock stats, clear action) plus BLE decode drops and BLE/DIN queue-drop + RX/rate/peak diagnostics; outgoing TX diagnostics now show bytes/rate/peak and NoteOn/NoteOff/CC counters. Suggestion tuning UX is now comparison-friendly (`SUG profile`/`flip` auto-lock, quick `SUG unlock now`, lock markers, and blocked-edit guidance); per-transport OUT split remains optional

## Milestone 4 - CoreS3 UX Completion

Goal: complete touch-first UX replacing encoder-era assumptions.

- [x] Finalize main page layout for CoreS3 aspect ratio and readability
- [x] Boot to **Play** surface directly (splash **not** required; optional splash may return as a setting later)
- [x] **Play** surface: key center + **8** surround chords (3×3 ring), key/mode picker, visual last-played feedback
- [x] **SELECT latched**: **transpose** ± on two surround pads; **voicing** V1–V4 on another pad + bottom **slider** panel (yellow seq-tool styling)
- [x] Implement heart / surprise chord mechanic after N plays
- [ ] Add explicit pages for history/diatonic/functional/related/chromatic
- [ ] Implement fast page cycling via BACK/FWD long-press
- [ ] Add confirmation prompts for destructive actions
- [ ] Add visual feedback for gesture states (hold progress indicator)
- [ ] Tune button hitboxes and debounce for reliable fingertip use
- [ ] Validate usability in bright and dim lighting

## Milestone 5 - Hardware E2E and Release

Goal: hardware-verified release candidate with repeatable test evidence.

- [x] Create hardware E2E checklist document (single source of truth) — see `docs/HARDWARE_E2E_CHECKLIST.md`
- [ ] Execute gesture tests on real CoreS3 (single touch + multi-touch)
- [ ] Execute persistence tests (save, reboot, verify values)
- [ ] Execute MIDI interoperability tests with at least 2 controllers
- [ ] Run 30-minute stability soak test (no leaks/crashes)
- [ ] Tag release (`v0.1.0`) and publish flashing instructions
- [ ] Attach known-issues + recovery steps to release notes

## Milestone 6 - Sequencer & Shift layer (planned)

Goal: **4×4** step sequencer page, **X–Y MIDI pad** page (assignable CCs), bezel paging across **Transport, Play, Sequencer, X–Y**), step editor (**chord / rest / tie**), **Shift** (long-press **SELECT**) for params including **per-step clock division**, **per-step arpeggiator** (patterns + divisions), and **Shift+BACK/FWD** tap/hold adjustment.

Full interaction and data model: **`docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`**.

- [x] Sequencer `Screen` + 16-step grid UI (pattern in NVS + optional SD backup; **no MIDI note output** from steps yet)
- [ ] Step tap → dropdown: current-key chords (`ChordModel`) + rest + tie (today: **cycle** placeholder)
- [x] Bezel BACK/FWD: **cyclic ring** — **Transport → Play → Sequencer → X–Y → …** (see `navigateMainRing` in `main.cpp`)
- [x] **X–Y** `Screen` + touch pad UI + CC mapping stored in NVS (**MIDI send** still stub — Milestone 3)
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
