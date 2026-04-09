# NVS persistence keys (`m5chord` namespace)

ESP32 **Preferences** namespace: **`m5chord`**. All keys below are stored in flash unless noted.

**Schema versioning:** Key **`schema`** (uint8): **`1`** = current layout. `prefsMigrateOnBoot()` in `setup()` writes `1` when missing or when upgrading from legacy (no key). Bump `kPrefsSchemaVersion` in `SettingsStore.cpp` when blobs change and add migration branches there.

| Key | Type | Purpose | Introduced |
|-----|------|---------|--------------|
| `schema` | u8 | Preferences schema version | v1 migration |
| `xpose` | u8 | Transpose: stored as `transpose + 24` (−24…+24) | transpose feature |
| `outCh` | u8 | MIDI out channel 1–16 | initial |
| `inCh` | u8 | MIDI in 0=OMNI, 1–16 | initial |
| `mTx` | u8 | MIDI transport send route 0–3 | AppSettings |
| `mRx` | u8 | MIDI transport receive route 0–3 | AppSettings |
| `mClk` | u8 | MIDI clock source 0–3 | AppSettings |
| `bright` | u8 | Brightness % | initial |
| `vel` | u8 | Output MIDI velocity | initial |
| `arpMode` | u8 | Global arpeggiator mode index | initial |
| `tonic` | u8 | Key index 0–11 | initial |
| `kmode` | u8 | `KeyMode` enum | initial |
| `seq` | bytes | 48 bytes = 3 lanes × 16 steps (legacy 16-byte read supported) | multi-lane seq |
| `seqCh` | bytes | 3 bytes: MIDI channel per lane | seq lanes |
| `voicing` | u8 | Chord voicing depth 1–4 (play) | voicing |
| `uiTheme` | u8 | UI theme index | themes |
| `xyCCA` | u8 | X–Y pad CC A | XY pad |
| `xyCCB` | u8 | X–Y pad CC B | XY pad |
| `clkVol` | u8 | Metronome click volume 0–100 | transport prefs |
| `metro` | u8 | Metronome on/off | transport prefs |
| `cntIn` | u8 | Count-in on/off | transport prefs |
| `xyArm` | u8 | XY record-to-sequence arm | transport prefs |
| `seqEx` | bytes | 58-byte `SeqExtras` blob (quantize/swing/rand/stepProb) | SeqExtras |
| `xyAu2` | bytes | 32 bytes: per-step XY automation A/B for seq | XY automation |
| `projBpm` | u32 | Project BPM 40–300 | project |
| `projNm` | string | Custom project name (SD) | project |
| `lastProj` | string | Last SD project folder basename | project |

Factory reset clears the namespace and rewrites defaults (`SettingsStore::factoryResetAll`).

See also: `src/SettingsStore.cpp`, `docs/TODO_IMPLEMENTATION.md` (Phase 0 migration task).
