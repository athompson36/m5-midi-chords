# CURSOR_CONTEXT_UI_UPGRADE.md
## M5 MIDI Chords — Comprehensive UI Upgrade Context

### Purpose of this file

This file exists to give Cursor a complete mental model of the intended UI upgrade for the M5 MIDI Chords project.

It is not a generic coding note. It is an implementation context file that should help Cursor:
- understand the product direction
- understand the target architecture
- avoid breaking working musical behavior
- complete the `/src/ui` component system cleanly
- migrate the current UI away from a `main.cpp`-heavy structure

This file should be treated as implementation guidance plus product intent.

---

# 1. Product identity

This project is no longer just a chord suggester.

It is now a compact hardware MIDI instrument UI built around:
- chord generation
- sequencer control
- X-Y modulation
- transport control
- MIDI routing and monitoring
- persistence and project handling

The UI should feel like a premium hardware music device, not a utility app.

Desired feel:
- tactile
- glossy
- slightly rounded
- compact
- deliberate
- performance-first

Reference feel:
- Elektron
- Teenage Engineering
- boutique grooveboxes
- high-end MIDI controllers

Do not design this like:
- a settings-heavy phone app
- a flat admin dashboard
- a debug-first engineering tool

---

# 2. Core UX philosophy

The entire UI should be organized around three layers of intent:

## Main page = CREATE
This is where the user performs and composes.
Only the most important controls belong here.

## Top drawer = CONTROL
This contains page-specific parameters that matter, but are not needed every second.

## Bottom drawer = TRANSPORT
This is global timing control and should stay separate from page-level controls.

This model must remain consistent throughout the refactor.

---

# 3. Page intent

## Play page
The Play page is the harmonic performance surface.

It should feel immediate and expressive.

### The main Play page should keep
- center key pad
- chord pads
- immediate play feedback
- suggestion/performance feedback only if lightweight

### The Play top drawer should own
- key
- mode
- transpose
- voicing
- arp mode
- MIDI out channel
- velocity
- velocity curve

### What Cursor must avoid
- do not overload the Play page with setup controls
- do not leave old edit overlays active if the drawer replaces them
- do not move creation-critical controls off the main surface

---

## Sequencer page
The Sequencer page is the pattern creation page.

### The main Sequencer page should keep
- lane tabs
- step grid
- playhead
- immediate step feedback
- step editing entry points

### The Sequencer top drawer should own
- lane/channel context
- swing
- quantize
- random
- humanize
- BPM
- transpose
- pattern utility actions

### Important special rule
Do not stuff per-step micro-editing into the top drawer.
Per-step items still belong in step editing flow.

### Preferred drawer structure
Use two compact internal modes if needed:
- Feel
- Utility

Do not create a complex navigation system for this.

---

## X-Y page
The X-Y page is the modulation/performance surface.

### The main X-Y page should keep
- X-Y pad
- value readout
- activity feedback
- crosshair / current gesture feedback

### The X-Y top drawer should own
- channel
- X CC
- Y CC
- X curve
- Y curve
- return mode
- record-to-seq state

### Important
The X-Y page is the safest first page to migrate.
Use it as the proving ground for the drawer and component system.

---

## Bottom transport drawer
The transport drawer should remain transport-only.

It should not become a catch-all drawer.

It is responsible for:
- play
- stop
- record
- BPM
- metronome
- count-in
- transport timing related quick controls

---

# 4. High-level architecture direction

The current project has a large `main.cpp` which mixes:
- layout
- geometry
- drawing
- touch handling
- popup logic
- runtime dispatch
- MIDI-related UI state

This worked for growth, but now it is the main obstacle to polish.

The target architecture is:

```text
src/
  main.cpp

  ui/
    UiTypes.h/.cpp? (header-only is fine)
    UiLayout.h/.cpp
    UiGloss.h/.cpp
    UiDrawers.h/.cpp

  screens/
    XyScreen.h/.cpp
    PlayScreen.h/.cpp
    SequencerScreen.h/.cpp
    TransportScreen.h/.cpp
    SettingsScreen.h/.cpp
```

## Responsibility boundaries

### main.cpp
Should own:
- setup
- loop
- orchestration
- screen dispatch
- integration between runtime systems

Should not own:
- glossy component implementations
- drawer rendering
- repeated layout helpers
- large page-specific rendering blocks forever

### ui/UiTypes.*
Should own:
- shared geometry structs like `Rect`
- enums used across UI modules
- compact component spec structs

### ui/UiLayout.*
Should own:
- layout constants
- drawer geometry
- strip layout helpers
- 2-up / 3-up / 4-up control placement helpers

### ui/UiGloss.*
Should own:
- visual rendering primitives
- component style behavior
- color derivation from the existing theme system

### ui/UiDrawers.*
Should own:
- top drawer state
- open/close logic
- gesture handling
- drawer routing and overlay behavior

### screens/*
Should own:
- page-specific drawing
- page-specific touch handling
- page-specific drawer content

---

# 5. Non-negotiable implementation constraints

Cursor must preserve working product behavior.

## Do not change
- MIDI routing behavior
- MIDI ingress behavior
- SysEx behavior
- persistence keys/schema
- transport timing behavior
- project save/restore semantics

## Do not do
- do not rewrite the app architecture all at once
- do not dump new code back into `main.cpp`
- do not invent a second palette system
- do not remove working code before replacement is proven
- do not create one-off UI components for every screen
- do not put per-step sequencer editing into the top drawer

## Always do
- build reusable primitives first
- preserve behavior first
- migrate in small slices
- keep rendering cheap
- prefer wrapper migration over destructive rewrite

---

# 6. The new component system

The component system is the foundation of the UI upgrade.

Every component should feel related.
The UI should look like one product.

The primary components are:
- DrawerSheet
- StripShell
- GlossButton
- ValuePill
- SliderTile
- StatusBadge

The secondary components are:
- TabChip
- StepCell

These should be enough to build the upgraded UI without a lot of special-case rendering.

---

# 7. Component visual language

The UI should feel:
- glossy
- rounded
- tactile
- layered

Depth should come from:
- top highlight bands
- subtle inner highlights
- bottom shadow edges
- pressed-state inset feel
- focused rings

Avoid:
- expensive gradients
- noisy effects
- excessive animations
- chrome-like skeuomorphism

The goal is premium hardware UI, not fancy graphics.

---

# 8. Explicit instructions for `/src/ui` components

This section is the direct implementation guidance for Cursor.

## 8.1 `src/ui/UiTypes.h`

### Purpose
Create a shared home for neutral UI types that are currently trapped in `main.cpp` or implied in ad-hoc ways.

### Required contents
At minimum:
- `Rect`
- `GlossRole`
- `GlossState`
- `TopDrawerKind`
- `GlossColors`
- `GlossButtonSpec`
- `DrawerSheetSpec`
- `StripShellSpec`
- `ValuePillSpec`
- `SliderTileSpec`
- `StatusBadgeSpec`
- `TopDrawerState`
- `LayoutMetrics`

### Explicit instructions
- keep these types lightweight
- do not add business logic here
- keep defaults safe and simple
- avoid dependencies on large runtime headers
- this file should be easy to include almost anywhere

### Important
If `Rect` currently exists in `main.cpp`, move or mirror it here first.
This is one of the highest-value first cleanup steps.

---

## 8.2 `src/ui/UiLayout.h/.cpp`

### Purpose
Own all shared layout logic so page modules do not each reinvent geometry.

### Required helper functions
At minimum:
- `insetRect()`
- `makeTopDrawerRect()`
- `makeDrawerContentRect()`
- `makeStripRect()`
- `layout2Up()`
- `layout3Up()`
- `layout4Up()`

### Explicit instructions
- use the agreed 320x240 geometry assumptions
- centralize margins and gaps in `LayoutMetrics`
- avoid magic numbers spread through page code
- keep these helpers dumb and reusable

### Intended use
Pages and drawers should ask layout helpers for rectangles.
They should not manually duplicate strip spacing everywhere.

### Good behavior
- deterministic layout
- no side effects
- easy to unit-test later

---

## 8.3 `src/ui/UiGloss.h/.cpp`

### Purpose
This is the shared rendering vocabulary for the new premium UI.

It should be the single visual layer for glossy components.

### Required public functions
At minimum:
- `resolveGlossColors()`
- `drawGlossButton()`
- `drawDrawerSheet()`
- `drawStripShell()`
- `drawValuePill()`
- `drawSliderTile()`
- `drawStatusBadge()`

Later:
- `drawTabChip()`
- `drawStepCell()`

### Critical instruction
Do not create a second theme system.

`UiGloss` must derive its colors from the existing theme/palette system already in the project.

That means:
- take the existing palette as source of truth
- compute lighter/darker companions for gloss and shadow
- do not replace the app’s personality
- do not hardcode a whole unrelated color table as the permanent solution

### Required behavior per component

#### `drawGlossButton()`
Must provide:
- rounded shell
- top gloss band
- 1 px inner highlight
- 1 px lower shadow
- pressed state that shifts content down slightly
- optional focus ring
- optional active ring

This function is the likely replacement target for the current flat button helper.

#### `drawDrawerSheet()`
Must provide:
- sheet container with larger radius
- visible separation from the page beneath
- optional handle/grab line
- title support if needed
- cheap rendering, not full heavy effects

#### `drawStripShell()`
Must provide:
- a clear backing section inside the drawer
- lower visual weight than the drawer itself
- consistent spacing framework for grouped controls

#### `drawValuePill()`
Must provide:
- compact capsule-like control
- label/value pairing
- readable at small sizes
- tap-to-cycle friendly appearance

#### `drawSliderTile()`
Must provide:
- shell
- label/value row
- visible track
- clear fill/thumb treatment
- works for parameter amounts like swing/velocity/etc.

#### `drawStatusBadge()`
Must provide:
- compact chip
- small readable status indicator
- low emphasis unless active

### Performance rules
- keep drawing operations cheap
- prefer banded fills over gradients
- support partial redraw workflows
- avoid full-screen visual effects

### Coding rules
- keep component APIs small and clear
- do not hide business logic inside draw functions
- rendering code should not mutate musical state

---

## 8.4 `src/ui/UiDrawers.h/.cpp`

### Purpose
Create a consistent top-drawer system that can be shared by Play, Sequencer, and X-Y.

### Required public behavior
At minimum:
- determine whether a screen allows a top drawer
- map current screen to drawer type
- open drawer
- close drawer
- test whether drawer is open
- handle top-edge gesture
- route drawer rendering

### Required state
At minimum:
- current drawer kind
- ignore-until-touch-up flag
- swipe tracking state
- swipe start position
- optional subpage index

### Explicit interaction rules
- swipe down from top edge opens the top drawer
- drawer only allowed on Play, Sequencer, X-Y
- drawer must not open over settings/modals/transport
- only one drawer at a time
- when a drawer is open, it owns input priority
- outside tap may close drawer if appropriate

### Important
The transport drawer is already a separate concept.
Do not break it while adding the top drawer framework.

### Important implementation note
`UiDrawers` should manage drawer state and generic dispatch, but page-specific drawer content should still live in the corresponding screen module.

That means:
- generic open/close logic in `UiDrawers`
- `drawPlayTopDrawer()` in Play screen module
- `drawSequencerTopDrawer()` in Sequencer screen module
- `drawXyTopDrawer()` in X-Y screen module

---

# 9. Screen extraction instructions

## 9.1 XyScreen
This should be the first extraction target.

### Why
- simplest page for proving drawer behavior
- cleanest top drawer
- lowest migration risk
- immediate visual payoff

### Required final shape
- page draw function
- page touch function
- top drawer draw function
- top drawer touch function

### Main page should keep
- pad
- values
- activity

### Drawer should own
- CC assignments
- curves
- channel
- return/record behavior

---

## 9.2 PlayScreen
This should be the second extraction target.

### Required final shape
- page draw function
- page touch function
- top drawer draw function
- top drawer touch function

### Main page should keep
- center key pad
- chord pads
- immediate performance feedback

### Drawer should own
- key/mode
- transpose
- voicing
- arp mode
- output channel
- velocity
- curve

### Special instruction
If old voicing overlays or SELECT-based edit burdens become redundant after the drawer is working, reduce or remove them carefully.
Do not break functionality while doing this.

---

## 9.3 SequencerScreen
This should be the last of the main page extractions.

### Why
- highest density
- most complex interaction surface
- easiest place to accidentally overbuild the drawer

### Required final shape
- page draw function
- page touch function
- top drawer draw function
- top drawer touch function

### Main page should keep
- grid
- lane tabs
- playhead
- direct step interaction

### Drawer should own
- timing/feel controls
- pattern-level musical context
- utility actions

### Special instruction
The sequencer drawer may need two compact internal modes:
- Feel
- Utility

Use that only if needed.
Do not create a bloated tab system.

---

# 10. Strip system instructions

The drawer should not be a random pile of controls.

Everything should be arranged in component strips.

## Strip types
Use only a few strip types:
- Value strip
- Parameter strip
- Action strip

## Preferred row patterns
- 2-up most often
- 3-up when compact
- 4-up only when labels are short and controls are truly compact

## Why
This gives consistency, clarity, and easier implementation.

---

# 11. Migration order Cursor should follow

This order matters.

## Phase 1
Add `UiTypes`, `UiLayout`, and `UiGloss` with scaffolds.

## Phase 2
Bridge current button rendering through `UiGloss`.
Do not fully rewrite page UIs yet.

## Phase 3
Add `UiDrawers`.

## Phase 4
Implement X-Y drawer first.

## Phase 5
Implement Play drawer.

## Phase 6
Implement Sequencer drawer.

## Phase 7
Add secondary components like `TabChip` and `StepCell`.

## Phase 8
Remove obsolete inline UI code only after parity is confirmed.

---

# 12. What done looks like

The UI upgrade is successful when:
- the app feels more like a premium hardware instrument
- main pages are cleaner
- top drawers own secondary controls
- transport remains globally accessible
- the glossy component system is real and reusable
- `main.cpp` is smaller and less UI-heavy
- replaced old UI code is being deleted, not left to rot
- MIDI, transport, and persistence behavior are unchanged

---

# 13. Explicit anti-patterns Cursor must avoid

Do not:
- create duplicate state sources for the same setting
- create one-off component renderers for every page
- move everything at once
- make the drawer a dumping ground
- hide critical creation controls in drawers
- add visual effects that hurt performance
- keep building in `main.cpp` because it is easier in the moment

If something is reusable, put it in `/src/ui`.
If something is page-specific, put it in `/src/screens`.
If something is transport/MIDI logic, keep it out of the UI modules.

---

# 14. Final implementation mindset

Think like you are turning a successful prototype into a product.

The current code already proves the musical concept.
The new work is about making the structure and interface worthy of the product it has become.

Main page = create.
Top drawer = shape.
Bottom drawer = time.

Everything should reinforce that model.
