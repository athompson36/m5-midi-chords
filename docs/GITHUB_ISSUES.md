# GitHub issues (one per TODO checkbox cluster)

Generated from `docs/TODO_IMPLEMENTATION.md` via:

```bash
python3 scripts/gen_github_issues.py
```

Create on GitHub:

1. **CLI (recommended):** ensure [`gh`](https://cli.github.com/) is installed and run:

   ```bash
   python3 scripts/create_github_issues.py
   ```

   Dry run: `python3 scripts/create_github_issues.py --dry-run`

   Other repo: `GITHUB_ISSUES_REPO=owner/name python3 scripts/create_github_issues.py`

2. **Manual:** use `scripts/github-issues.json` (title + body per entry) or the numbered list below.

**Total issues:** 50

---

## 1. [P1] Optional: **MIDI thru** (merge incoming to out) — only after Phase 2 ingress exist…

```
## Phase 1 — MIDI OUT foundation (prerequisite for XY, play, sequencer audio)

From `docs/TODO_IMPLEMENTATION.md`:

Optional: **MIDI thru** (merge incoming to out) — only after Phase 2 ingress exists; gate behind setting.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 1)
- `docs/DEV_ROADMAP.md`
```

## 2. [P2] Ingress layer: parse MIDI bytes from **USB** and **BLE** (and stub **DIN** for fut…

```
## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

From `docs/TODO_IMPLEMENTATION.md`:

Ingress layer: parse MIDI bytes from **USB** and **BLE** (and stub **DIN** for future UART).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 2)
- `docs/DEV_ROADMAP.md`
```

## 3. [P2] Tag each event with **transport** (Usb / Ble / Din).

```
## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

From `docs/TODO_IMPLEMENTATION.md`:

Tag each event with **transport** (`Usb` / `Ble` / `Din`).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 2)
- `docs/DEV_ROADMAP.md`
```

## 4. [P2] Replace single midiInChannel with **inChUsb**, **inChBle**, **inChDin** (0–16 OMNI…

```
## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

From `docs/TODO_IMPLEMENTATION.md`:

Replace single `midiInChannel` with **`inChUsb`**, **`inChBle`**, **`inChDin`** (0–16 OMNI/CH) + Settings UI rows + NVS + SD backup + migration from legacy `inCh`.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 2)
- `docs/DEV_ROADMAP.md`
```

## 5. [P2] **Active note tracking** per channel (and optionally per transport) for chord dete…

```
## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

From `docs/TODO_IMPLEMENTATION.md`:

**Active note tracking** per channel (and optionally per transport) for chord detection and panic.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 2)
- `docs/DEV_ROADMAP.md`
```

## 6. [P2] **MIDI clock follow**: handle Start / Stop / Continue / Timing Clock / SPP; derive…

```
## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

From `docs/TODO_IMPLEMENTATION.md`:

**MIDI clock follow**: handle Start / Stop / Continue / Timing Clock / SPP; derive BPM from **24 PPQN** rolling average.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 2)
- `docs/DEV_ROADMAP.md`
```

## 7. [P2] Connect AppSettings.midiClockSource / midiTransportReceive to actual source select…

```
## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

From `docs/TODO_IMPLEMENTATION.md`:

Connect `AppSettings.midiClockSource` / `midiTransportReceive` to actual source selection and clock extraction.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 2)
- `docs/DEV_ROADMAP.md`
```

## 8. [P2] **Play surface UX**: small **corner BPM** readout (spec: top-right, low-contrast);…

```
## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

From `docs/TODO_IMPLEMENTATION.md`:

**Play surface UX**: small **corner BPM** readout (spec: top-right, low-contrast); **flash** on quarter-note (or setting); show `—` when no clock.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 2)
- `docs/DEV_ROADMAP.md`
```

## 9. [P2] Settings row: **Follow MIDI clock** toggle (clkFollow) + optional clkFlashEdge + NVS.

```
## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

From `docs/TODO_IMPLEMENTATION.md`:

Settings row: **Follow MIDI clock** toggle (`clkFollow`) + optional `clkFlashEdge` + NVS.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 2)
- `docs/DEV_ROADMAP.md`
```

## 10. [P2] **Chord detection pipeline**: aggregate simultaneous notes → chord class → map to …

```
## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

From `docs/TODO_IMPLEMENTATION.md`:

**Chord detection pipeline**: aggregate simultaneous notes → chord class → map to suggestion / highlight (tie into `ChordModel` or new module).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 2)
- `docs/DEV_ROADMAP.md`
```

## 11. [P2] **Suggestion generation & ranking** (if distinct from simple match): define algori…

```
## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

From `docs/TODO_IMPLEMENTATION.md`:

**Suggestion generation & ranking** (if distinct from simple match): define algorithm, tests on host where possible.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 2)
- `docs/DEV_ROADMAP.md`
```

## 12. [P2] **Debug overlay** page or screen: last N IN events, active notes, BPM, transport —…

```
## Phase 2 — MIDI IN, transports, and clock (per `MIDI_INPUT_SPEC.md`)

From `docs/TODO_IMPLEMENTATION.md`:

**Debug overlay** page or screen: last N IN events, active notes, BPM, transport — toggle from settings or build flag.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 2)
- `docs/DEV_ROADMAP.md`
```

## 13. [P3] **All-notes-off** / CC panic on: screen change, error, transport stop, settings en…

```
## Phase 3 — Safety, thru, panic

From `docs/TODO_IMPLEMENTATION.md`:

**All-notes-off** / CC panic on: screen change, error, transport stop, settings entry (policy table).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 3)
- `docs/DEV_ROADMAP.md`
```

## 14. [P3] **MIDI thru** behavior: per setting, which transports merge to OUT (after Phase 2).

```
## Phase 3 — Safety, thru, panic

From `docs/TODO_IMPLEMENTATION.md`:

**MIDI thru** behavior: per setting, which transports merge to OUT (after Phase 2).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 3)
- `docs/DEV_ROADMAP.md`
```

## 15. [P4] **Transport page / rows**: tie UI to real **project BPM** (already in NVS/project)…

```
## Phase 4 — Settings expansion (Milestone 2 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**Transport page / rows**: tie UI to real **project BPM** (already in NVS/project); add **default strum** mode if still product goal.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 4)
- `docs/DEV_ROADMAP.md`
```

## 16. [P4] **Swing / humanize** for outgoing MIDI or internal clock (align with existing SeqE…

```
## Phase 4 — Settings expansion (Milestone 2 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**Swing / humanize** for outgoing MIDI or internal clock (align with existing `SeqExtras.swingPct` vs global transport).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 4)
- `docs/DEV_ROADMAP.md`
```

## 17. [P4] **UI**: theme / accent selection (persist; apply UiTheme or expand palette).

```
## Phase 4 — Settings expansion (Milestone 2 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**UI**: theme / accent selection (persist; apply `UiTheme` or expand palette).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 4)
- `docs/DEV_ROADMAP.md`
```

## 18. [P4] **Touch**: configurable long-press thresholds (settings entry, Shift, seq long-pre…

```
## Phase 4 — Settings expansion (Milestone 2 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**Touch**: configurable long-press thresholds (settings entry, Shift, seq long-press) with sane bounds.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 4)
- `docs/DEV_ROADMAP.md`
- `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`
```

## 19. [P4] **Velocity curve**: input curve for touch or output curve for MIDI (document vs si…

```
## Phase 4 — Settings expansion (Milestone 2 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**Velocity curve**: input curve for touch or output curve for MIDI (document vs single velocity).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 4)
- `docs/DEV_ROADMAP.md`
```

## 20. [P4] **Brightness** — verify already wired; add preview if missing.

```
## Phase 4 — Settings expansion (Milestone 2 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**Brightness** — verify already wired; add preview if missing.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 4)
- `docs/DEV_ROADMAP.md`
```

## 21. [P5] **Chord category pages**: history, diatonic, functional, related, chromatic — navi…

```
## Phase 5 — Play surface UX (Milestone 4 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**Chord category pages**: history, diatonic, functional, related, chromatic — navigation model + `Screen` or modal stack.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 5)
- `docs/DEV_ROADMAP.md`
```

## 22. [P5] **BACK/FWD long-press** → fast page cycle (vs short-tap ring); de-conflict with se…

```
## Phase 5 — Play surface UX (Milestone 4 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**BACK/FWD long-press** → fast page cycle (vs short-tap ring); de-conflict with settings 800 ms hold.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 5)
- `docs/DEV_ROADMAP.md`
```

## 23. [P5] **Destructive actions**: confirm dialogs (factory reset already partial — extend p…

```
## Phase 5 — Play surface UX (Milestone 4 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**Destructive actions**: confirm dialogs (factory reset already partial — extend pattern).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 5)
- `docs/DEV_ROADMAP.md`
```

## 24. [P5] **Gesture feedback**: linear hold progress (settings entry, Shift arm, etc.).

```
## Phase 5 — Play surface UX (Milestone 4 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**Gesture feedback**: linear hold progress (settings entry, Shift arm, etc.).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 5)
- `docs/DEV_ROADMAP.md`
- `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`
```

## 25. [P5] **Hit-testing**: measure and tune debounce / fat-finger padding; document test pro…

```
## Phase 5 — Play surface UX (Milestone 4 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**Hit-testing**: measure and tune debounce / fat-finger padding; document test procedure.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 5)
- `docs/DEV_ROADMAP.md`
```

## 26. [P5] **Usability**: bright / dim environment checklist (manual QA).

```
## Phase 5 — Play surface UX (Milestone 4 remainder)

From `docs/TODO_IMPLEMENTATION.md`:

**Usability**: bright / dim environment checklist (manual QA).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 5)
- `docs/DEV_ROADMAP.md`
```

## 27. [P6] **Step tap → dropdown**: list ChordModel surround + rest + tie + (optional) surpri…

```
## Phase 6 — Sequencer core (playback + editor)

From `docs/TODO_IMPLEMENTATION.md`:

**Step tap → dropdown**: list `ChordModel` surround + rest + tie + (optional) surprise pool — replace pure cycle.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 6)
- `docs/DEV_ROADMAP.md`
```

## 28. [P6] **Internal transport** → **MIDI OUT** from pattern: for each step, resolve chord/r…

```
## Phase 6 — Sequencer core (playback + editor)

From `docs/TODO_IMPLEMENTATION.md`:

**Internal transport** → **MIDI OUT** from pattern: for each step, resolve chord/rest/tie, respect lane + swing/quantize from `SeqExtras` as product intends.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 6)
- `docs/DEV_ROADMAP.md`
```

## 29. [P6] **Live playhead** highlight on grid during playback (may exist partially — verify …

```
## Phase 6 — Sequencer core (playback + editor)

From `docs/TODO_IMPLEMENTATION.md`:

**Live playhead** highlight on grid during playback (may exist partially — verify vs spec).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 6)
- `docs/DEV_ROADMAP.md`
```

## 30. [P6] **Surprise / heart chords in seq**: in or out of dropdown (resolve open question i…

```
## Phase 6 — Sequencer core (playback + editor)

From `docs/TODO_IMPLEMENTATION.md`:

**Surprise / heart chords in seq**: in or out of dropdown (resolve open question in spec).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 6)
- `docs/DEV_ROADMAP.md`
```

## 31. [P6] **Tie semantics** across loop boundary (spec open question — pick one, implement, …

```
## Phase 6 — Sequencer core (playback + editor)

From `docs/TODO_IMPLEMENTATION.md`:

**Tie semantics** across loop boundary (spec open question — pick one, implement, document).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 6)
- `docs/DEV_ROADMAP.md`
```

## 32. [P7] **Shift model**: long-press **SELECT** (400–800 ms) = modifier while held **or** t…

```
## Phase 7 — Shift layer (Play + Sequencer + XY)

From `docs/TODO_IMPLEMENTATION.md`:

**Shift model**: long-press **SELECT** (400–800 ms) = modifier while held **or** toggle — pick v1 per `SEQUENCER_AND_SHIFT_UX_SPEC.md`.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 7)
- `docs/DEV_ROADMAP.md`
- `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`
```

## 33. [P7] De-conflict with **Play** SELECT = key-edit latch: use timing, double-tap, or sepa…

```
## Phase 7 — Shift layer (Play + Sequencer + XY)

From `docs/TODO_IMPLEMENTATION.md`:

De-conflict with **Play** SELECT = key-edit latch: use timing, double-tap, or separate chord (document UX).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 7)
- `docs/DEV_ROADMAP.md`
```

## 34. [P7] **Shift + chord pads**: alternate labels; send **CC / program change** map (define…

```
## Phase 7 — Shift layer (Play + Sequencer + XY)

From `docs/TODO_IMPLEMENTATION.md`:

**Shift + chord pads**: alternate labels; send **CC / program change** map (define default map + settings storage).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 7)
- `docs/DEV_ROADMAP.md`
- `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`
```

## 35. [P7] **Shift + sequencer steps**: expose **clock division**, **arp on/off**, **arp patt…

```
## Phase 7 — Shift layer (Play + Sequencer + XY)

From `docs/TODO_IMPLEMENTATION.md`:

**Shift + sequencer steps**: expose **clock division**, **arp on/off**, **arp pattern**, **arp clock div** (matrix or overlay).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 7)
- `docs/DEV_ROADMAP.md`
- `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`
```

## 36. [P7] **Shift + BACK/FWD**: increment/decrement focused parameter; **hold** = accelerate.

```
## Phase 7 — Shift layer (Play + Sequencer + XY)

From `docs/TODO_IMPLEMENTATION.md`:

**Shift + BACK/FWD**: increment/decrement focused parameter; **hold** = accelerate.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 7)
- `docs/DEV_ROADMAP.md`
- `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`
```

## 37. [P7] **Focus model**: last touched step/pad vs global (resolve spec open question).

```
## Phase 7 — Shift layer (Play + Sequencer + XY)

From `docs/TODO_IMPLEMENTATION.md`:

**Focus model**: last touched step/pad vs global (resolve spec open question).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 7)
- `docs/DEV_ROADMAP.md`
```

## 38. [P8] Data model: per-step stepClockDiv, arpEnabled, arpPattern, arpClockDiv (+ migratio…

```
## Phase 8 — Per-step clock division & arpeggiator

From `docs/TODO_IMPLEMENTATION.md`:

Data model: per-step `stepClockDiv`, `arpEnabled`, `arpPattern`, `arpClockDiv` (+ migration from 16-byte pattern blob).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 8)
- `docs/DEV_ROADMAP.md`
```

## 39. [P8] **Transport math**: step window length from global default + per-step override; in…

```
## Phase 8 — Per-step clock division & arpeggiator

From `docs/TODO_IMPLEMENTATION.md`:

**Transport math**: step window length from global default + per-step override; interaction with BPM.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 8)
- `docs/DEV_ROADMAP.md`
```

## 40. [P8] **Arp engine**: schedule note-ons within step window; **truncate vs spill** policy…

```
## Phase 8 — Per-step clock division & arpeggiator

From `docs/TODO_IMPLEMENTATION.md`:

**Arp engine**: schedule note-ons within step window; **truncate vs spill** policy (pick one from spec open questions).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 8)
- `docs/DEV_ROADMAP.md`
```

## 41. [P8] UI: all Shift paths wired; visual indicators on steps with arp/div overrides.

```
## Phase 8 — Per-step clock division & arpeggiator

From `docs/TODO_IMPLEMENTATION.md`:

UI: all Shift paths wired; visual indicators on steps with arp/div overrides.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 8)
- `docs/DEV_ROADMAP.md`
- `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`
```

## 42. [P8] Persist expanded blob to NVS + **SD backup** format bump + restore.

```
## Phase 8 — Per-step clock division & arpeggiator

From `docs/TODO_IMPLEMENTATION.md`:

Persist expanded blob to NVS + **SD backup** format bump + restore.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 8)
- `docs/DEV_ROADMAP.md`
```

## 43. [P9] **Assignment editor**: channel, CC per axis, curve (linear/log/invert), optional l…

```
## Phase 9 — X–Y MIDI page (complete spec)

From `docs/TODO_IMPLEMENTATION.md`:

**Assignment editor**: channel, CC per axis, curve (linear/log/invert), optional labels (`Cutoff`, etc.).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 9)
- `docs/DEV_ROADMAP.md`
```

## 44. [P9] **14-bit** CC pairs or MSB/LSB where supported; fallback 7-bit.

```
## Phase 9 — X–Y MIDI page (complete spec)

From `docs/TODO_IMPLEMENTATION.md`:

**14-bit** CC pairs or MSB/LSB where supported; fallback 7-bit.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 9)
- `docs/DEV_ROADMAP.md`
```

## 45. [P9] **Release policy** per axis: hold last, spring to center, send zero — implement + …

```
## Phase 9 — X–Y MIDI page (complete spec)

From `docs/TODO_IMPLEMENTATION.md`:

**Release policy** per axis: hold last, spring to center, send zero — implement + persist `xyPadSpring`-style enum.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 9)
- `docs/DEV_ROADMAP.md`
```

## 46. [P9] Replace stub midiSendControlChange usage with real transport; throttle if needed t…

```
## Phase 9 — X–Y MIDI page (complete spec)

From `docs/TODO_IMPLEMENTATION.md`:

Replace stub `midiSendControlChange` usage with real transport; throttle if needed to avoid MIDI flood.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 9)
- `docs/DEV_ROADMAP.md`
```

## 47. [P9] NVS + SD backup for XY mapping (align with SdBackup / project files).

```
## Phase 9 — X–Y MIDI page (complete spec)

From `docs/TODO_IMPLEMENTATION.md`:

NVS + SD backup for XY mapping (align with `SdBackup` / project files).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 9)
- `docs/DEV_ROADMAP.md`
```

## 48. [P10] Run checklist on CoreS3; record dates/results in E2E_STATUS.md or checklist appendix.

```
## Phase 10 — Hardware E2E & release (Milestone 5)

From `docs/TODO_IMPLEMENTATION.md`:

Run checklist on CoreS3; record dates/results in `E2E_STATUS.md` or checklist appendix.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 10)
- `docs/DEV_ROADMAP.md`
```

## 49. [P10] **Tag** v0.1.0 (or agreed version); **release notes** with known issues + recovery…

```
## Phase 10 — Hardware E2E & release (Milestone 5)

From `docs/TODO_IMPLEMENTATION.md`:

**Tag** `v0.1.0` (or agreed version); **release notes** with known issues + recovery (flash, NVS clear, SD).

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 10)
- `docs/DEV_ROADMAP.md`
```

## 50. [P10] Verify FLASH_INSTRUCTIONS.md / CI match release artifact.

```
## Phase 10 — Hardware E2E & release (Milestone 5)

From `docs/TODO_IMPLEMENTATION.md`:

Verify `FLASH_INSTRUCTIONS.md` / CI match release artifact.

## References
- `docs/TODO_IMPLEMENTATION.md` (Phase 10)
- `docs/DEV_ROADMAP.md`
```
