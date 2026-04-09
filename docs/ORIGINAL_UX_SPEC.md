# Original UX Reference Spec

This document captures the intended user experience for the chord-suggester firmware, based on the original round-display prototype and [demonstration video](https://www.youtube.com/watch?v=bHaTqlVzRXw).

## Hardware context

The original prototype used a **round GC9A01 display** (~1.28" 240×240) inside a knurled metal rotary-encoder housing with an RGB LED ring base. The CoreS3 port adapts this concept to a **320×240 rectangular IPS touchscreen** with multi-touch (no rotary encoder).

## Screen flow

```
Boot Splash ──tap──▶ Play Surface ──two-finger hold──▶ Settings
                         ▲                                  │
                         └──────── Save & exit ─────────────┘
```

### 1. Boot Splash

- Full-screen greeting with a prominent **"Hi! Let's Play"** button in the center.
- Single tap anywhere on the button transitions to the Play Surface.
- Displayed once after power-on; returning from Settings goes directly to the Play Surface.

### 2. Play Surface

The main interactive screen where the musician selects a key and sees chord suggestions.

#### Layout (original round display)

- **Center circle**: shows the currently selected **key** (e.g. "C", "F♯").
  Tapping the center cycles through keys (all 12 chromatic pitches).
- **6 surrounding buttons**: arranged radially, each showing a chord name derived from the selected key.
- Buttons are **color-coded by harmonic importance**:

| Role | Color | Example (key of C) |
|------|-------|---------------------|
| Tonic (I) | Shown in center | C |
| Principal chords (IV, V) | **Green** | F, G |
| Standard diatonic (ii, iii, vi) | **Blue / Cyan** | Dm, Em, Am |
| Tension / diminished (vii°) | **Red** | Bdim |

- Tapping a chord button "plays" it (visual highlight now; MIDI note-on/off in the future).

#### Layout (CoreS3 rectangular adaptation)

The same concept rearranged for a 320×240 touchscreen:

- **3×3 grid of equal squares**: the **center** cell is the key / heart control; the **eight** outer cells are chord pads (**I**, **ii**, **iii**, **IV**, **V**, **vi**, **vii°**, **♭VII**), placed clockwise from the top-left.
- All pads use the **same square** size; corner radius scales with cell size.
- **Bottom status bar**: last action / chord name feedback.

#### Heart / Surprise Chord

After the player taps **a threshold number of chord buttons** (e.g. 5 plays), the center key area briefly transforms into a **heart icon (♥)**. Tapping the heart plays a **surprise chord** — a more colorful or obscure voicing that fits the current key but is outside the standard diatonic set (modal interchange, secondary dominant, Neapolitan, tritone sub, etc.). This adds musical depth and discovery.

Once played (or after a short timeout), the heart reverts to the key display and the counter resets.

### 3. Settings

Entered from the Play Surface via **two-finger hold** on the left and right edges (~0.8 s).

Current adjustable values:

| Setting | Range | Default |
|---------|-------|---------|
| MIDI out channel | 1–16 | 1 |
| MIDI in channel | OMNI (0) or 1–16 | OMNI |
| Brightness | 10–100% | 80% |
| Output velocity | 40–127 | 100 |

Navigation: BACK / FWD move the highlighted row; SELECT changes the value. Last row is **Save & exit** (persists to NVS flash and returns to the Play Surface).

## Chord generation model

For a given **key** (one of the 12 chromatic pitches, assuming major scale):

| Degree | Quality | Role | Color |
|--------|---------|------|-------|
| I | maj | Tonic | Green (principal) |
| ii | min | Standard | Blue |
| iii | min | Standard | Blue |
| IV | maj | Principal | Green |
| V | maj | Principal | Green |
| vi | min | Standard | Blue |
| vii° | dim | Tension | Red |
| ♭VII | maj (borrowed) | Color / mixolydian borrow | Pink |

**Original round UI:** 6 radial buttons showed **ii–vii°** (tonic only in the center readout).

**CoreS3 port:** 8 square pads in a ring show **I, ii, iii, IV, V, vi, vii°, ♭VII** around the center key control.

### Surprise chord pool (per key)

A curated set of non-diatonic chords that sound interesting in the key context:

- **Secondary dominants**: V/V, V/ii, V/vi, etc.
- **Modal interchange** (borrowed from parallel minor): ♭III, ♭VI, ♭VII, iv
- **Neapolitan**: ♭II
- **Tritone substitution**: ♭II7 of V
- **Augmented sixth**: It+6, Fr+6, Ger+6

One is chosen at random (or round-robin) when the heart is tapped.

## Color palette

| Role | Hex | TFT constant |
|------|-----|---------------|
| Principal (green) | `#00C853` | Custom `0x0660` |
| Standard (blue/cyan) | `#00B0FF` | Custom `0x05BF` |
| Tension (red) | `#FF1744` | Custom `0xF8A8` |
| Heart | `#FF4081` | Custom `0xFA10` |
| Key center bg | `#2962FF` | Custom `0x2B1F` |
| Background | `#000000` | `TFT_BLACK` |
| Text | `#FFFFFF` | `TFT_WHITE` |

## Future enhancements (not in initial port)

- Rotary encoder emulation via swipe gestures on CoreS3.
- MIDI IN chord detection → automatic key tracking.
- RGB LED strip control via GPIO (if external NeoPixel ring is wired).
- Minor / modal key support toggle in settings.
- Chord voicing display (notes within the chord).
