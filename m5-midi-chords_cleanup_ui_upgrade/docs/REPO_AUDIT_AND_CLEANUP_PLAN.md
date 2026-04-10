# Repo Audit and Cleanup Plan
## M5 MIDI Chords

## Executive summary

The project has outgrown its original “Chord Suggester” framing. The firmware now behaves more like a compact touchscreen MIDI instrument with multiple surfaces, transport, sequencer, X-Y modulation, persistence, backup/restore, and diagnostics.

The main problems are no longer “missing features.” The main problems are:

- documentation drift
- `src/main.cpp` carrying too many responsibilities
- legacy compatibility paths without a retirement plan
- overlapping roadmap/backlog documents
- UI implementation tightly coupled to page logic

This is a consolidation problem, not a rescue problem.

---

## Key findings

### 1. Documentation drift

Current repo docs do not all describe the same product state.

#### Highest-risk mismatches
- `README.md` still frames the firmware as a “Chord Suggester,” which undersells the current app.
- `README.md` describes navigation as though Transport is part of the fixed ring, but code currently treats Transport as a separate drawer/sheet entry path.
- `docs/DEV_ROADMAP.md` lags behind implemented features and newer backlog docs.
- `docs/E2E_STATUS.md` contains historically correct statements that can be misleading without “superseded” labeling.
- `docs/CURRENT_ROADMAP_AND_TODOS.md` is helpful, but overlaps with `TODO_IMPLEMENTATION.md` and `DEV_ROADMAP.md`.

#### Recommendation
Establish one authoritative implementation backlog, one release/testing log pair, and one product-facing summary.

---

### 2. `main.cpp` is too large and too central

`src/main.cpp` currently mixes:
- screen definitions
- UI state
- geometry
- drawing
- touch handling
- popup/dropdown logic
- MIDI event presentation
- transport output timing helpers
- top-level orchestration

This makes cleanup and visual polish harder than necessary. It also increases regression risk.

#### Recommendation
Refactor `main.cpp` toward:
- app orchestration only
- screen dispatch only
- minimal shared state wiring

Move rendering, layout, and drawer logic into dedicated modules.

---

### 3. Legacy compatibility is accumulating

The persistence layer is sensibly preserving legacy behavior, but there is no visible retirement plan yet.

Examples:
- legacy `midiInChannel`
- legacy NVS key `inCh`
- legacy compatibility key `mThr`

These are reasonable during migration, but should not remain indefinite defaults.

#### Recommendation
Define a compatibility sunset:
- keep reading legacy keys through the next release boundary
- stop writing legacy keys once migration confidence is high
- remove legacy mirrored fields after one deliberate version cutoff

---

### 4. UI architecture is still prototype-shaped

The app now contains several overlay/popup patterns:
- transport drawer
- key picker
- sequencer chord dropdown
- step edit popup
- settings dropdowns
- confirmation overlays

These are useful, but they are implemented as local state clusters inside `main.cpp` rather than a coherent drawer/overlay system.

#### Recommendation
Introduce:
- shared drawer/sheet primitives
- shared strip layout helpers
- shared glossy component primitives

Then migrate screens over to them incrementally.

---

### 5. Native test coverage is good but incomplete

The current native PlatformIO test environment excludes:
- `main.cpp`
- `SettingsStore.cpp`
- `SdBackup.cpp`
- `Transport.cpp`

Excluding `main.cpp` is normal.
Excluding `SettingsStore.cpp` and `Transport.cpp` leaves two important stateful areas outside host-side regression testing.

#### Recommendation
Increase coverage by extracting more testable logic out of those runtime-heavy files.

---

## What looks healthy already

These are strengths worth preserving:

- modular domain logic around chords/settings/persistence/MIDI transport pieces
- active backlog discipline
- good awareness of hardware E2E reality
- clear persistence ownership in `SettingsStore`
- explicit UX backlog thinking
- existing partial-redraw work in Play and some X-Y rendering paths

---

## Cleanup priorities

### Priority 1 — Fix documentation truthfulness
Do this before any large refactor.

Tasks:
- update `README.md` to reflect current scope and navigation
- mark `docs/DEV_ROADMAP.md` as stale or resync it
- clarify historical vs current truth in `docs/E2E_STATUS.md`
- decide whether `docs/CURRENT_ROADMAP_AND_TODOS.md` remains necessary

Deliverable:
- one clear “authoritative docs map”

---

### Priority 2 — Reduce `main.cpp` responsibility
This is the most important code cleanup.

First extraction targets:
- `UiLayout.*`
- `UiGloss.*`
- `UiDrawers.*`

Second extraction targets:
- `PlayScreen.*`
- `SequencerScreen.*`
- `XyScreen.*`
- `TransportScreen.*`
- `SettingsScreen.*`

Do not attempt a single giant rewrite.

---

### Priority 3 — Build the premium UI foundation
Before polishing pages individually, define:
- glossy button language
- strip system
- drawer shell
- value pill
- slider tile
- status badge

This should become the shared UI vocabulary.

---

### Priority 4 — Retire or quarantine legacy paths
Document exactly what is legacy compatibility and when it can be removed.

Add a short section in persistence docs:
- legacy keys still read
- legacy keys still written
- removal target version

---

### Priority 5 — Expand testability
Extract logic so that:
- dropdown behavior
- layout rules
- settings mapping
- transport timing helpers
- migration behavior

can be tested without the full runtime screen stack.

---

## Recommended target document structure

### Keep
- `README.md`
- `docs/TODO_IMPLEMENTATION.md`
- `docs/HARDWARE_E2E_CHECKLIST.md`
- `docs/E2E_STATUS.md`
- `docs/PERSISTENCE_KEYS.md`
- MIDI / sequencer specs that are still active references

### Consolidate or retire
- `docs/DEV_ROADMAP.md`
- `docs/CURRENT_ROADMAP_AND_TODOS.md`

### Add
- `docs/DOCS_INDEX.md`
- `docs/UI_REFACTOR_MASTER_PLAN.md`
- `docs/UI_COMPONENT_SPEC.md`

---

## Suggested refactor milestone order

### Milestone A — Doc stabilization
- update README
- label stale docs
- add docs index
- define authoritative backlog

### Milestone B — UI foundation extraction
- add `UiLayout`
- add `UiGloss`
- add `UiDrawers`
- keep wrappers for existing draw helpers

### Milestone C — First drawer migration
- implement X-Y top drawer first
- lowest complexity, best proving ground

### Milestone D — Play drawer migration
- move harmonic context controls into Play drawer

### Milestone E — Sequencer drawer migration
- split into Feel / Utility groups if needed
- avoid overloading one drawer screen

### Milestone F — Old UI cleanup
- remove replaced helpers only after parity is verified

---

## Completion criteria for cleanup phase

The cleanup phase is successful when:

- README describes the actual current product
- there is only one authoritative backlog
- `main.cpp` is materially smaller
- glossy components live outside `main.cpp`
- at least one page drawer is modular and reusable
- legacy compatibility policy is documented
- old replaced UI code is being removed instead of accumulating

---

## Bottom line

This repo is growing like a real product.
The cleanup work should aim to preserve that momentum by making the structure match the maturity of the firmware.
