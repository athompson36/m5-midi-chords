# External MIDI clock & transport (implementation notes)

## MIDI 1.0 (relevant subset)

- **System Real-Time** (single-byte, may appear anywhere in the stream): `0xF8` Clock (24 per quarter note), `0xFA` Start, `0xFB` Continue, `0xFC` Stop.
- **Song Position Pointer** `0xF2` + 2 data bytes: coarse position in MIDI beats (implementation maps into the 16-step grid).
- **MMC** (SysEx): **Real Time** commands `F0 7F <id> 06 <cmd> F7` are handled when **Follow clock** and **Receive route** match the ingress source — **Stop** (0x01), **Play** (0x02), **Deferred Play** (0x03), **Pause** (0x09, treated like stop). Other MMC (Locate, punch, **MTC**, etc.) is **not** parsed.

## USB Device Class MIDI (ESP32-S3 / TinyUSB)

- Incoming data uses **`tud_midi_stream_read`**, which exposes a **linear MIDI 1.0 byte stream** (USB-MIDI 4-byte cable packets are decoded by TinyUSB).
- **`UsbMidiTransport.cpp`** registers the MIDI interface; **`m5ChordUsbMidiRead`** in that translation unit overrides the weak stub in `MidiIngress.cpp` (which would otherwise read **USB CDC Serial** — wrong for class-compliant MIDI).

## Routing in firmware (`AppSettings`)

- **`midiTransportReceive`**: which physical path may **arm** external sync — must be **USB** (etc.) and match the ingress source, not “Off”.
- **`clkFollow`**: if off, realtime transport messages are ignored for sync.
- **`midiTransportSend`**: single route for **outgoing** Clock / Start / Stop / Continue (notes may still fan out elsewhere — see `MIDI_STACK.md`).

## Why “Stop” on the host may not stop the M5

1. **No `0xFC` on this cable** — Some DAWs do not send MIDI Stop to every USB MIDI destination.
2. **Clock continues** — If the host keeps sending `0xF8` while “stopped” (free-running clock), the firmware cannot distinguish idle from play without `0xFC` or MMC; use **Stop on the M5** or configure the host to **stop MIDI clock** to the device when transport stops.
3. **Clock stops** — When the host **ceases** `0xF8`, the firmware treats **~700 ms** without clock activity while slaved as an **implicit stop** (see `transportExternalClockStalled`).

## Clock-only arming

If the host never sends `0xFA`/`0xFB`, the first `0xF8` can arm slaving (`transportOnExternalContinue`). After **MIDI Stop** (or inferred stall) while **following external clock**, or after **local Stop** when transport **was** slaved, re-arming from `0xF8` alone is **blocked** until the next **Start** or **Continue** (or MMC Play/Deferred) so continuous clock cannot immediately restart transport after Stop. **Local Stop** after **internal-only** play does **not** set that block, so host clock alone can slave again.

## Native Instruments Maschine (typical expectations)

- Enable MIDI clock / transport to the **CoreS3 USB MIDI** port used by this firmware.
- If sync still misbehaves, verify in Maschine’s MIDI I/O whether **Stop** and **Clock** are routed to that output; behavior differs by version and template.
