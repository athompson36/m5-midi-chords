# M5 MIDI Chords — UI Layout Grid Spec + Before/After Mapping
## Cursor Implementation Reference

This file combines:
- pixel-precise layout geometry for the new UI
- before/after mapping of current UI concepts into the upgraded layout
- finger-friendly sizing rules
- drawer and strip coordinates for Play, Sequencer, X-Y, and Transport

Use this as the implementation-level layout source of truth during the UI refactor.

---

# 1. Global screen geometry

## Display
- Resolution: **320 x 240**
- Orientation: **landscape**

## Reserved regions
- **Status bar**: `x=0 y=0 w=320 h=24`
- **Content region**: `x=0 y=24 w=320 h=196`
- **Bottom bezel zone**: `x=0 y=220 w=320 h=20`

## Global spacing system
- Outer screen margin: **6**
- Drawer padding: **6**
- Strip gap: **6**
- Component gap inside strip: **6**
- Small radius: **6**
- Medium radius: **10**
- Large radius: **14**

## Finger-friendly target rules
- Preferred minimum button height: **44**
- Compact pill height: **22–24**
- Recommended drawer control height: **28–36**
- Sequencer step cell can be smaller due to density, but should stay as large as current layout allows

---

# 2. Global frame used by all main pages

## Status bar
`x=0 y=0 w=320 h=24`

Purpose:
- passive info only
- key / BPM / MIDI source / clock / state chips
- not a dense interaction area in v1

## Main page content area
`x=6 y=24 w=308 h=196`

## Bottom bezel / gesture bar
`x=0 y=220 w=320 h=20`

Purpose:
- BACK / SELECT / FWD labeling
- swipe-up origin for transport drawer
- page-local gesture hints only if lightweight

---

# 3. Top drawer shared geometry

## Standard top drawer shell
Use this for Play and X-Y.
- `x=6 y=24 w=308 h=120`
- radius: **14**

## Tall top drawer shell
Use this for Sequencer if needed.
- `x=6 y=24 w=308 h=126`
- radius: **14**

## Drawer content rect
For standard 120-high drawer:
- outer: `x=6 y=24 w=308 h=120`
- content: `x=12 y=30 w=296 h=108`

For 126-high drawer:
- outer: `x=6 y=24 w=308 h=126`
- content: `x=12 y=30 w=296 h=114`

## Drawer handle
- `x=136 y=28 w=48 h=4`
- radius: **2**

## Strip shell defaults
- shell x: **12**
- shell w: **296**
- internal content x: **16**
- internal content w: **288**
- shell radius: **10**

---

# 4. Strip layout formulas

These formulas should be reused rather than hand-recomputed.

## 2-up row inside 288px content width
- gap: **6**
- width each: `(288 - 6) / 2 = 141`

Positions:
- left: `x=16`
- right: `x=163`

## 3-up row inside 288px content width
- gap: **6**
- width each: `(288 - 12) / 3 = 92`

Positions:
- 1: `x=16`
- 2: `x=114`
- 3: `x=212`

## 4-up row inside 288px content width
- gap: **6**
- width each: `(288 - 18) / 4 = 67`

Positions:
- 1: `x=16`
- 2: `x=89`
- 3: `x=162`
- 4: `x=235`

---

# 5. PLAY PAGE — pixel grid spec

## Main page layout

### Main content frame
- `x=6 y=24 w=308 h=196`

### Suggested play page zones

#### Top info band
- `x=12 y=28 w=296 h=18`

Use for:
- compact key/mode text
- active chord/suggestion hint
- optional MIDI activity chip

Keep lightweight.

#### Primary chord field
- `x=16 y=52 w=288 h=132`

This is the main performance surface.

#### Bottom feedback band
- `x=12 y=188 w=296 h=24`

Use for:
- last chord
- simple status hint
- not dense controls

## Play surface suggested pad layout

This keeps the page finger-friendly and centered.

### Center key pad
- `x=110 y=118 w=100 h=52`
- radius: **14**

### Upper pads row
- pad 1: `x=16  y=52  w=88 h=42`
- pad 2: `x=116 y=52  w=88 h=42`
- pad 3: `x=216 y=52  w=88 h=42`

### Middle pads row
- pad 4: `x=16  y=100 w=88 h=42`
- center key pad occupies middle center
- pad 5: `x=216 y=100 w=88 h=42`

### Lower pads row
- pad 6: `x=16  y=148 w=88 h=42`
- pad 7: `x=116 y=148 w=88 h=42`
- pad 8: `x=216 y=148 w=88 h=42`

This gives:
- 8 surrounding pads
- 1 larger center key pad
- consistent breathing room around the central target

## Play top drawer layout

### Drawer shell
- `x=6 y=24 w=308 h=120`

### Strip 1 — Harmonic
- shell: `x=12 y=34 w=296 h=30`
- content: `x=16 y=40 w=288 h=22`

Controls:
- Key:  `x=16  y=40 w=141 h=22`
- Mode: `x=163 y=40 w=141 h=22`

### Strip 2 — Voice / routing
- shell: `x=12 y=70 w=296 h=30`
- content: `x=16 y=76 w=288 h=22`

Controls:
- Transpose: `x=16  y=76 w=67 h=22`
- Voicing:   `x=89  y=76 w=67 h=22`
- Arp:       `x=162 y=76 w=67 h=22`
- Ch:        `x=235 y=76 w=67 h=22`

### Strip 3 — Feel
- shell: `x=12 y=106 w=296 h=36`
- content: `x=16 y=112 w=288 h=28`

Controls:
- Velocity: `x=16  y=112 w=141 h=28`
- Curve:    `x=163 y=112 w=141 h=28`

---

# 6. SEQUENCER PAGE — pixel grid spec

## Main page layout

### Main content frame
- `x=6 y=24 w=308 h=196`

### Lane tabs row
- `x=12 y=28 w=296 h=24`

Recommended layout:
- allow 4 tabs or current lane count representation
- each tab should be at least 44px wide if tap-targeted directly

If 4 tabs:
- each tab width around 70 with light breathing room

### Step grid frame
- `x=12 y=58 w=296 h=132`

### Bottom feedback band
- `x=12 y=194 w=296 h=18`

Can show:
- selected step summary
- lane state
- subtle hint text

## Sequencer step cell recommendation

Assuming 16 visible steps across a single row-like grid band:
- container width: 296
- if 16 cells with 4px gap is too dense, keep current visual density but improve states, not size

A practical dense visual target:
- cell width ~14–16
- cell height ~24–28 in tight mode

If the existing implementation already has a proven layout, keep the geometry and upgrade the visuals first.

## Sequencer top drawer — Feel page

### Drawer shell
- `x=6 y=24 w=308 h=126`

### Strip 1 — Lane / channel
- shell: `x=12 y=34 w=296 h=30`
- content: `x=16 y=40 w=288 h=22`

Controls:
- Lane: `x=16  y=40 w=141 h=22`
- Ch:   `x=163 y=40 w=141 h=22`

### Strip 2 — Feel
- shell: `x=12 y=70 w=296 h=36`
- content: `x=16 y=76 w=288 h=28`

Controls:
- Swing: `x=16  y=76 w=141 h=28`
- Quant: `x=163 y=76 w=141 h=28`

### Strip 3 — Variation
- shell: `x=12 y=112 w=296 h=36`
- content: `x=16 y=118 w=288 h=28`

Controls:
- Rand:   `x=16  y=118 w=141 h=28`
- Human:  `x=163 y=118 w=141 h=28`

## Sequencer top drawer — Utility page

### Drawer shell
- `x=6 y=24 w=308 h=120`

### Strip 1
- shell: `x=12 y=34 w=296 h=30`
- content: `x=16 y=40 w=288 h=22`

Controls:
- BPM:   `x=16  y=40 w=141 h=22`
- Trans: `x=163 y=40 w=141 h=22`

### Strip 2
- shell: `x=12 y=70 w=296 h=30`
- content: `x=16 y=76 w=288 h=22`

Controls:
- Arp:   `x=16  y=76 w=141 h=22`
- Clear: `x=163 y=76 w=141 h=22`

### Strip 3
- shell: `x=12 y=106 w=296 h=30`
- content: `x=16 y=112 w=288 h=22`

Controls:
- Copy:   `x=16  y=112 w=141 h=22`
- Rotate: `x=163 y=112 w=141 h=22`

If Copy/Rotate are not implemented yet:
- keep positions
- render as disabled or placeholder

---

# 7. X-Y PAGE — pixel grid spec

## Main page layout

### Main content frame
- `x=6 y=24 w=308 h=196`

### Title/info band
- `x=12 y=28 w=296 h=18`

Use for:
- XY label
- compact route summary
- CC assignment summary only if lightweight

### X-Y pad frame
- `x=26 y=48 w=268 h=144`
- radius: **14**

This leaves comfortable outer margins and a large finger area.

### X/Y value band
- `x=26 y=196 w=268 h=18`

Split:
- X readout left
- Y readout right

Example:
- X value area: `x=26  y=196 w=128 h=18`
- Y value area: `x=166 y=196 w=128 h=18`

## X-Y top drawer layout

### Drawer shell
- `x=6 y=24 w=308 h=120`

### Strip 1 — Assignment
- shell: `x=12 y=34 w=296 h=30`
- content: `x=16 y=40 w=288 h=22`

Controls:
- Ch:   `x=16  y=40 w=92 h=22`
- X CC: `x=114 y=40 w=92 h=22`
- Y CC: `x=212 y=40 w=92 h=22`

### Strip 2 — Curves
- shell: `x=12 y=70 w=296 h=30`
- content: `x=16 y=76 w=288 h=22`

Controls:
- X Curve: `x=16  y=76 w=141 h=22`
- Y Curve: `x=163 y=76 w=141 h=22`

### Strip 3 — Behavior
- shell: `x=12 y=106 w=296 h=30`
- content: `x=16 y=112 w=288 h=22`

Controls:
- Return: `x=16  y=112 w=141 h=22`
- Rec→Seq:`x=163 y=112 w=141 h=22`

---

# 8. TRANSPORT BOTTOM DRAWER — pixel grid spec

This is approximate and should be adapted to the existing transport drawer behavior, but keep the structure transport-only.

## Bottom drawer shell
Suggested:
- `x=6 y=132 w=308 h=82`
- radius: **14**

## Transport control row
- `x=16 y=142 w=288 h=30`

3-up:
- Play: `x=16  y=146 w=92 h=24`
- Stop: `x=114 y=146 w=92 h=24`
- Rec:  `x=212 y=146 w=92 h=24`

## BPM row
- shell: `x=12 y=176 w=296 h=30`
- content: `x=16 y=182 w=288 h=22`

2-up:
- BPM:         `x=16  y=182 w=141 h=22`
- Tap/Alt ctl: `x=163 y=182 w=141 h=22`

If metronome/count-in need dedicated buttons, use another layout pass, but do not overload the page or top drawers with them.

---

# 9. Before/After mapping of current UI concepts

This section maps current functional UI concepts into their intended new locations.

Because the current repo keeps a lot of UI in `main.cpp`, this mapping focuses on feature ownership rather than exact current line-level placement.

## PLAY PAGE mapping

### Current / legacy concept
- key selection and mode access mixed into Play-related workflows
- voicing overlays / SELECT-latched edits
- secondary setup controls sharing attention with play surface

### New target
- main page keeps only performance pads and immediate feedback
- top drawer owns all secondary harmonic/performance controls

### Mapping table

| Current UI concept | New location | Notes |
|---|---|---|
| Center key trigger | Play main page | Remains prominent |
| Chord pad triggers | Play main page | Remain primary |
| Key selection access | Play top drawer | Can still open picker if needed |
| Mode selection | Play top drawer | ValuePill |
| Transpose | Play top drawer | Compact pill |
| Voicing selection | Play top drawer | Replaces inline burden |
| Arp mode | Play top drawer | Compact pill |
| MIDI out channel | Play top drawer | Compact pill |
| Output velocity | Play top drawer | SliderTile |
| Velocity curve | Play top drawer | ValuePill / segmented choice |
| Suggestion feedback | Play main page | Keep lightweight only |
| SELECT-based edit burden | Reduce/remove | Only if drawer fully replaces it |

---

## X-Y PAGE mapping

### Current / legacy concept
- top config pills visible on main page
- assignment and behavior controls competing with performance pad space

### New target
- main page becomes pad-first
- top drawer owns setup controls

### Mapping table

| Current UI concept | New location | Notes |
|---|---|---|
| X-Y pad | X-Y main page | Primary surface |
| Crosshair / gesture feedback | X-Y main page | Primary feedback |
| X value | X-Y main page | Bottom readout |
| Y value | X-Y main page | Bottom readout |
| Channel selector | X-Y top drawer | Assignment strip |
| X CC selector | X-Y top drawer | Assignment strip |
| Y CC selector | X-Y top drawer | Assignment strip |
| X curve selector | X-Y top drawer | Curves strip |
| Y curve selector | X-Y top drawer | Curves strip |
| Return-to-center | X-Y top drawer | Behavior strip |
| Record-to-seq | X-Y top drawer | Behavior strip |

---

## SEQUENCER PAGE mapping

### Current / legacy concept
- grid plus extra controls sharing page real estate
- lane context and feel controls competing with pattern surface
- step-level and pattern-level controls too close conceptually

### New target
- main page is grid-first
- top drawer owns pattern-level shaping
- step micro-edits remain local to step editing flow

### Mapping table

| Current UI concept | New location | Notes |
|---|---|---|
| Lane tabs | Sequencer main page | Remain top-level direct control |
| Step grid | Sequencer main page | Primary composition area |
| Playhead | Sequencer main page | Primary feedback |
| Selected step feedback | Sequencer main page or popup | Keep direct |
| Lane/channel context | Sequencer top drawer | Feel page strip 1 |
| Swing | Sequencer top drawer | Feel page strip 2 |
| Quantize | Sequencer top drawer | Feel page strip 2 |
| Random | Sequencer top drawer | Feel page strip 3 |
| Humanize | Sequencer top drawer | Feel page strip 3 |
| BPM | Sequencer top drawer | Utility page |
| Transpose | Sequencer top drawer | Utility page |
| Arp mode | Sequencer top drawer | Utility page |
| Clear lane/pattern | Sequencer top drawer | Utility page |
| Copy / Rotate tools | Sequencer top drawer | Utility page, disabled if not ready |
| Per-step arp/div/gate/etc. | Stay in step edit flow | Do not move to top drawer |

---

## TRANSPORT mapping

| Current UI concept | New location | Notes |
|---|---|---|
| Play / Stop / Rec | Bottom drawer | Remains global |
| BPM | Bottom drawer | Remains transport-level |
| Tap tempo | Bottom drawer | Remains transport-level |
| Metronome | Bottom drawer | If surfaced, keep here |
| Count-in | Bottom drawer | If surfaced, keep here |
| Clock mode quick access | Bottom drawer or transport-specific control path | Not in page drawers |

---

# 10. Finger-friendly UI improvement checklist

Use this as a practical validation list during implementation.

## Main pages
- No tiny setup pills on primary surfaces unless absolutely necessary
- No unnecessary duplication of controls between page and drawer
- Primary interaction targets are easy to hit one-handed
- Important controls are visually dominant

## Drawers
- Controls grouped into strips
- No mixed unrelated controls in one strip
- Pills large enough to tap comfortably
- SliderTiles wide enough to drag or tap without frustration
- One drawer never contains everything

## Visuals
- Gloss is subtle and readable
- Pressed states are obvious
- Active/focused states are easy to distinguish
- No visual clutter added purely for decoration

---

# 11. Recommended implementation order from this spec

1. Add `UiTypes` and move `Rect`
2. Add `UiLayout` using the exact geometry in this file
3. Add `UiGloss`
4. Wrap existing button drawing through `drawGlossButton()`
5. Build X-Y top drawer using the coordinates above
6. Move Play page to the new drawer layout
7. Move Sequencer page to the new drawer layout
8. Keep transport bottom-only and stable
9. Remove superseded inline UI only after parity is confirmed

---

# 12. Final rules

If a control is not needed during active performance, it belongs in the top drawer.

If a control is hard to tap, it is too small or in the wrong place.

If a page feels crowded, remove secondary controls from the page and move them into strips inside the drawer.

Main page = create  
Top drawer = shape  
Bottom drawer = time
