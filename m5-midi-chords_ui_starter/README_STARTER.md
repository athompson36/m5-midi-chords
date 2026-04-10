# M5 MIDI Chords — UI Starter Pack

This folder is a **reference scaffold** from an earlier integration pass. The **canonical, buildable sources** for the glossy UI and top drawers live in the main firmware tree:

- [`src/ui/`](../src/ui/) — `UiTypes`, `UiLayout`, `UiGloss`, `UiDrawers`
- [`src/screens/*.inc`](../src/screens/) — Play / Sequencer / XY / Transport / Settings UI fragments included from [`src/main.cpp`](../src/main.cpp) (shared anonymous namespace with app state)
- [`src/screens/PlayDrawerLayout.h`](../src/screens/PlayDrawerLayout.h) — example extracted layout helper (native-testable)

The file names below describe the **intended** module split; the shipping project may use `.inc` plus small headers instead of standalone `.cpp` screen TUs until globals are lifted out of `main.cpp`.

## What this reference pack sketches

- `src/ui/UiTypes.h`
- `src/ui/UiLayout.h`
- `src/ui/UiLayout.cpp`
- `src/ui/UiGloss.h`
- `src/ui/UiGloss.cpp`
- `src/ui/UiDrawers.h`
- `src/ui/UiDrawers.cpp`
- Screen `.h` / `.cpp` pairs (conceptual; see firmware `src/screens/` for actual layout)
- [`docs/STARTER_INTEGRATION_NOTES.md`](docs/STARTER_INTEGRATION_NOTES.md)

## Important

These files are scaffolds. They are designed to:
- establish clean ownership boundaries
- give agents a sane starting point
- reduce pressure on `main.cpp`

They are **not** a drop-in compile guarantee without adaptation, because the app keeps many shared types and globals in [`src/main.cpp`](../src/main.cpp).

## Intended first integration order

1. Add `UiTypes.h`
2. Move/duplicate `Rect` into `UiTypes.h`
3. Add `UiLayout.*`
4. Add `UiGloss.*`
5. Wrap existing `drawRoundedButton()` through `UiGloss`
6. Add `UiDrawers.*`
7. Migrate `XyScreen` first
8. Then `PlayScreen`
9. Then `SequencerScreen`
