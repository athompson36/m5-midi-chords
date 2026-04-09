# DIN MIDI CHIP setup (M5Stack CoreS3)

This project now supports DIN MIDI I/O through a UART transport suitable for boards like BadassMIDI MIDI CHIP.

## Wiring

- CoreS3 `TX` -> MIDI CHIP serial input (DIN OUT path)
- CoreS3 `RX` <- MIDI CHIP serial output (DIN IN path)
- CoreS3 `GND` <-> MIDI CHIP `GND`
- Use the MIDI CHIP in **3.3V logic mode** for ESP32-S3 compatibility.

Default CoreS3 UART pins in this repo:

- `RX1 = GPIO44`
- `TX1 = GPIO43`

## Firmware behavior

- DIN MIDI runs at `31250` baud, `8N1`.
- DIN input feeds the shared MIDI ingress parser (`m5ChordDinMidiRead` path).
- DIN output is included in the MIDI fan-out path (`MidiOut` now writes to USB + BLE + DIN).

## Optional pin override

If your hardware routing needs different pins, set compile-time defines in `platformio.ini`:

```ini
build_flags =
  -DM5CHORD_DIN_RX_PIN=<gpio_number>
  -DM5CHORD_DIN_TX_PIN=<gpio_number>
```

Example:

```ini
build_flags =
  -DM5CHORD_DIN_RX_PIN=9
  -DM5CHORD_DIN_TX_PIN=10
```

## Quick validation checklist

1. Flash firmware.
2. Open `MIDI Debug` on device.
3. Send DIN notes to CoreS3 and verify DIN RX counters increase.
4. Tap chords on Play page and verify receiving DIN synth/module gets note events.
5. Trigger stop/panic and confirm notes release on the DIN target.
