# Release Notes - v0.1.0

Release date: 2026-04-09

## Highlights

- Core play, sequencer, transport, shift layer, and X-Y MIDI workflows are integrated on CoreS3.
- Settings persistence covers NVS and SD backup/restore project flows.
- MIDI input/output path includes USB/BLE/DIN routing foundations, clock hooks, and diagnostics.
- Sequencer supports per-step probability, per-step div/arp controls, and shift-based step parameter edits.
- X-Y page now supports assignable channel/CC/curve, 14-bit CC auto-pairs on CC 0-31, and CC throttling.

## Known issues (v0.1.0)

- Hardware E2E checklist sign-off is still pending manual on-device completion.
- Root-level `firmware.bin` may lag behind current `.pio/build/m5stack-cores3/firmware.bin` unless explicitly refreshed before packaging.
- X-Y release policy is currently global (not per-axis policy variants yet).

## Recovery / support

### Reflash firmware

Use `FLASH_INSTRUCTIONS.md` and flash:

- `bootloader.bin` -> `0x0`
- `partitions.bin` -> `0x8000`
- `firmware.bin` -> `0x10000`

### NVS clear (factory reset path)

- From app settings: use Factory Reset to clear persisted app state and reapply defaults.
- If device state is severely inconsistent, erase and reflash:

```bash
python -m esptool --port YOUR_PORT erase_flash
```

Then flash all three images again.

### SD backup recovery

- SD data lives under `/fs-chord-seq/`.
- If restore fails due to corrupted files, remove or rename the affected project folder on SD and re-run backup from device settings.
- Keep at least one known-good backup folder before testing new builds.

## Build/CI alignment for this release

- CI jobs (`.github/workflows/ci.yml`) run:
  - `python3 -m platformio test -e native`
  - `python3 -m platformio run -e m5stack-cores3`
- Artifact metadata and checksums are tracked in `BUILD_INFO.md`.
