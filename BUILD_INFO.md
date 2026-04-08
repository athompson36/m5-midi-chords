# Build Information

Firmware Build Date: April 8, 2026  
Target Board: esp32-s3-devkitc-1 (CrowPanel variant)  
Platform: espressif32 v5.3.0  
Framework: Arduino

## Build Configuration

```ini
[env:crowpanel_esp32s3]
platform = espressif32@~5.3.0
board = esp32-s3-devkitc-1
framework = arduino

upload_speed = 921600
monitor_speed = 115200

build_flags =
  -DARDUINO_USB_MODE=0
  -DARDUINO_USB_CDC_ON_BOOT=0
  -DMIDI_UART_RX_PIN=44
  -DMIDI_UART_TX_PIN=43
  -DCONFIG_TINYUSB_MIDI_ENABLED=1

lib_deps =
    lovyan03/LovyanGFX@^1.2.7
    adafruit/Adafruit NeoPixel@^1.12.3
    fortyseveneffects/MIDI Library@^5.0.2
  lathoub/BLE-MIDI@^2.2
```

## Binary Artifacts

| File | Size (bytes) | Size (human) | Flash Offset |
|------|--------------|--------------|--------------|
| bootloader.bin | 14,768 | 14.4 KiB | 0x0 |
| partitions.bin | 3,072 | 3.0 KiB | 0x8000 |
| firmware.bin | 1,053,152 | 1028.5 KiB | 0x10000 |

## MD5 Checksums

| File | MD5 |
|------|-----|
| bootloader.bin | d02f12d259aba791a0294bf5329afc3a |
| partitions.bin | 801ba71678a964614657a6d8fbc6baca |
| firmware.bin | 06f8753eeba87c2f381c217a7871ef6a |

## Notable Firmware Changes In This Distribution

- Suggested chord playback now learns strum timing from incoming played chords.
- Suggested chord playback alternates downstroke/upstroke direction between triggers for a more natural response.
- Emotional chord suggestion replaced with a weighted candidate approach:
- Key detection from recent chord history (last 3 chords)
- Borrowed candidates: bVI, bVII, iv, ii dim
- Functional candidates: V/V, Vsus, V7 flavor, IVadd9, ii7
- Combined score: bass motion + harmonic function + voicing proximity
- Voicing proximity now explicitly favors close movement from previous chord
- Emotional scoring parameters are now constants in src/main.cpp for easier tuning
- Passing-chord and touch UX improvements:
- Running suggestion is stopped before new passing chord starts
- Center circle now toggles passing mode on/off
- First center tap in welcome state seeds C major harmony
- Mechanical display button long-press improvements:
- Holding the encoder switch for 3 seconds resets the session to "Hi! Let's play"
- Chord history is cleared and key estimation is reset to `--`
- Related Keys page now supports instant key changes:
  - Selecting a chord on "Related Keys" page automatically changes to that chord's native key
  - Suggests diatonic chords for the new key immediately after key change
  - Returns to "Diatonic" page after key change for seamless transitions
- Chord History page now preserves state:
  - Tapping chords on the "Chord History" page plays them without modifying the history
  - History is only updated by MIDI keyboard input or tapping chords on other pages
  - Allows safe browsing and replaying past chords on the history page
- History page sequencer updates:
  - 8 history chords are shown and can be sequenced in order
  - Play/Stop transport in center circle with BPM display and encoder-based BPM edit
  - LED clock animation: yellow on chord change, violet between changes
  - One chord now spans 4 beats (whole-note behavior)
- Generated chord register now follows the user MIDI keyboard register dynamically
- USB MIDI device output is now exposed over the CrowPanel USB-C connection
- BLE MIDI peripheral output is available for wireless hosts
- MIDI DIN I/O on Serial1 at 31250 baud
- Suggestion playback channel: 0
- Held-note analysis cap: 12 notes
- Triple-tap touch sends all-notes-off

## Rebuild

```bash
pip install platformio
platformio run -e crowpanel_esp32s3
platformio run -e crowpanel_esp32s3 --target upload
```

## References

- ESP32-S3 resources: https://www.espressif.com/en/products/socs/esp32-s3/resources
- CrowPanel wiki: https://www.elecrow.com/wiki/CrowPanel_1.28inch-HMI_ESP32_Rotary_Display_Arduino_lesson1.html

---

Build info generated: April 8, 2026
