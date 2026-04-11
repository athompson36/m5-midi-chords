# NVS persistence keys (`m5chord` namespace)

ESP32 **Preferences** namespace: **`m5chord`**. All keys below are stored in flash unless noted.

**Schema versioning:** Key **`schema`** (uint8) matches **`kPrefsSchemaVersion`** in `SettingsStore.cpp` (currently **`2`**). `prefsMigrateOnBoot()` upgrades older stores: below **2**, legacy **`inCh`** is copied into **`inUsb`**, **`inBle`**, and **`inDin`** once. Bump `kPrefsSchemaVersion` when blob layouts change and add migration branches there.

| Key | Type | Purpose | Introduced |
|-----|------|---------|--------------|
| `schema` | u8 | Preferences schema version | v1 migration |
| `xpose` | u8 | Transpose: stored as `transpose + 24` (−24…+24) | transpose feature |
| `outCh` | u8 | MIDI out channel 1–16 | initial |
| `inCh` | u8 | Legacy MIDI in mirror (see below); 0=OMNI, 1–16 | initial |
| `inUsb` | u8 | MIDI in USB 0=OMNI, 1–16 | per-port in |
| `inBle` | u8 | MIDI in BLE 0=OMNI, 1–16 | per-port in |
| `inDin` | u8 | MIDI in DIN 0=OMNI, 1–16 | per-port in |
| `mTx` | u8 | MIDI transport send route 0–3 | AppSettings |
| `mRx` | u8 | MIDI transport receive route 0–3 | AppSettings |
| `mThM` | u8 | MIDI thru mask 0–7 (USB/BLE/DIN bits); canonical thru storage | AppSettings |
| `mClk` | u8 | MIDI clock source 0–3 | AppSettings |
| `bright` | u8 | Brightness % | initial |
| `adim` | u8 | Display auto-dim idle: 0=off, 1=30s, 2=1m, 3=5m | display power saving |
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

---

## Legacy compatibility (read/write policy)

Some keys exist for **backward compatibility** with older firmware or mirrored fields:

| Item | Role | Policy |
|------|------|--------|
| `inCh` | Legacy single MIDI-in channel | **Read** on boot as initial value for migration; **write** on each save as `midiInChannelUsb` so downgrades to firmware that only reads `inCh` still see a sensible default. Canonical per-port values are **`inUsb`**, **`inBle`**, **`inDin`**. |
| `mThr` | Legacy MIDI thru on/off | **Read** only if **`mThM`** is absent (255): maps to `midiThruMask` all-on vs off. **Write** still mirrors a 0/1 flag for old readers. Canonical mask is **`mThM`** (3-bit combination). |
| Legacy `seq` 16-byte read | Older single pattern | Wider blob preferred; store still accepts legacy read paths where implemented |

**Retirement note (not yet executed):** stop writing **`mThr`** (and optionally stop mirroring **`inCh`**) after a chosen release once field verification is complete; keep **`mThM`** / **`inUsb`**… as the source of truth. Bump **`schema`** / **`kPrefsSchemaVersion`** when removing reads.

**Sunset rule:** keep **reading** legacy keys until users are unlikely to downgrade across that boundary; **remove** legacy reads only after a documented version cutoff (document here).

Do not rename NVS keys or change save semantics for UI refactors—drawers and new screens must bind to existing `AppSettings` / store fields.
