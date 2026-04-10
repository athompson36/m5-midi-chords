# Starter Integration Notes

## Why this starter is scaffold-first

The current project keeps many important items inside `main.cpp`, including:
- UI state
- shared geometry structs
- theme/global drawing context access
- screen enums
- many runtime-owned globals

Because of that, a safe starter should establish structure first and move behavior in controlled slices.

## Recommended first edit sequence

### 1. Add `UiTypes.h`
Use it as the home for:
- `Rect`
- simple UI enums
- compact shared spec structs

### 2. Add `UiLayout.*`
No behavior change.
Start using it only for new drawer geometry and strip splits.

### 3. Add `UiGloss.*`
Start with:
- `drawGlossButton()`
- `drawDrawerSheet()`
- `drawStripShell()`
- `drawValuePill()`
- `drawSliderTile()`

### 4. Bridge old button rendering
Keep `drawRoundedButton()` in `main.cpp` temporarily, but route its visuals through `drawGlossButton()`.

### 5. Add `UiDrawers.*`
Use this as the home for:
- top drawer state
- open/close behavior
- top-edge gesture detection
- per-page top drawer dispatch

### 6. Migrate `XyScreen` first
This is the safest first screen extraction and the best proving ground for the top drawer pattern.

## Deliberate omissions in this starter

This starter does not try to:
- replace all existing globals
- rewrite all state ownership
- force a new app framework
- guess the exact signature of every current helper in the repo

That is intentional.
