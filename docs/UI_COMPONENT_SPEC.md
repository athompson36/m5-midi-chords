# UI Component Spec
## Glossy Component System for M5 MIDI Chords

## Purpose

Define a reusable visual language for the CoreS3 UI refactor.

This spec is for:
- consistent visuals
- consistent interaction feedback
- easier implementation reuse
- cleaner separation from page logic

---

## Visual intent

The UI should feel:
- glossy
- slightly rounded
- tactile
- layered
- premium

It should not feel:
- flat mobile
- hyper-skeuomorphic
- toy-like
- noisy

---

## Shared design rules

### Radius
Use soft radii, not sharp boxes.
Default component radii should feel consistent across the system.

### Depth
Depth comes from:
- highlight band near top
- subtle inner highlight
- lower shadow edge
- focused ring
- pressed inset

### Contrast
Text and control states must remain readable under:
- bright studio light
- dim room use

### Performance
Effects must be cheap:
- no expensive full-screen gradients
- prefer layered fills and 1 px accents
- no unnecessary animation loops

---

## Primary components

### 1. DrawerSheet
Purpose:
- container for top drawer content

Visual:
- slightly darker than page background
- rounded lower corners
- thin top highlight
- shadow edge at bottom
- optional grab line

Behavior:
- can open/close
- blocks main page interaction while active

---

### 2. StripShell
Purpose:
- group related controls in a drawer

Visual:
- panel-like section
- subtle border
- low-contrast depth
- internal padding

Use for:
- harmonic settings
- sequencer feel settings
- X-Y assignment groupings

---

### 3. GlossButton
Purpose:
- action or discrete state button

States:
- idle
- hover/focus
- active
- pressed
- disabled

Visual rules:
- rounded rectangle
- top sheen
- slight inner highlight
- pressed version sinks slightly
- active state can glow lightly

Use for:
- toggles
- actions
- mode buttons
- utility commands

---

### 4. ValuePill
Purpose:
- compact discrete value chip

Visual:
- tighter rounded capsule
- slightly raised
- readable at small sizes

Use for:
- mode
- channel
- curve
- status snippets
- compact parameter values

---

### 5. SliderTile
Purpose:
- finger-friendly continuous control

Visual:
- tile shell + internal track
- glossy thumb or fill indicator
- strong label/value pairing

Use for:
- velocity
- swing
- quantize
- humanize
- random amount

---

### 6. StatusBadge
Purpose:
- non-interactive or lightly interactive status display

Visual:
- compact capsule or chip
- low-emphasis background
- high-contrast text only when needed

Use for:
- lock state
- active profile
- route status
- armed state
- return mode

---

## Secondary components

### 7. TabChip
Purpose:
- compact tab selector inside drawers or dense sections

Use sparingly.
Prefer when one drawer needs two mini-subsections.

### 8. StepCell
Purpose:
- upgraded sequencer cell visual language

Should eventually support:
- chord label
- probability annotation
- arp marker
- division marker
- active playhead glow
- selected edit focus

---

## Motion / feedback rules

### Pressed state
Pressed must feel physical:
- slight downward shift
- stronger lower shadow
- reduced highlight

### Focused state
Focused controls should show:
- crisp ring
- not giant glow
- obvious but restrained

### Active state
Active controls may use:
- accent ring
- brighter fill
- subtle glow

Avoid stacking all effects at once.

---

## Layout rules

### Drawer layout
Default patterns:
- 2-up for major controls
- 3-up for compact related controls
- 4-up only for very short labels

### Spacing
Use a single spacing rhythm:
- outer margins small but consistent
- strip padding consistent
- gaps between components consistent

### Typography
Keep labels short.
Do not overfill components with explanatory text.

---

## Example strip groupings

### Play drawer strips
- key / mode
- transpose / voicing
- arp mode / MIDI out
- velocity / velocity curve

### Sequencer drawer strips
- lane / channel
- swing / quantize
- humanize / random
- BPM / utility

### X-Y drawer strips
- channel / return mode
- X CC / Y CC
- X curve / Y curve
- record-to-seq / status

---

## Implementation notes

The component system should be built so that page code asks for components, not raw drawing steps.

Preferred direction:
- define compact specs/props
- pass them to shared drawing helpers
- keep visual logic centralized

That allows the glossy look to evolve without rewriting every screen.

---

## Completion criteria

The component system is ready when:
- at least DrawerSheet, StripShell, GlossButton, ValuePill, and SliderTile are implemented
- X-Y drawer is built entirely from shared components
- the same components can be reused on Play and Sequencer without special-case hacks
