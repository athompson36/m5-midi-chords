# UI Implementation Map
## File and responsibility plan

## Goal

Provide a safe extraction map from the current monolithic UI toward reusable modules.

---

## Target structure

```text
src/
  main.cpp

  ui/
    UiTypes.h
    UiLayout.h
    UiLayout.cpp
    UiGloss.h
    UiGloss.cpp
    UiDrawers.h
    UiDrawers.cpp

  screens/
    PlayScreen.h
    PlayScreen.cpp
    SequencerScreen.h
    SequencerScreen.cpp
    XyScreen.h
    XyScreen.cpp
    TransportScreen.h
    TransportScreen.cpp
    SettingsScreen.h
    SettingsScreen.cpp
```

This structure is a target, not a mandatory all-at-once move.

---

## Responsibility map

### `main.cpp`
Keep:
- setup
- loop
- app-level state initialization
- MIDI polling
- transport ticking
- screen dispatch

Move out:
- page rendering
- page touch handling
- component rendering
- drawer rendering
- layout geometry helpers

---

### `ui/UiTypes.h`
Owns:
- shared UI structs like `Rect`
- enums used across screen and drawer modules
- compact UI prop structs when needed

This file should be introduced early because it reduces friction for the rest of the extraction.

---

### `ui/UiLayout.*`
Owns:
- shared spacing constants
- common geometry helpers
- strip layout math
- split-row helpers
- drawer sheet bounds
- top/bottom reserved regions

This should replace repeated manual layout logic inside page code over time.

---

### `ui/UiGloss.*`
Owns:
- glossy component drawing
- highlight/shadow derivation
- component visual states
- reusable draw helpers

Should not own:
- page-specific hit testing
- business logic

---

### `ui/UiDrawers.*`
Owns:
- top drawer open/close state
- top drawer gesture handling
- drawer hit priority logic
- shared drawer shell rendering
- routing to per-page drawer content

Should not own:
- page domain state itself

---

### `screens/PlayScreen.*`
Owns:
- Play page layout
- Play page draw
- Play page touch processing
- Play-specific drawer content rendering and touch handling

---

### `screens/SequencerScreen.*`
Owns:
- sequencer grid rendering
- lane tab rendering
- sequencer page touch handling
- sequencer drawer rendering/touch handling
- later, step cell polish logic

---

### `screens/XyScreen.*`
Owns:
- X-Y page draw
- crosshair updates
- X-Y page touch handling
- X-Y drawer content and touch handling

This is the best first module to extract.

---

### `screens/TransportScreen.*`
Owns:
- transport drawer/page presentation
- BPM edit view
- tap tempo view
- transport touch handling

Transport behavior itself should remain in transport modules, not in screen code.

---

### `screens/SettingsScreen.*`
Owns:
- settings menu rendering
- settings section rendering
- settings dropdown rendering
- settings-specific touch handling

---

## Recommended extraction order

### Pass 1
Add shared files only:
- `UiTypes`
- `UiLayout`
- `UiGloss`

No large behavior changes yet.

### Pass 2
Move drawer shell behavior into `UiDrawers`.

### Pass 3
Extract `XyScreen`.

### Pass 4
Extract `PlayScreen`.

### Pass 5
Extract `TransportScreen`.

### Pass 6
Extract `SequencerScreen`.

### Pass 7
Extract `SettingsScreen`.

---

## Safe migration rules

### Rule 1
Prefer wrapper migration over delete-and-rebuild.

### Rule 2
Do not move transport/MIDI behavior into UI files.

### Rule 3
Do not create duplicate state sources.
Page modules should operate on existing shared state until a larger architecture pass is intentionally planned.

### Rule 4
Remove dead code only after replacement is confirmed working.

---

## First extraction shortlist

These are the best first things to move out of `main.cpp`:

- `Rect`
- rounded/glossy button drawing helpers
- shared top/bottom bezel layout helpers
- drawer shell geometry
- X-Y page draw + touch logic

This sequence gives the best payoff with the least risk.

---

## End state

The implementation map is successful when:
- page UI code is modular
- component rendering is centralized
- drawer logic is reusable
- `main.cpp` is mostly orchestration
