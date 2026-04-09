# Full implementation checklist (remaining features)

This is the working backlog to reach parity with `DEV_ROADMAP.md`, `MIDI_INPUT_SPEC.md`, `SEQUENCER_AND_SHIFT_UX_SPEC.md`, and related UX docs. Order respects prerequisites (e.g. MIDI OUT before audible sequencer).

**Conventions:** `[ ]` not started · In progress / blocked can be noted in PRs · Link issues to sections here when useful.

---

## Phase 0 — Documentation & schema groundwork

- [x] Refresh `DEV_ROADMAP.md` checkboxes against current firmware (boot flow, bezel ring pages, XY shell).
- [x] Update `SEQUENCER_AND_SHIFT_UX_SPEC.md` “Done / Not done” and BACK/FWD ring to match `main.cpp`.
- [x] Update `README.md` boot/splash and sequencer bullets if still stale.
- [x] Define **NVS schema version** byte and **migration** pattern — `schema` key + `prefsMigrateOnBoot()` (`SettingsStore.cpp`); extend with versioned branches when blobs change.
- [x] List all persisted keys (`SettingsStore`, `Transport` prefs, seq blob, XY, project name) in one table with version introduced (`docs/PERSISTENCE_KEYS.md`).

---

## Phase 1 — MIDI OUT foundation (prerequisite for XY, play, sequencer audio)

- [x] Choose stack: USB MIDI **device** vs **host** for CoreS3; document decision in `docs/` or README — see **`docs/MIDI_STACK.md`** (CDC `Serial` now; class MIDI device TBD).
- [x] Implement real `midiSendControlChange` in `MidiOut.cpp` (USB and/or UART bridge as applicable).
- [x] Add `midiSendNoteOn` / `midiSendNoteOff` / `midiSendAllNotesOff` (channel 0–15) with sensible defaults.
- [x] Wire **play surface** chord + tonic taps to note output using `ChordModel` voicing + `g_chordVoicing` + `g_settings.outputVelocity`.
- [x] Wire **global** `arpeggiatorMode` (from `AppSettings`) into play output (`arpeggiatorOrderedIntervals` in `MidiOut.cpp`).
- [x] Settings: **transpose** (global semitone(s), −24…+24) + NVS `xpose` + SD global + MIDI apply on play.
- [x] Optional: **MIDI thru** (merge incoming to out) — shipped as a per-transport source mask setting (`MIDI thru src`: Off/USB/BLE/DIN/combinations/All) that forwards incoming channel voice events (NoteOn/NoteOff/CC) after source/channel filtering.

---

## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

- [~] Ingress layer: parse MIDI bytes from **USB**, **BLE**, and **DIN**. (USB parser shipped; BLE stack + provider hook `m5ChordBleMidiRead` wired; DIN UART reader + provider hook `m5ChordDinMidiRead` now wired into `midiIngressPollDin`; hardware validation/pinout confirmation pending)
- [x] Tag each event with **transport** (`Usb` / `Ble` / `Din`) at parser/dispatch level.
- [x] Replace single `midiInChannel` with **`inChUsb`**, **`inChBle`**, **`inChDin`** (0–16 OMNI/CH) + Settings UI rows + NVS + SD backup + migration from legacy `inCh`.
- [~] **Active note tracking** per channel (and per transport source) for chord detection and panic. (core tracking shipped via `MidiInState`; initial panic wiring landed for ring page changes, settings-entry gestures, and transport stop/external stop via all-notes-off; remaining policy table/edge-cases pending)
- [~] **MIDI clock follow**: handle Start / Stop / Continue / Timing Clock / SPP; derive BPM from **24 PPQN** rolling average. (foundation hooks + rolling BPM shipped; UX/polish still pending)
- [x] Connect `AppSettings.midiClockSource` / `midiTransportReceive` to actual source selection and clock extraction.
- [~] **Play surface UX**: small **corner BPM** readout (spec: top-right, low-contrast); **flash** on quarter-note (or setting); show `—` when no clock. (corner BPM + flash + `clkFlashEdge` shipped; added optional Play IN monitor with compact/detailed modes, clock-hold filter, adaptive age, and settings legend/swatch; final hardware UX validation pending)
- [~] Settings row: **Follow MIDI clock** toggle (`clkFollow`) + optional `clkFlashEdge` + NVS. (`clkFollow` + `clkFlashEdge` shipped with NVS/SD persistence; further UX tuning optional)
- [~] **Chord detection pipeline**: aggregate simultaneous notes → chord class → map to suggestion / highlight (tie into `ChordModel` or new module). (basic triad detection + Play hint shipped; basic ranked suggestion mapping to current-key surround chords now shown on Play; richer context-aware ranking remains optional)
- [~] **Suggestion generation & ranking** (if distinct from simple match): define algorithm, tests on host where possible. (first-pass scoring/ranking module shipped with host tests; `MIDI Debug` now shows top-3 suggestions + scores for on-device tuning plus `S` marker when stability bias held prior pick; stability window/score-gap are configurable in MIDI settings + persisted and shown live on `MIDI Debug`; added `SUG defaults`, `SUG profile` A/B quick-apply (now auto-enables lock), one-tap `SUG flip A/B` (auto-enables lock), `SUG profile lock`, and one-tap `SUG unlock now` override to prevent accidental window/gap edits during A/B comparisons while preserving fast manual tuning (including `already unlocked` feedback when appropriate), with explicit unlock guidance when edits are blocked, a `[L]` lock marker on window/gap rows while locked, and grouped row ordering for quicker tuning flow; further weighting/tuning optional)
- [~] **Debug overlay** page or screen: last N IN events, active notes, BPM, transport — toggle from settings or build flag. (minimal `MIDI Debug` screen shipped with build flag + Settings gate + clear action; BLE decode drops + BLE/DIN queue-drop counters + per-transport RX/rate/peak + panic trigger counters now shown; outgoing TX diagnostics now include bytes/rate/peak + NoteOn/NoteOff/CC counters; any further per-transport OUT split remains optional)

---

## Phase 3 — Safety, thru, panic

- [~] **All-notes-off** / CC panic on: screen change, error, transport stop, settings entry (policy table). (wiring covers settings-entry gestures, ring page changes, transport stop/external stop, key-picker transitions, project-name/restore transitions, factory reset, save-exit, and SD error/failure feedback paths; policy table is documented in `MIDI_INPUT_SPEC.md`; remaining: optional/future trigger review as UX evolves)
- [x] **MIDI thru** behavior: per setting, which transports merge to OUT (after Phase 2). (implemented as `midiThruMask` with USB/BLE/DIN bitmask UI + NVS/SD persistence; legacy bool migration supported)

---

## Phase 4 — Settings expansion (Milestone 2 remainder)

- [x] **Transport screen** (`main.cpp`): **project BPM** from NVS — large readout, **tap** → numeric editor (40–300), **long-press** → tap-tempo sheet; **step** 1–16 with beat/subdivision; Play/Stop/Rec row; Metro/CntIn under Play; **MIDI out / in / clock** as full-width **dropdowns** (large rows, BACK/FWD + drag scroll, tap to select; `transportDropOpen` / `transportDropDismiss`).
- [x] **Transport page / rows (remainder)**: **default strum** mode if still product goal; any extra rows not covered above. (default strum quick dropdown shipped on Transport via `arpeggiatorMode`; no additional transport rows required for current v1 scope)
- [x] **Swing / humanize** for outgoing MIDI or internal clock (align with existing `SeqExtras.swingPct` vs global transport). (global transport `Global swing` + `Global humanize` settings shipped as persisted 0–100% controls and applied to internal step interval timing; swing combines with per-lane `SeqExtras.swingPct`, humanize adds bounded random micro-jitter)
- [x] **UI**: theme / accent selection (persist; apply `UiTheme` or expand palette). (`g_uiTheme` persisted and applied across built-in palettes with accent colors per theme)
- [x] **Touch**: configurable long-press thresholds (settings entry, Shift, seq long-press) with sane bounds. (persisted presets shipped for settings-entry hold, sequencer clear hold, and transport BPM tap-hold; Shift-specific threshold is handled with Shift-layer implementation in Phase 7)
- [x] **Velocity curve**: input curve for touch or output curve for MIDI (document vs single velocity). (global output velocity curve shipped: Linear/Soft/Hard, persisted and applied to local Play chord output; touch-input curve is optional future refinement)
- [x] **Brightness** — wired: Settings display row cycles 10–100%, **`applyBrightness()`** on pick and boot (`main.cpp` / `settingsApplyDropdownPick`); no separate preview screen (optional polish).

---

## Phase 5 — Play surface UX (Milestone 4 remainder)

- [x] **SELECT latched** play tools: **transpose** ±1 semitone (surround pads, −24…+24, `settingsSave`); **chord voicing** V1–V4 on a surround pad + **bottom slider** strip (1–4 voices; seq-lane tab color); two-finger voicing gesture disabled while SELECT latched; voicing panel + navigate-away dismiss (`s_playVoicingPanelOpen`).
- [x] **Chord category pages**: history, diatonic, functional, related, chromatic — navigation model + `Screen` or modal stack. (added `PlayCategory` screen with category paging on BACK/FWD, SELECT to return to Play, tap-to-play category chords, and Play key-center hold to enter categories)
- [x] **BACK/FWD long-press** → fast page cycle (vs short-tap ring); de-conflict with settings 800 ms hold. (Play + PlayCategory now reserve BACK/FWD hold for fast repeat cycling; single-finger FWD hold-to-settings is disabled on those screens, while short taps still use ring/category navigation)
- [x] **Destructive actions**: confirm dialogs (factory reset already partial — extend pattern). (added Settings confirm overlay for `Restore SD` and `Clear MIDI debug` with explicit cancel/confirm flow on bezel actions; factory reset keeps explicit two-option confirmation)
- [x] **Gesture feedback**: linear hold progress (settings entry, Shift arm, etc.). (added linear bezel hold-progress strip for Settings entry holds: single-finger FWD hold and two-finger BACK+FWD hold; also added BPM hold-progress strip before tap-tempo branch; Shift-arm-specific progress remains tied to Phase 7 Shift implementation)
- [x] **Hit-testing**: measure and tune debounce / fat-finger padding; document test procedure. (added Play/Seq/PlayCategory hit padding and light tap debounce to reduce accidental brushes; added explicit hit-padding/debounce manual checks in `docs/HARDWARE_E2E_CHECKLIST.md`)
- [x] **Usability**: bright / dim environment checklist (manual QA). (added dedicated bright/dim readability checklist section with brightness/theme sweeps in `docs/HARDWARE_E2E_CHECKLIST.md`)

---

## Phase 6 — Sequencer core (playback + editor)

- [x] **Step tap → dropdown**: list `ChordModel` surround + rest + tie + (optional) surprise pool — replace pure cycle. (sequencer step tap now opens a picker with surround chords, tie, rest, and a surprise token `S?`; this replaces the old tap-cycle editing path)
- [x] **Internal transport** → **MIDI OUT** from pattern: for each step, resolve chord/rest/tie, respect lane + swing/quantize from `SeqExtras` as product intends. (transport playhead now emits MIDI per sequencer step across all 3 lanes with per-lane MIDI channels, chord/rest/tie/surprise handling, velocity curve, voicing cap, transpose, global arpeggiator mode, and per-step probability gating; swing timing is applied in transport interval math; quantize remains scoped to future record-quantize behavior)
- [x] **Live playhead** highlight on grid during playback (may exist partially — verify vs spec). (sequencer grid highlights `transportAudibleStep()` while transport is playing)
- [x] **Surprise / heart chords in seq**: in or out of dropdown (resolve open question in spec). (resolved **in** for v1 via sequencer `S? surprise` dropdown token; heart mechanic stays on Play surface and is not a sequencer step type)
- [x] **Tie semantics** across loop boundary (spec open question — pick one, implement, document). (resolved policy: tie = no new attack; held notes continue, including across loop wrap from step 16 -> 1; tie after rest behaves as silence)

---

## Phase 7 — Shift layer (Play + Sequencer + XY)

- [x] **Shift model**: long-press **SELECT** (400–800 ms) = modifier while held **or** toggle — pick v1 per `SEQUENCER_AND_SHIFT_UX_SPEC.md`. (picked v1 = **hold-to-Shift** at ~550 ms; bezel label flips to `SHIFT` while active; sequencer tool row is shown only while Shift is held)
- [x] De-conflict with **Play** SELECT = key-edit latch: use timing, double-tap, or separate chord (document UX). (resolved by timing split: **tap SELECT** keeps Play key-edit latch, **hold SELECT** enters temporary Shift and suppresses tap-toggle on release)
- [~] **Shift + chord pads**: alternate labels; send **CC / program change** map (define default map + settings storage). (while Shift is held, Play pads now show alternate labels and send a default map: pads 0..3 -> CC20..23 (value 127), pads 4..7 -> Program Change 1..4 on current MIDI out channel; settings-backed user remap storage still pending)
- [~] **Shift + sequencer steps**: expose **clock division**, **arp on/off**, **arp pattern**, **arp clock div** (matrix or overlay). (while Shift is held in Sequencer, tap a step to focus it; top row exposes and cycles `Div`, `Arp`, `Pat`, and `ADiv` values for that focused step; persistence/engine wiring lands with Phase 8 data-model work)
- [x] **Shift + BACK/FWD**: increment/decrement focused parameter; **hold** = accelerate. (in Sequencer while Shift is held and a step is focused, BACK/FWD now nudges the focused Shift parameter; holding BACK/FWD auto-repeats with acceleration)
- [x] **Focus model**: last touched step/pad vs global (resolve spec open question). (resolved v1 focus policy: **last touched target in current context**; sequencer Shift step focus is tracked per lane and restored when switching lanes, while Play Shift pad actions are immediate and stateless)

---

## Phase 8 — Per-step clock division & arpeggiator

- [x] Data model: per-step `stepClockDiv`, `arpEnabled`, `arpPattern`, `arpClockDiv` (+ migration from 16-byte pattern blob).  
      Implemented in `SeqExtras` as persisted 3x16 arrays; legacy 16-byte `seq` blobs still load as lane0-only with lane1/2 rest fill.
- [x] **Transport math**: step window length from global default + per-step override; interaction with BPM.  
      Internal transport now computes per-step base interval from BPM and `stepClockDiv` (1x/2x/4x/8x), then applies swing/humanize on top.
- [x] **Arp engine**: schedule note-ons within step window; **truncate vs spill** policy (pick one from spec open questions).  
      Implemented per-lane step arp scheduling using `arpEnabled/arpPattern/arpClockDiv`; v1 policy is **truncate** (no note spill across step boundary).
- [x] UI: all Shift paths wired; visual indicators on steps with arp/div overrides.  
      Sequencer step cells now show compact override badges (`D2x/4x/8x`, `A...`) and Shift top-row editors reflect focused-step arp/div values.
- [x] Persist expanded blob to NVS + **SD backup** format bump + restore.  
      `seqEx` NVS blob now supports v1 (legacy) and v2 (expanded per-step fields); SD `project.txt` now round-trips `seqD/seqAe/seqAp/seqAd` with backward-compatible parsing.

---

## Phase 9 — X–Y MIDI page (complete spec)

- [x] **Assignment editor**: channel, CC per axis, curve (linear/log/invert), optional labels (`Cutoff`, etc.).  
      Added on-page XY config pills for CH / X-CC / Y-CC / X-curve / Y-curve (tap to cycle), with live send-path usage.
- [x] **14-bit** CC pairs or MSB/LSB where supported; fallback 7-bit.  
      XY send-path now auto-emits MSB/LSB pairs when CC is 0-31 (`cc` + `cc+32`), otherwise sends standard 7-bit CC.
- [x] **Release policy** per axis: hold last, spring to center, send zero — implement + persist `xyPadSpring`-style enum.  
      Implemented v1 global release policy: default hold-last, SELECT toggles return-to-center on release, persisted via NVS + SD global backup.
- [x] Replace stub `midiSendControlChange` usage with real transport; throttle if needed to avoid MIDI flood.  
      XY output now runs through queued send helpers with 10ms min interval per axis, plus loop-time flush and forced send on release-center events.
- [x] NVS + SD backup for XY mapping (align with `SdBackup` / project files).  
      XY mapping now persists channel + CCA/CCB + curveA/curveB in NVS and round-trips in SD project files (`xyCh`, `xyCCA`, `xyCCB`, `xyCvA`, `xyCvB`).

---

## Phase 10 — Hardware E2E & release (Milestone 5)

- [x] Author **`docs/HARDWARE_E2E_CHECKLIST.md`** (single source of truth): gestures, persistence, MIDI with 2+ devices, 30 min soak.
- [ ] Run checklist on CoreS3; record dates/results in `E2E_STATUS.md` or checklist appendix.
      2026-04-09: host verification updated in `docs/E2E_STATUS.md` (build/tests PASS); manual on-device checklist pass still pending sign-off.
- [x] **Tag** `v0.1.0` (or agreed version); **release notes** with known issues + recovery (flash, NVS clear, SD).  
      Tagged `v0.1.0` from commit `5f5109f`; release notes finalized in `docs/RELEASE_NOTES_v0.1.0.md`; GitHub release published.
- [x] Verify `FLASH_INSTRUCTIONS.md` / CI match release artifact.  
      Updated flashing/build docs to align with CI artifact path (`.pio/build/m5stack-cores3`) and current build metadata/checksums.

---

## Dependency sketch

```
Phase 0 (docs/schema)
    ↓
Phase 1 (MIDI OUT) ─────────────────────────────┐
    ↓                                            │
Phase 2 (MIDI IN + clock + suggest)             │
    ↓                                            │
Phase 3 (panic/thru)                             │
    ↓                                            │
Phase 4 (settings remainder)                     │
    ↓                                            │
Phase 5 (play UX pages/gestures)                 │
    ↓                                            │
Phase 6 (seq editor + seq MIDI) ◄────────────────┘
    ↓
Phase 7 (Shift)
    ↓
Phase 8 (per-step div + arp + migration)
    ↓
Phase 9 (XY complete)
    ↓
Phase 10 (E2E + release)
```

Parallel tracks after Phase 1: **Phase 5** can overlap **Phase 2–4** with care; **Phase 6** needs **Phase 1**; **Phase 8** needs **Phase 6–7** foundations.

---

## Quick reference — spec files

| Document | Scope |
|----------|--------|
| `docs/DEV_ROADMAP.md` | Milestones 1–6 |
| `docs/MIDI_INPUT_SPEC.md` | Transports, channels, clock, BPM UI |
| `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md` | Seq, Shift, XY, arp, open questions |
| `docs/ORIGINAL_UX_SPEC.md` | Baseline play/settings UX |

---

**GitHub issues:** one issue per checkbox cluster — run `python3 scripts/gen_github_issues.py` to refresh `scripts/github-issues.json` and `docs/GITHUB_ISSUES.md`, then `python3 scripts/create_github_issues.py` (requires [`gh`](https://cli.github.com/) auth).

*Last updated: USB MIDI class-device path enabled in `UsbMidiTransport` (TinyUSB MIDI interface), BLE TX framing tightened for multi-message interoperability, transport realtime send path wired (Clock/Start/Continue/Stop on selected route), panic now clears tracked notes on all channels, sequencer SELECT tool toggle latching fixed, and MIDI ingress/provider tests extended with USB provider-hook coverage; hardware interoperability verification remains ongoing in `docs/HARDWARE_E2E_CHECKLIST.md`.*
