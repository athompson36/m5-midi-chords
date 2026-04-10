# UI/UX polish & interaction backlog

**Purpose:** Track scrolling-vs-tap, redraw efficiency, transport clock behavior, settings completeness, and sequencer/arp UX—aligned with product feedback (2026).

**Related:** `docs/TODO_IMPLEMENTATION.md` Phase 11 · `docs/MIDI_INPUT_SPEC.md` · `docs/SEQUENCER_AND_SHIFT_UX_SPEC.md`

---

## 1. Scroll vs first-touch selection (high priority)

**Problem:** While dragging to scroll a long list or page, the first contact often selects a row or triggers an action.

**Surfaces to audit:**

| Surface | Mechanism today | Desired behavior |
|---------|-----------------|------------------|
| Transport MIDI/clock/strum dropdowns | `s_transportDropDragLastY`, `s_transportDropFingerScrollCount` | On lift: if `fingerScrollCount > 0`, **do not** apply row pick; require separate tap after scroll settles |
| Settings section list (if scrollable) | (verify) | Same: **scroll threshold** (e.g. 12–16 px movement) before row activation |
| Key/mode picker, project pickers | Full-screen lists | Drag = scroll only until release without qualifying scroll |
| Sequencer chord/step pickers | Dropdown overlays | Same pattern as transport |

**Implementation notes:**

- Introduce shared helper: `touchScrollAccum`, `kScrollThresholdPx`, `bool scrollGestureConsumedPick`.
- Optional: **short delay** before first hit-test (poor for music UX—prefer movement threshold).
- Document in `docs/MIDI_INPUT_SPEC.md` or a short `docs/TOUCH_GESTURES.md`.

**Todos**

- [x] Unify scroll-suppresses-pick across Transport dropdowns, Settings dropdowns (finger-drag + movement threshold), key picker, SD restore list, and sequencer chord dropdown (`kUiScrollSuppressPickPx`, `touchMovedPastSuppressThreshold()` in `main.cpp`).
- [ ] Add regression tests where feasible (simulated touch sequences on host).

---

## 2. Full-screen redraw / “flash” on every touch (high priority)

**Problem:** Play (chord pads), Sequencer, X–Y, and Transport **full clear + redraw** on many touch events, causing visible flashing.

**Current assets:**

- `playRedrawGridBand()`, `playRedrawAfterOutlineChange()`, `playRedrawClearFingerHighlight()` — partial grid refresh paths exist but many call sites still use `drawPlaySurface()`.
- `drawXyCrosshairOnly()` exists for lighter XY updates.

**Direction:**

- **Finger down / move:** update only the **pressed cell** (or crosshair), not `fillRect` full panel.
- **Finger up:** one redraw of affected widgets, or invalidate dirty rects.
- **Static chrome** (hints, bezel): redraw bezel only when needed (or batch in `endWrite`).

**Todos**

- [x] Play: touch-move / bezel slide uses **`playRedrawFingerHighlightOnly`** (dirty key + at most two chord cells); full **`drawPlaySurface`** when last-played outline mode flips (`fingerChord < -50`), count-in / live REC overlay, or voicing panel open.
- [ ] Sequencer: same for `fingerCell` highlight; avoid full clear when only playhead or one cell changes.
- [ ] X–Y: prefer `drawXyCrosshairOnly` on drag; full pad only on theme/page entry.
- [ ] Transport: split **static** (BPM block, buttons) vs **dynamic** (step, recording state); minimize `fillScreen` / full-height `fillRect`.
- [ ] Consider **M5.Display** sprite layer or **dirty flags** per region if partial APIs remain too slow.

---

## 3. Transport MIDI clock IN / OUT

**Symptoms:** Clock not received or not sent despite BLE/USB working for notes.

**Code paths:**

- **OUT:** `transportSendTickIfNeeded()` → `midiRouteWriteRealtime()` → `midiRouteWriteBytes(route, …)` for route `midiTransportSend` (1=USB, 2=BLE, 3=DIN). **Note:** Chord output uses **broadcast** (`usb`+`ble`+`din`); clock uses **single route** only—if Transport send is USB, BLE will not get clock unless route is changed or product adds fan-out.
- **Suppress echo:** When `transportExternalClockActive()`, outgoing clock is **suppressed** (avoid feedback).
- **IN:** Ingress dispatches `MidiEventType::Clock` / Start / Stop when `clkFollow` and receive route match `midiTransportReceive` + `midiClockSource` policy (see `main.cpp` / `MidiIngress`).

**Recent fix:** DIN route **3** now calls `dinMidiWrite()` for realtime bytes (was previously dropped).

**Todos**

- [ ] **Product decision:** Clock **fan-out** (USB+BLE+DIN) vs single route—document and optionally add “clock to all outputs” toggle.
- [ ] **Debug:** When clock missing, verify `midiTransportSend != 0`, `clkFollow` not blocking expectation, and host actually listens on the same physical path as notes.
- [ ] **USB:** Confirm `CONFIG_TINYUSB_MIDI_ENABLED` build and `usbMidiReady()` true (serial log at boot).
- [ ] **IN:** Verify `midiClockSource` / `midiTransportReceive` / per-transport IN channels match the cable in use.
- [ ] Extend **MIDI Debug** screen with last realtime bytes and clock tick rate.

---

## 4. Settings: missing rows & admin mode

**Gaps (typical):**

- Advanced / **admin** section: factory-adjacent tools, NVS schema version, “clear MIDI debug”, build fingerprint, optional **developer** flags (verbose serial, mock ingress).
- MIDI: all items from `MIDI_INPUT_SPEC` (per-port IN, thru mask, clock follow, flash edge, suggestion tuning) — track against `AppSettings` + `PERSISTENCE_KEYS.md`.
- **Consistency:** Section scrolling vs paged settings (if list exceeds screen).

**Todos**

- [ ] Inventory **every** persisted `AppSettings` / transport field vs visible UI; add rows or Advanced subsection.
- [ ] **Admin mode:** e.g. Settings → hold **SELECT** on build row → unlock hidden section (PIN optional later).
- [ ] Document admin capabilities in `README` (short) and `PERSISTENCE_KEYS.md`.

---

## 5. Sequencer / arp: SELECT-toggle tools, dropdowns, sliders

**Goal:** All **seq/arp** parameters reachable via **SELECT** (latched or hold-Shift per current UX), **finger-friendly** controls on/near steps: **dropdowns + sliders** (existing yellow tool row pattern is the baseline).

**Feature checklist (verify vs firmware):**

| Item | Target UX |
|------|-----------|
| Per-step clock div | Shift + step focus + top row / dropdown |
| Arp on/off, pattern, arp clock div | Same |
| Quantize / swing / step prob / chord random | Tool row + sliders (keep layout) |
| Global vs per-step | Clear labeling |
| SELECT toggles “seq tools” vs chord pick | Avoid gesture conflict |

**Todos**

- [ ] UX pass: one **mode indicator** (tools vs picker vs Shift) always visible.
- [ ] Ensure **all** `SeqExtras` fields have UI + persistence (incl. SD v2).
- [ ] Optional: merge small tools into **one** “Seq settings” sheet with tabs (reduces clutter).
- [ ] Add **haptic or color** affordance when SELECT changes mode (non-blocking).

---

## 6. Finger-friendly density

- [ ] Minimum touch targets ≥ 44 px equivalent where possible (already partially in `HARDWARE_E2E_CHECKLIST`).
- [ ] Reduce simultaneous controls on screen; progressive disclosure (Shift, long-press, second page).

---

## Tracking

| Doc | Role |
|-----|------|
| This file | Detailed UX/engineering backlog for polish |
| `docs/TODO_IMPLEMENTATION.md` | Phase 11 summary + links here |
| `docs/CURRENT_ROADMAP_AND_TODOS.md` | High-level phases |

*Created: 2026-04-09 — update as items ship.*
