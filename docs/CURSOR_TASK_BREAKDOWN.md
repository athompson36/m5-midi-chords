# Cursor Task Breakdown
## M5 MIDI Chords cleanup + UI upgrade

## How to use this file

Work in small, vertical slices.
Do not attempt the entire cleanup/refactor in one pass.

---

## Track 1 — Documentation cleanup

### Task 1.1
Update `README.md` so it matches actual current firmware behavior.

Include:
- product framing beyond “Chord Suggester”
- actual navigation model
- current screen set
- current MIDI capabilities at a high level

### Task 1.2
Audit `docs/DEV_ROADMAP.md` against current code and either:
- resync it
- or mark it as historical/superseded

### Task 1.3
Review `docs/E2E_STATUS.md` and clearly label historical findings that were later superseded.

### Task 1.4
Add a small `docs/DOCS_INDEX.md` listing:
- authoritative docs
- historical docs
- release/testing docs

---

## Track 2 — UI foundation extraction

### Task 2.1
Create `src/ui/UiTypes.h`
Move shared `Rect` and other neutral UI structs here.

### Task 2.2
Create `src/ui/UiLayout.h/.cpp`
Move shared layout math and spacing helpers here.

### Task 2.3
Create `src/ui/UiGloss.h/.cpp`
Implement shared component drawing helpers.

Start with:
- GlossButton
- StripShell
- DrawerSheet

### Task 2.4
Wrap the current button helper behind the new shared UI drawing layer if needed for safe migration.

---

## Track 3 — Drawer framework

### Task 3.1
Create `src/ui/UiDrawers.h/.cpp`

Responsibilities:
- top drawer open/close state
- one-open-at-a-time behavior
- drawer shell rendering
- shared gesture handling

### Task 3.2
Define per-page drawer sections:
- Play
- Sequencer
- X-Y

### Task 3.3
Keep bottom drawer transport behavior intact during this pass.

---

## Track 4 — X-Y drawer first

### Task 4.1
Move X-Y page draw/touch code into `screens/XyScreen.*`

### Task 4.2
Implement X-Y top drawer layout using shared components.

Suggested strips:
- channel / return mode
- X CC / Y CC
- X curve / Y curve
- record-to-seq / status

### Task 4.3
Verify:
- no MIDI behavior regressions
- no redraw corruption
- no gesture conflict with bottom transport drawer

---

## Track 5 — Play drawer

### Task 5.1
Move Play page draw/touch code into `screens/PlayScreen.*`

### Task 5.2
Implement Play top drawer using shared strips.

Suggested strips:
- key / mode
- transpose / voicing
- arp mode / MIDI out
- velocity / velocity curve

### Task 5.3
Keep the main Play surface creation-focused.

---

## Track 6 — Sequencer drawer

### Task 6.1
Move sequencer page draw/touch code into `screens/SequencerScreen.*`

### Task 6.2
Implement Sequencer top drawer.
Do not overload it.

Suggested organization:
- Feel strip: swing / quantize
- Motion strip: humanize / random
- Context strip: lane / channel / BPM
- Utility strip: pattern actions if needed

### Task 6.3
Leave per-step micro-edit controls in the step editing flow, not all in the drawer.

---

## Track 7 — Settings and transport cleanup

### Task 7.1
Move transport presentation code into `screens/TransportScreen.*`

### Task 7.2
Move settings rendering/touch handling into `screens/SettingsScreen.*`

### Task 7.3
Keep runtime transport logic and persistence logic outside UI modules.

---

## Track 8 — Legacy compatibility cleanup plan

### Task 8.1
Document legacy NVS keys and mirrored fields still being preserved.

### Task 8.2
Create a versioned retirement note for:
- `inCh`
- `mThr`
- mirrored legacy input field behavior

### Task 8.3
Do not remove compatibility immediately unless explicitly targeted.

---

## Track 9 — Testability improvements

### Task 9.1
Identify logic inside UI-heavy files that can be extracted into testable helpers.

### Task 9.2
Add host-testable coverage for:
- layout rules
- dropdown value mapping
- settings normalization/migration assumptions
- transport helper logic when feasible

---

## Good stopping points

Use these as safe checkpoints:
1. docs cleaned up
2. UI foundation files added
3. X-Y drawer working
4. Play drawer working
5. Sequencer drawer working
6. transport/settings extracted
7. old UI code removed

---

## Definition of done

This project cleanup/UI upgrade pass is complete when:
- docs are truthful and less redundant
- `main.cpp` is materially smaller
- glossy shared components exist
- page drawers are modular
- old replaced UI code is being removed
- behavior is preserved

---

## Implementation snapshot (repo state)

Use this to see what is already landed vs still aspirational. Update when major slices merge.

| Track | Status |
|-------|--------|
| **1 — Documentation** | `README.md` and `DOCS_INDEX.md` are current; `DEV_ROADMAP.md` marked historical; `E2E_STATUS.md` labeled as dated runs with reading guide |
| **2 — UI foundation** | `src/ui/UiTypes.h`, `UiLayout.*`, `UiGloss.*` exist and are wired |
| **3 — Drawer framework** | `src/ui/UiDrawers.*` + top-drawer behavior in firmware |
| **4–6 — Per-screen drawers** | Play / Sequencer / X–Y surfaces use shared UI; bodies in `src/screens/*Screen.inc`, built from `src/screens/*Screen.cpp` with shared linkage via `m5chords_app/M5ChordsAppShared.h` |
| **7 — Transport / Settings** | Same pattern (`TransportScreen`, `SettingsScreen`); shell orchestration and globals still in `main.cpp` |
| **8 — Legacy keys** | Documented in `PERSISTENCE_KEYS.md` (schema v2, `inCh` / `mThr` / `mThM` policy, retirement note) |
| **9 — Tests** | Native tests for domain + layout + settings; `AppSettings::displayAutoDimIdleTimeoutMs()` and similar helpers are preferred for new host-testable mappings |

**Optional next:** further shrink `main.cpp` by moving non-screen helpers (e.g. play layout, MIDI debug surface) into dedicated `.cpp` units if you want smaller translation units (behavior-first).
