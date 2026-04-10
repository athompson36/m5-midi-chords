# CURSOR_RULES.md
# M5 CoreS3 MIDI Chords — UI Refactor Rules

## Mission

Refactor the CoreS3 UI into a premium glossy, drawer-based interface while preserving all existing musical behavior.

Priorities:
1. Do not break MIDI, SysEx, sequencer, transport, or persistence.
2. Move secondary page controls into top drawers.
3. Keep the main pages focused on creation.
4. Prevent further UI sprawl inside `main.cpp`.

---

## Product Intent

The device should feel like a premium hardware instrument.

Use this mental model:

- Main page = create / perform
- Top drawer = page controls
- Bottom drawer = transport
- Settings = persistent / device / admin controls

Do not turn drawers into dumping grounds. Every drawer must have a clear purpose.

- Play drawer = harmonic context
- Sequencer drawer = pattern shaping
- X-Y drawer = modulation assignment

---

## Non-Negotiable Constraints

### Do not change these unless explicitly tasked
- MIDI routing behavior
- MIDI input parsing behavior
- SysEx behavior
- transport timing behavior
- persistence keys / schema
- project save / restore format
- working gesture behavior outside the drawer refactor

### Do not do these
- Do not rewrite the app architecture all at once
- Do not move everything into `main.cpp`
- Do not create a second theme system
- Do not replace working logic just because it looks messy
- Do not add heavy animations or full-screen gradients
- Do not mix per-step sequencer editing into the top drawer

### Always do these
- Reuse existing palette/theme data
- Keep rendering lightweight
- Prefer additive refactors
- Preserve current behavior first, then simplify safely
- Build reusable UI primitives before page-specific code

---

## File Ownership Rules

### `main.cpp`
Allowed:
- high-level integration
- screen dispatch
- wiring existing flow to new modules
- temporary wrapper bridges during migration

Not allowed:
- large new glossy drawing functions
- page-specific drawer rendering
- new layout helper collections
- new reusable UI primitives

If a function could reasonably live elsewhere, do not place it in `main.cpp`.

### `UiGloss.*`
Owns:
- glossy component drawing
- component spec structs
- gloss color derivation from `UiPalette`
- reusable visual primitives

### `UiLayout.*`
Owns:
- drawer geometry helpers
- strip layout helpers
- shared spacing and dimension constants
- 2-up / 3-up / 4-up row splitting

### `UiDrawers.*`
Owns:
- top drawer state
- top drawer open/close behavior
- top drawer gesture handling
- per-page top drawer drawing
- per-page top drawer touch handling

### `UiTheme.*`
Remains source of truth for base colors.
Do not fork or duplicate theme definitions.

---

## UI Architecture Rules

### Main page content
Keep only controls that are essential to active creation.

#### Play main page
Keep:
- center key pad
- chord pads
- immediate feedback

Move out:
- secondary harmonic controls
- voicing controls
- performance setup controls

#### Sequencer main page
Keep:
- lane tabs
- step grid
- playhead and step feedback

Move out:
- global pattern shaping controls
- noncritical utilities
- timing/musical context controls

#### X-Y main page
Keep:
- X-Y pad
- value feedback
- activity feedback

Move out:
- assignment controls
- curve controls
- behavior toggles

### Top drawer rules
- Only one top drawer may be open at a time
- Top drawer only opens on Play, Sequencer, X-Y
- Top drawer must not open over settings/modals/transport
- Top drawer content must be grouped into strips
- Prefer 2-up and 3-up layouts
- Avoid scrolling in v1
- Do not overload a drawer with every possible option

### Bottom drawer rules
- Bottom drawer remains transport-only
- Do not redesign its purpose during this phase

---

## Component Rules

Build and use these as the primary UI vocabulary:

- DrawerSheet
- StripShell
- GlossButton
- ValuePill
- SliderTile
- StatusBadge

Later:
- StepCell
- TabChip

### Visual rules
- Slightly rounded
- Glossy, not chrome
- Subtle 3D depth
- Banded fills, not expensive gradients
- 1 px highlight/shadow lines
- Pressed states should visibly depress
- Focused states should use crisp rings
- Active states may glow lightly

### Performance rules
- Avoid full-screen redraws unless necessary
- Prefer partial redraw compatibility
- Use simple banding and borders for depth
- Keep animations minimal and cheap

---

## Drawer Content Rules

### Play drawer
Allowed:
- Key
- Mode
- Transpose
- Voicing
- Arp mode
- MIDI out channel
- Velocity
- Velocity curve

Not allowed:
- anything unrelated to harmonic performance context
- device/admin settings

### Sequencer drawer
Allowed:
- lane
- channel
- swing
- quantize
- random
- humanize
- BPM
- transpose
- arp mode
- pattern utilities

Not allowed:
- per-step arp config
- per-step division editing
- per-step micro-edit controls

### X-Y drawer
Allowed:
- channel
- X CC
- Y CC
- X curve
- Y curve
- return mode
- record-to-seq

Not allowed:
- unrelated global MIDI settings
- transport controls

---

## Sequencer Special Rule

Do not force every sequencer secondary control into one overloaded drawer view.

Preferred model:
- Sequencer Drawer A = Feel
- Sequencer Drawer B = Utility

If you need subpages, use compact tab chips or simple toggles inside the drawer.
Do not create a complicated navigation model.

---

## Migration Rules

### Required order
1. Add UI modules
2. Build glossy primitives
3. Wrap existing flat button helper
4. Add layout helpers
5. Add top drawer framework
6. Implement X-Y drawer first
7. Implement Play drawer second
8. Implement Sequencer drawer last
9. Add special sequencer components
10. Remove obsolete UI code only after replacements work

### Wrapping rule
When replacing old drawing code, prefer a wrapper bridge first.
Do not do a destructive delete-first rewrite unless the replacement is already proven.

### Cleanup rule
Only remove old UI code after:
- replacement is integrated
- touch behavior is verified
- visual output is correct

---

## Touch and Gesture Rules

- Swipe down from top edge opens top drawer
- Swipe up from SELECT bezel opens transport drawer
- Do not allow gesture conflicts
- When drawer is open, drawer touch handling gets priority
- Main page touch should be blocked while interacting with the drawer
- Outside-tap may close drawer if appropriate
- Ignore carried touch until release when opening drawers from a swipe

---

## Persistence Rules

Any control moved into a drawer must continue to use the existing backing state and persistence flow.

Do not:
- rename keys
- create parallel values
- change save/load semantics

If a drawer exposes an existing setting, it must edit the existing value directly.

---

## Coding Style Rules

- Prefer small focused functions
- Prefer explicit names over clever abstractions
- Keep layout constants centralized
- Keep component APIs simple
- Avoid hidden side effects in drawing functions
- Separate drawing from touch handling where practical

### Good pattern
- `drawPlayTopDrawer()`
- `processPlayTopDrawerTouch()`

### Bad pattern
- giant mixed draw/input/state function with many unrelated branches

---

## Acceptance Rules Per Change

Every UI change must satisfy all of these:
- builds successfully
- no broken touch paths
- no visual clipping on 320x240
- no regressions in MIDI/transport behavior
- no drawer conflicts
- no duplicate UI ownership for the same control
- no obvious performance regressions

---

## Commit / Checkpoint Guidance

Prefer small vertical slices.

Good checkpoints:
1. glossy buttons only
2. X-Y drawer working
3. Play drawer working
4. Sequencer drawer working
5. StepCell / TabChip polish
6. old UI cleanup

Do not batch the entire refactor into one giant commit.

---

## If Unsure

When uncertain, choose the option that:
- preserves existing behavior
- reduces `main.cpp` responsibility
- increases component reuse
- keeps the main page cleaner
- fits the main/create, drawer/control, transport/bottom model

If there is a conflict between “fancier” and “safer,” choose safer.
