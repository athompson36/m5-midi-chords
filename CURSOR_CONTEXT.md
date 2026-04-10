# CURSOR_CONTEXT.md
## M5 CoreS3 MIDI Chords — UI & UX Intent

# What This Project Is

This is not just a utility app.

This is a hardware instrument UI:
- chord generator
- sequencer
- performance controller
- MIDI brain

It should feel like:
- Elektron gear
- Teenage Engineering devices
- high-end stompboxes / grooveboxes

Every UI decision should reinforce that feeling.

---

# Core UX Philosophy

## 1. Separation of intent

The entire UI is built around three layers of intent:

### Main Page = CREATE
- immediate interaction
- performance
- composition
- lowest friction possible

### Top Drawer = CONTROL
- page-specific parameters
- important but not constantly used
- grouped logically

### Bottom Drawer = TRANSPORT
- global timing control
- start/stop/clock/BPM
- consistent across all pages

---

## 2. Pages are instruments, not menus

Each page should feel like its own instrument.

### Play
- chord performance surface
- expressive and immediate
- minimal UI clutter

### Sequencer
- step composition tool
- grid is the focus
- editing is direct

### X-Y
- modulation/performance surface
- clean, responsive, tactile

If something is not part of active creation, it should not live on the main page.

---

## 3. Drawers are not junk drawers

Each drawer has a clear purpose.

### Play Drawer
Harmonic context:
- key
- mode
- transpose
- voicing
- performance shaping

### Sequencer Drawer
Pattern shaping:
- swing
- quantize
- randomness
- lane/channel
- pattern tools

### X-Y Drawer
Modulation assignment:
- CC mapping
- curves
- behavior

Do not mix unrelated controls into drawers.

---

# Visual Philosophy

## 1. Material feel

UI should feel:
- glossy
- slightly rounded
- tactile
- layered

Not:
- flat mobile app
- heavy chrome
- overly animated

---

## 2. Depth language

Use:
- top highlight bands
- bottom shadow lines
- subtle inner highlights
- pressed states that sink

Avoid:
- gradients everywhere
- expensive rendering
- visual noise

---

## 3. Consistency over cleverness

All components should feel related:
- same corner radii
- same spacing system
- same interaction feedback
- same state transitions

---

# Component System Intent

All UI should be built from a small set of primitives:

- DrawerSheet
- StripShell
- GlossButton
- ValuePill
- SliderTile
- StatusBadge

Do not create one-off UI elements unless truly necessary.

---

# Layout Philosophy

## Strips, not chaos

Inside drawers:
- everything is arranged in component strips
- strips group related controls
- strips have consistent spacing and height

Patterns:
- 2-up most often
- 3-up when compact
- 4-up only for very small controls

---

## Spacing matters

- small margins everywhere
- consistent gaps
- avoid cramming controls
- prefer clarity over density

---

# Interaction Philosophy

## Touch behavior

- Tap = primary interaction
- Hold = optional
- Swipe = drawer control only

## Gesture model

- Swipe down from top opens page drawer
- Swipe up from SELECT opens transport
- Tap outside drawer may close it

Gestures must never conflict.

## Focus and state

Every component must clearly communicate:
- idle
- focused
- active
- pressed

Pressed state must feel physical.

---

# Performance Philosophy

This runs on CoreS3.

So:
- keep drawing cheap
- avoid full redraws
- prefer banded fills over gradients
- reuse drawing primitives

The UI should feel smooth, not flashy.

---

# Architecture Philosophy

## `main.cpp` is not the UI layer

`main.cpp` should:
- orchestrate screens
- route input
- call UI modules

It should not:
- contain large drawing systems
- own reusable components
- become a dumping ground

## Modules have clear roles

### UiGloss
how things look

### UiLayout
where things go

### UiDrawers
how drawers behave

Keep these boundaries clean.

## Refactor safely

Always:
- wrap existing behavior first
- replace incrementally
- remove old code after proof

Never:
- delete working code before replacement is proven

---

# Mental model

When making decisions, think:

Would this exist on a real hardware instrument?

If not:
- it probably belongs in a drawer
- or in settings
- or should not exist

---

# Target outcome

After refactor, the app should feel like:
- a dedicated music device
- not a developer tool
- not a settings-heavy touchscreen menu

It should feel:
- fast
- focused
- tactile
- intentional

---

# If uncertain

Choose the option that:
- keeps the main page cleaner
- reduces cognitive load
- improves touch clarity
- uses existing components
- preserves behavior

Avoid:
- extra complexity
- duplicate UI paths
- exposing too many controls at once

---

# Final principle

Main page = PLAY
Top drawer = SHAPE
Bottom drawer = TIME

Everything should follow this.
