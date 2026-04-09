# MIDI input, transports, and clock — design spec

This document specifies how the firmware should treat **multiple MIDI transports**, **independent channel routing per transport**, **future DIN support**, and **MIDI clock → BPM display**. Implementation is staged (USB and Bluetooth first; DIN when hardware exists).

## Goals

1. **Independent receive channels** for each physical/logical path:
   - **USB** (device class–compliant MIDI over USB, when the CoreS3 stack exposes it as a host or device—implementation choice TBD).
   - **Bluetooth** (BLE MIDI profile).
   - **DIN** (5-pin MIDI IN via UART on a future revision or add-on board).

2. **MIDI clock follow** with a **discrete BPM readout** that **flashes in sync** with the master tempo (quarter-note or chosen subdivision—default: one flash per quarter note).

3. Settings remain **persisted in NVS** with a **versioned schema** when these fields land in code (see `DEV_ROADMAP.md` migration item).

---

## Current firmware snapshot (vs this spec)

Normative sections below are the target; this table only describes what ships **today** (`AppSettings`, `main.cpp` Transport screen + Settings).

| Topic | Shipped now | This spec (future) |
|-------|-------------|-------------------|
| MIDI IN filter | Per-transport **`midiInChannelUsb`**, **`midiInChannelBle`**, **`midiInChannelDin`** (0 = OMNI, 1–16); rows in **Settings** + USB quick dropdown on **Transport** | Keep independent per-transport channels (key names may settle to `inChUsb/inChBle/inChDin`) |
| MIDI OUT channel | **`midiOutChannel`**; Settings + Transport dropdown | Per-destination / thru TBD |
| Clock / routing UI | **`midiClockSource`**, **`midiTransportSend`**, **`midiTransportReceive`**, **`clkFollow`**, **`clkFlashEdge`** in Settings; realtime clock events gated by receive/source/follow | Source-priority policy and advanced timeout behavior |
| BPM / tempo UI | **Transport**: large **project** BPM (NVS), tap → numeric editor, long-press → tap tempo; **Play** corner BPM shown when following external clock; flash edge selectable (`1/4`/`1/8`) | Additional UX polish/timeout/fade behavior |
| Ingress & clock parse | **USB foundation shipped** (`MidiIngress` running-status + realtime + SPP, source-tagged dispatch, transport clock hooks + rolling BPM); BLE and DIN now both use byte-provider hooks (`m5ChordBleMidiRead`, `m5ChordDinMidiRead`) feeding the same parser queue; queue-overflow counters exposed per transport for debug (`QDrop`) | Hardware-validated USB / BLE / DIN full pipeline |

---

## Transport model

Each transport is a **separate ingress** into the same internal pipeline (note events, clock, etc.). Events carry a **source tag**: `Usb`, `Ble`, `Din` (future).

| Transport | Typical stack (ESP32-S3) | Notes |
|-----------|---------------------------|--------|
| USB | USB host or USB device MIDI class | Depends on whether the CoreS3 acts as USB host to a keyboard or as a device to a computer; channel filtering applies to the stream that arrives on that path. |
| Bluetooth | BLE MIDI (GATT) | Pairing and reconnection are UX concerns; channel filter is per-connection stream. |
| DIN | UART RX (31250 baud) + opto-isolator | **Future revision**: reserved GPIO / connector; same software path once bytes are framed as MIDI. |

**Routing rule:** For each incoming message, determine **transport** → apply that transport’s **receive channel filter** → if accepted, forward to chord analyzer / clock handler / thru as configured.

### BLE ingress integration hook (implemented)

`MidiIngress` exposes an optional C hook for the BLE transport layer:

- `extern "C" size_t m5ChordBleMidiRead(uint8_t* dst, size_t cap);`
- Called by `midiIngressPollBle()` every loop tick.
- Return the number of raw MIDI bytes copied into `dst` (up to `cap`).
- Default weak implementation returns `0` (safe no-op if BLE stack is absent).
- Returned bytes are enqueued and parsed through the same BLE source-tagged pipeline as test-fed bytes.

This keeps parser behavior identical across USB/BLE paths while allowing the BLE stack to be integrated independently.

### DIN ingress integration hook (implemented)

`MidiIngress` exposes an optional C hook for the DIN/UART transport layer:

- `extern "C" size_t m5ChordDinMidiRead(uint8_t* dst, size_t cap);`
- Called by `midiIngressPollDin()` every loop tick.
- Return the number of raw MIDI bytes copied into `dst` (up to `cap`).
- Default weak implementation returns `0` (safe no-op if DIN transport is absent).
- Returned bytes are enqueued and parsed through the same DIN source-tagged pipeline as test-fed bytes.

---

## Per-transport receive channel assignment

Each transport has its own setting:

- **Value `0`**: OMNI (accept all channels on that transport).
- **Values `1`–`16`**: Accept only that channel on that transport.

These are **independent**: e.g. USB can be OMNI, Bluetooth channel **3**, DIN (when present) channel **10** for drums-only testing.

### Suggested NVS keys (when implemented)

| Key | Meaning |
|-----|---------|
| `inChUsb` | USB MIDI IN filter (0–16) |
| `inChBle` | Bluetooth MIDI IN filter (0–16) |
| `inChDin` | DIN MIDI IN filter (0–16); ignored until DIN hardware enabled |

Legacy key `inCh` (single global MIDI in) should migrate to all three with the same stored value, or default all three to the old value on first boot after upgrade.

### Settings UI (future)

- Either a **MIDI inputs** sub-page or additional rows:
  - “MIDI in (USB): OMNI / CH n”
  - “MIDI in (BT): OMNI / CH n”
  - “MIDI in (DIN): OMNI / CH n” (dimmed or “N/A” until hardware reports present)

---

## MIDI clock and BPM display

### Clock follow

- **Toggle** (e.g. “Follow MIDI clock”): **Off** = internal timing only (or none); **On** = derive tempo from incoming **System Real-Time** and **System Common** messages as needed.

- **Sources:** Clock may arrive on **any** transport that is enabled; policy options:
  - **Preferred:** First transport that sends **Start** / **Timing Clock** after idle, or
  - **Priority order:** USB > Bluetooth > DIN (configurable later).

- **BPM estimation:** Standard **24 ticks per quarter note** (PPQN). Maintain rolling average of intervals between clock ticks to compute BPM; handle **Start**, **Continue**, **Stop**, and **Song Position Pointer** if song-relative display is ever added (v1 can be BPM + flash only).

### BPM readout (UX)

- **Placement:** Small text in a **fixed corner** (recommended: **top-right** on landscape play surface) so it does not overlap the chord grid or key readout.

- **Style:** **Small** font (size 1 or dedicated compact font), **low-contrast** color (e.g. dark grey / muted cyan) so it stays **discrete**.

- **Flash:** On each **quarter-note boundary** (configurable later to 8th), briefly **brighten** or **invert** the BPM text (or a single-pixel “dot” beside it) for **one frame period** (~50–100 ms) so it **pulses with the master tempo** without dominating the UI.

- **Fallback:** If clock stops or becomes unstable, show **“—”** or last BPM greyed out; optional timeout to hide BPM after N seconds without clock.

### Suggested NVS keys

| Key | Meaning |
|-----|---------|
| `clkFollow` | 0 = off, 1 = follow external MIDI clock |
| `clkFlashEdge` | 0 = quarter, 1 = eighth (optional v2) |

---

## Chord analyzer interaction (future)

When chord detection exists:

- **Per-transport filters** apply before note aggregation for “what keyboard played.”
- **Clock** is global: BPM display reflects the **selected master** clock source, independent of which transport carried notes.

---

## Panic policy (current snapshot)

Current firmware routes all panic calls through a centralized trigger helper and emits `all-notes-off`
(CC123 on channels 1-16) for these triggers:

| Trigger | Required now | Implemented |
|---------|--------------|-------------|
| Enter Settings (gesture/bezel paths) | Yes | Yes |
| Ring page navigation | Yes | Yes |
| Local transport stop | Yes | Yes |
| External transport stop (`clkFollow` + route gate) | Yes | Yes |
| Enter key picker from Play | Yes | Yes |
| Exit key picker (`Done` / `Cancel`) | Yes | Yes |
| Open project-name editor | Yes | Yes |
| Exit project-name editor (`Save` / `Cancel`) | Yes | Yes |
| Enter SD restore picker | Yes | Yes |
| Exit SD restore picker (`Cancel` / selection) | Yes | Yes |
| Single-project direct restore path | Yes | Yes |
| Factory reset | Yes | Yes |
| Settings save-and-exit | Yes | Yes |
| Recoverable SD error feedback paths (`restore`/`backup`/`list`) | Yes | Yes |

Optional/future policy candidates (not required in current UX contract):

- Non-fatal UI-only operations that do not change mode.
- Additional transport/system faults not yet surfaced as explicit user actions.

Validation aid:

- `MIDI Debug` now includes compact per-trigger panic counters (selected trigger groups) so
  hardware bring-up can verify policy wiring during real UI flows.

---

## Implementation order (suggested)

1. USB MIDI IN + single global channel filter (prove pipeline).
2. Split channel filters: **USB** vs **Bluetooth** in settings + NVS migration.
3. MIDI clock receive + BPM + corner flash.
4. BLE transport stack implementation of `m5ChordBleMidiRead` (queue/parser contract already in place).
5. DIN UART/provider path + `inChDin` (now wired in firmware; validate final pinout/hardware behavior).

---

## References

- MIDI 1.0: System Real-Time (0xF8 Timing Clock, 0xFA Start, 0xFC Stop), channel voice messages.
- `docs/ORIGINAL_UX_SPEC.md` — play surface and analyzer UX.
- `docs/DEV_ROADMAP.md` — milestones and NVS migration.
