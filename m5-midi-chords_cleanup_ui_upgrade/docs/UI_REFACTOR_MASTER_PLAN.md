# UI Refactor Master Plan
## M5 CoreS3 MIDI Chords

## Goal

Refactor the UI into a premium glossy, tactile, hardware-like interface while preserving all current musical behavior.

Main principle:

- main page = create
- top drawer = page controls
- bottom drawer = transport

---

## Product intent

This device should feel like a real instrument, not a settings-heavy utility app.

Desired feel:
- premium
- tactile
- slightly glossy
- compact
- intentional
- performance-first

Not desired:
- phone app flatness
- menu sprawl
- engineering/debug first impression

---

## Page model

### Play page
Purpose:
- chord performance
- fast creation
- direct harmonic interaction

Main page keeps:
- center key pad
- chord pads
- visual play feedback

Top drawer holds:
- key
- mode
- transpose
- voicing
- arp mode
- MIDI out
- velocity
- velocity curve

---

### Sequencer page
Purpose:
- pattern creation
- step editing
- live shaping

Main page keeps:
- lane tabs
- step grid
- playhead
- immediate step feedback

Top drawer holds:
- swing
- quantize
- random
- humanize
- lane/channel context
- BPM
- pattern utilities
- optional split groups like Feel / Utility

Avoid forcing every micro-edit into the drawer.

---

### X-Y page
Purpose:
- modulation and expressive control

Main page keeps:
- X-Y pad
- value feedback
- crosshair / activity

Top drawer holds:
- channel
- X CC
- Y CC
- X curve
- Y curve
- return mode
- record-to-seq
- optional 14-bit behavior presentation

---

### Bottom drawer
Purpose:
- global transport only

It should remain dedicated to:
- play
- stop
- record
- BPM
- metronome
- count-in
- clock / transport quick access if needed

Do not let it become a general-purpose settings drawer.

---

## Structural refactor order

### Step 1 — Add shared UI modules
Add:
- `src/ui/UiTypes.h`
- `src/ui/UiLayout.h/.cpp`
- `src/ui/UiGloss.h/.cpp`
- `src/ui/UiDrawers.h/.cpp`

At this stage, do not change behavior.
Just establish reusable homes for layout, component drawing, and drawer state.

---

### Step 2 — Wrap existing flat primitives
Current UI already relies heavily on a rounded button helper.
Do not delete it first.

Instead:
- wrap current button rendering behind a new component API
- preserve current behavior
- allow the glossy renderer to replace the visuals later

This lowers risk.

---

### Step 3 — Build drawer framework
Add a generic top drawer system:
- open/close state
- sheet geometry
- swipe-down / top-edge activation
- touch priority when open
- outside-tap close behavior if appropriate
- one-open-at-a-time policy

---

### Step 4 — Implement X-Y drawer first
Why first:
- fewer control types
- easy to validate drawer behavior
- immediate visible improvement
- lower behavioral risk than Play or Sequencer

---

### Step 5 — Implement Play drawer
Move harmonic context out of the main surface.
Keep the center area uncluttered.

---

### Step 6 — Implement Sequencer drawer
This should happen last because it has the most UX density and the greatest risk of becoming overloaded.

If needed:
- split into compact internal drawer tabs
- or alternate strips inside the same sheet

Keep navigation simple.

---

### Step 7 — Remove obsolete code
Only after parity is confirmed:
- delete superseded draw helpers
- delete old inline drawer logic
- delete duplicated layout blocks

---

## Visual design rules

### Material language
Use:
- slightly rounded corners
- top highlight bands
- bottom shadow lines
- mild inner stroke
- pressed state sink
- focused state ring
- restrained glow

Avoid:
- chrome look
- aggressive gradients
- noisy textures
- expensive animation

---

### Color behavior
Keep existing theme colors.
Do not invent a second palette system.

Instead:
- derive glossy states from the existing palette
- compute lighter and darker companions per component state

---

### Density rules
Inside drawers:
- use strips
- prefer 2-up layouts
- use 3-up layouts sparingly
- use 4-up only for compact chips

Avoid scrolling in first implementation unless necessary.

---

## Technical rules

### `main.cpp`
Should own:
- app setup
- loop
- screen routing
- high-level state wiring

Should not own:
- glossy component implementations
- drawer rendering
- shared layout helpers
- long page-specific UI chunks once extraction starts

---

### Draw vs touch
Where practical:
- drawing lives in one function
- touch handling lives in another
- avoid giant mixed functions

Good:
- `drawPlayTopDrawer()`
- `processPlayTopDrawerTouch()`

Bad:
- one huge function mixing layout, draw, hit test, and state mutation for unrelated features

---

## Acceptance criteria

The refactor is successful when:
- main pages are visually cleaner
- top drawers hold secondary controls
- transport still works globally
- no MIDI or persistence behavior regresses
- UI feels more premium without becoming slower
- `main.cpp` is smaller and easier to reason about
