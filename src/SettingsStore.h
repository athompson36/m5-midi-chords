#pragma once

#include "AppSettings.h"
#include "SeqExtras.h"

struct ChordModel;

/// Call once from `setup()` before other NVS loads. Writes `schema` when missing / upgrades.
void prefsMigrateOnBoot();

void settingsLoad(AppSettings& s);
void settingsSave(const AppSettings& s);

void chordStateLoad(ChordModel& m);
void chordStateSave(const ChordModel& m);

/// Three independent 16-step patterns (sequencer lanes 1–3).
void seqPatternLoad(uint8_t out[3][16]);
void seqPatternSave(const uint8_t in[3][16]);

/// Per-lane MIDI out channels (1–16), defaults 1 / 2 / 3.
void seqLaneChannelsLoad(uint8_t out[3]);
void seqLaneChannelsSave(const uint8_t in[3]);

/// Chord voicing depth on Play: 4 = full, 1 = root only (used when MIDI note output exists).
void chordVoicingLoad(uint8_t* out);
void chordVoicingSave(uint8_t v);

/// UI color theme index 0 … (see kUiThemeCount in UiTheme.h - 1).
void uiThemeLoad(uint8_t* out);
void uiThemeSave(uint8_t v);

/// X–Y mapping: channel (1–16), CC numbers (0–127), curve per axis (0=lin,1=log,2=invert).
void xyMappingLoad(uint8_t* channel, uint8_t* ccA, uint8_t* ccB, uint8_t* curveA, uint8_t* curveB);
void xyMappingSave(uint8_t channel, uint8_t ccA, uint8_t ccB, uint8_t curveA, uint8_t curveB);

/// Metronome / transport prefs (click volume 0–100, toggles).
void transportPrefsLoadFromStore(uint8_t* clickVol, bool* metro, bool* countIn, bool* xyArm);
void transportPrefsSaveToStore(uint8_t clickVol, bool metro, bool countIn, bool xyArm);

/// Quantize / swing / chord random / per-step probability (per lane).
void seqExtrasLoad(SeqExtras* out);
void seqExtrasSave(const SeqExtras* in);

/// Per-step X/Y automation for the sequencer (0–127; 255 = empty).
void xyAutoPatternLoad(uint8_t a[16], uint8_t b[16]);
void xyAutoPatternSave(const uint8_t a[16], const uint8_t b[16]);
void xyAutoWriteStep(uint8_t step, uint8_t x, uint8_t y, uint8_t a[16], uint8_t b[16]);

/// Project / SD: BPM (40–300), optional custom folder name (empty = auto date_key_bpm), last backup folder basename.
void projectBpmLoad(uint16_t* bpm);
void projectBpmSave(uint16_t bpm);
void projectCustomNameLoad(char out[48]);
void projectCustomNameSave(const char* name);
void lastProjectFolderLoad(char out[48]);
void lastProjectFolderSave(const char* folderBasename);

/// Clears NVS app + chord state, applies defaults, and persists.
void factoryResetAll(AppSettings& s, ChordModel& m);
