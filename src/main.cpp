#include <M5Unified.h>
#include <esp_random.h>
#include <stdlib.h>
#include <string.h>

#include "AppSettings.h"
#include "BleMidiTransport.h"
#include "BuildInfo.h"
#include "ChordModel.h"
#include "MidiChordDetect.h"
#include "MidiEventHistory.h"
#include "MidiInState.h"
#include "MidiIngress.h"
#include "DinMidiTransport.h"
#include "MidiOut.h"
#include "MidiSuggest.h"
#include "SdBackup.h"
#include "SettingsEntryGesture.h"
#include "Arpeggio.h"
#include "SettingsStore.h"
#include "SeqExtras.h"
#include "Transport.h"
#include "UiTheme.h"

#ifndef M5CHORD_ENABLE_MIDI_DEBUG_SCREEN
#define M5CHORD_ENABLE_MIDI_DEBUG_SCREEN 1
#endif

SeqExtras g_seqExtras;

// External linkage — referenced from Transport.cpp
uint16_t g_projectBpm = 120;
uint8_t g_seqPattern[3][16];
uint8_t g_seqLane = 0;
uint8_t g_xyValX = 64;
uint8_t g_xyValY = 64;
uint8_t g_xyAutoA[16];
uint8_t g_xyAutoB[16];

namespace {

uint16_t colorForRole(ChordRole role) {
  switch (role) {
    case ChordRole::Principal: return g_uiPalette.principal;
    case ChordRole::Standard: return g_uiPalette.standard;
    case ChordRole::Tension: return g_uiPalette.tension;
    case ChordRole::Surprise: return g_uiPalette.surprise;
  }
  return g_uiPalette.subtle;
}

struct Rect {
  int x, y, w, h;
};

// --- State ---
enum class Screen : uint8_t {
  Play,
  PlayCategory,
  Sequencer,
  XyPad,
  Transport,
  TransportBpmEdit,
  TransportTapTempo,
  MidiDebug,
  KeyPicker,
  Settings,
  ProjectNameEdit,
  SdProjectPick
};

int g_pickTonic = 0;
KeyMode g_pickMode = KeyMode::Major;

Rect g_keyPickCells[ChordModel::kKeyCount];
Rect g_modePickCells[static_cast<int>(KeyMode::kCount)];
Rect g_keyPickCancel;
Rect g_keyPickDone;

void drawKeyPicker();
void drawPlayCategorySurface(int fingerCell = -1);
void drawProjectNameEdit();
void drawSdProjectPick();
void drawSettingsUi();
void drawMidiDebugSurface();
void processMidiDebugTouch(uint8_t touchCount, int w, int h);
void processPlayCategoryTouch(uint8_t touchCount, int w, int h);
void navigateMainRing(int direction);
void midiPanicAllChannels();
enum class PanicTrigger : uint8_t {
  EnterSettings,
  RingNavigation,
  TransportStop,
  ExternalTransportStop,
  KeyPickerTransition,
  ProjectNameTransition,
  RestoreTransition,
  FactoryReset,
  SettingsSaveExit,
  ErrorPath,
  Count
};
void panicForTrigger(PanicTrigger trigger);
const char* panicTriggerShortLabel(PanicTrigger trigger);

ChordModel g_model;
AppSettings g_settings;
Screen g_screen = Screen::Play;

String g_lastAction = "";

bool wasTouchActive = false;
uint32_t touchStartMs = 0;

SettingsEntryGestureState g_settingsEntryGesture;
bool suppressNextPlayTap = false;

static uint32_t s_bezelLongPressMs = 0;
static bool s_bezelLongPressFired = false;
bool checkBezelLongPressSettings(uint8_t touchCount);

enum class HoldProgressOwner : uint8_t { None, SettingsSingle, SettingsDual, TransportBpm };
static HoldProgressOwner s_holdProgressOwner = HoldProgressOwner::None;

int g_settingsRow = 0;
bool g_factoryResetConfirmArmed = false;
char g_settingsFeedback[48] = "";
enum class SettingsConfirmAction : uint8_t { None, RestoreSd, ClearMidiDebug };
SettingsConfirmAction g_settingsConfirmAction = SettingsConfirmAction::None;

enum class SettingsPanel : uint8_t { Menu, Midi, Display, SeqArp, Storage };

SettingsPanel g_settingsPanel = SettingsPanel::Menu;
int g_settingsCursorRow = 0;
int g_settingsListScroll = 0;


constexpr int kBezelBarH = 20;

constexpr uint8_t kSeqRest = 0xFE;
constexpr uint8_t kSeqTie = 0xFD;
constexpr uint8_t kSeqSurprise = 0xFC;

// Play surface hit regions (set by computePlaySurfaceLayout)
Rect g_keyRect;
Rect g_chordRects[ChordModel::kSurroundCount];
Rect g_bezelBack;
Rect g_bezelSelect;
Rect g_bezelFwd;

// Sequencer: 4 bars × 4 beats × 3 lanes (see docs/SEQUENCER_AND_SHIFT_UX_SPEC.md)
Rect g_seqCellRects[16];
Rect g_seqTabRects[3];
uint8_t g_seqMidiCh[3] = {1, 2, 3};

/// 1 = root only … 4 = full voicing (for future MIDI chord output).
uint8_t g_chordVoicing = 4;

static bool s_drawPlayVoicingShift = false;
static int s_seqComboTab = -1;
static uint8_t s_seqGestureMaxTouches = 0;

enum class SeqTool : uint8_t { None, Quantize, Swing, StepProb, ChordRand };
static SeqTool g_seqTool = SeqTool::None;
static int8_t g_seqProbFocusStep = -1;
static Rect g_seqSliderPanel{};
static bool g_seqSliderActive = false;
static bool g_seqDraggingSlider = false;
/// While Shift is active, top row of seq cells (0-3) shows tool picks.
static bool s_seqSelectHeld = false;
static int s_seqChordDropStep = -1;
static int8_t s_shiftSeqFocusStep = -1;
static int8_t s_shiftSeqFocusStepByLane[3] = {-1, -1, -1};
static uint8_t s_shiftSeqFocusField = 0;  // 0 Div, 1 Arp, 2 Pat, 3 ADiv
static uint32_t s_seqStepDownMs = 0;
static int s_seqStepDownIdx = -1;
static bool s_seqLongPressHandled = false;
static int8_t s_shiftSeqNudgeDir = 0;
static uint32_t s_shiftSeqNudgeStartMs = 0;
static uint32_t s_shiftSeqNudgeLastMs = 0;
static bool s_shiftSeqNudgeConsumed = false;
static uint8_t s_playGestureMaxTouches = 0;
static bool s_playVoicingCombo = false;
/// Tap SELECT (single-finger) toggles key-edit mode: center key highlighted; tap key to open picker.
static bool s_playSelectLatched = false;

static bool s_playVoicingPanelOpen = false;
static Rect g_playVoicingPanel{};
static bool s_playVoicingDragging = false;
static uint8_t s_playVoicingDisplayed = 0;
static uint32_t s_playKeyHoldStartMs = 0;
static bool s_playKeyHoldArmed = false;
static bool s_playOpenCategoryOnRelease = false;
static uint32_t s_playBezelHoldStartMs = 0;
static uint32_t s_playBezelRepeatMs = 0;
static int8_t s_playBezelHoldDir = 0;
static bool s_playBezelLongPressConsumed = false;
static bool s_shiftActive = false;
static bool s_shiftSelectDown = false;
static bool s_shiftTriggeredThisHold = false;
static uint32_t s_shiftSelectDownMs = 0;
constexpr uint32_t kShiftHoldMs = 550U;

enum class PlayCategoryPage : uint8_t { History, Diatonic, Functional, Related, Chromatic, Count };
static PlayCategoryPage s_playCategoryPage = PlayCategoryPage::History;
static Rect g_playCategoryCells[ChordModel::kSurroundCount];
static uint8_t s_playCategoryItems[ChordModel::kSurroundCount] = {};
static uint8_t s_playCategoryCount = 0;
static uint8_t s_playChordHistory[16] = {};
static uint8_t s_playChordHistoryCount = 0;
static uint8_t s_playChordHistoryHead = 0;
static uint32_t s_playTouchStartMs = 0;
static uint32_t s_seqTouchStartMs = 0;
static uint32_t s_playCategoryTouchStartMs = 0;

constexpr uint32_t kPlayBezelCycleHoldMs = 450U;
constexpr uint32_t kPlayBezelCycleRepeatMs = 140U;
constexpr int kPlayHitPadPx = 5;
constexpr int kSeqHitPadPx = 4;
constexpr int kPlayCategoryHitPadPx = 5;
constexpr uint32_t kTapDebounceMs = 35U;
constexpr uint8_t kShiftPadCcMap[4] = {20, 21, 22, 23};
constexpr uint8_t kShiftPadProgramMap[4] = {0, 1, 2, 3};
static const char* kStepDivLabs[4] = {"1x", "2x", "4x", "8x"};
static const char* kArpPatLabs[4] = {"Up", "Dn", "UD", "Rnd"};
constexpr uint32_t kShiftSeqNudgeHoldMs = 320U;

Rect g_xyPadRect;
Rect g_xyCfgChRect;
Rect g_xyCfgCcARect;
Rect g_xyCfgCcBRect;
Rect g_xyCfgCvARect;
Rect g_xyCfgCvBRect;
uint8_t g_xyOutChannel = 1;
uint8_t g_xyCcA = 74;
uint8_t g_xyCcB = 71;
uint8_t g_xyCurveA = 0;
uint8_t g_xyCurveB = 0;
static uint8_t s_xyMidiSentX = 255;
static uint8_t s_xyMidiSentY = 255;
static uint32_t s_xyLastSendMsX = 0;
static uint32_t s_xyLastSendMsY = 0;
constexpr uint32_t kXyCcMinIntervalMs = 10U;
struct XyAxisPending {
  bool dirty = false;
  uint8_t channel = 0;
  uint8_t cc = 0;
  uint8_t value = 0;
};
static XyAxisPending s_xyPendingX{};
static XyAxisPending s_xyPendingY{};

int g_lastTouchX = 0;
int g_lastTouchY = 0;
int g_lastPlayedOutline = -1;

char g_projectCustomName[48] = "";
char g_lastProjectFolder[48] = "";

uint8_t g_uiTheme = 0;

constexpr int kProjEditLen = 24;
char g_projEditBuf[kProjEditLen + 1];
int g_projEditCursor = 0;
Rect g_peSave;
Rect g_peClear;
Rect g_peCancel;

char g_sdPickNames[8][48];
int g_sdPickCount = 0;
Rect g_sdPickRows[8];
Rect g_sdPickCancel;

// --- Geometry helpers ---

bool pointInRect(int px, int py, const Rect& r) {
  return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

static bool pointInRectPad(int px, int py, const Rect& r, int pad) {
  return px >= (r.x - pad) && px < (r.x + r.w + pad) && py >= (r.y - pad) &&
         py < (r.y + r.h + pad);
}

struct XyComboTrack {
  bool armed;
  int sx, sy, lx, ly;
};

XyComboTrack s_xyCombo{};

// Skip full-screen redraws while touch is held unless visual state changes (avoids flicker).
static int s_playTouchDrawChord = -100;
static bool s_playTouchDrawOnKey = false;
static bool s_playTouchDrawVoicing = false;
static int s_lastSeqPreviewCell = -9999;
static bool s_wasSeqMultiFinger = false;
static int s_lastDrawnSeqComboTab = -99999;
static int8_t s_settingsDropRowId = -1;
static int s_settingsDropOptScroll = 0;
static int8_t s_xyTouchZone = -1;
static bool s_xyTwoFingerSurfaceDrawn = false;
static MidiIngressParser s_midiIngressParser;
static MidiInState s_midiInState;
static MidiEventHistory s_midiEventHistory;
static uint32_t s_playClockFlashUntilMs = 0;
static char s_midiDetectedChord[12] = "";
static uint32_t s_midiDetectedChordUntilMs = 0;
static char s_midiDetectedSuggest[12] = "";
static uint32_t s_midiDetectedSuggestUntilMs = 0;
static char s_midiSuggestTop3[3][12] = {};
static int16_t s_midiSuggestTop3Score[3] = {0, 0, 0};
static uint32_t s_midiSuggestTop3UntilMs = 0;
static uint32_t s_midiSuggestStableUntilMs = 0;
static MidiSource s_midiSuggestLastSource = MidiSource::Usb;
static uint8_t s_midiSuggestLastChannel = 0;
static uint32_t s_midiSuggestLastAtMs = 0;
static char s_playIngressInfo[44] = "";
static uint32_t s_playIngressInfoUntilMs = 0;
static bool s_playIngressInfoDirty = false;
static uint32_t s_playIngressLastMusicalMs = 0;
static uint32_t s_playIngressInfoEventMs = 0;
static int8_t s_midiDbgSourceFilter = -1;  // -1 all, 0 usb, 1 ble, 2 din
static int8_t s_midiDbgTypeFilter = -1;    // -1 all, else MidiEventType value
static uint32_t s_bleDbgRatePrevMs = 0;
static uint32_t s_bleDbgRatePrevRx = 0;
static uint16_t s_bleDbgRateBps = 0;
static uint16_t s_bleDbgRatePeakBps = 0;
static uint32_t s_bleDbgLastActivityMs = 0;
static bool s_bleDbgPeakHeld = true;
static uint32_t s_dinDbgRatePrevMs = 0;
static uint32_t s_dinDbgRatePrevRx = 0;
static uint16_t s_dinDbgRateBps = 0;
static uint16_t s_dinDbgRatePeakBps = 0;
static uint32_t s_dinDbgLastActivityMs = 0;
static bool s_dinDbgPeakHeld = true;
static uint32_t s_outDbgRatePrevMs = 0;
static uint32_t s_outDbgRatePrevTx = 0;
static uint16_t s_outDbgRateBps = 0;
static uint16_t s_outDbgRatePeakBps = 0;
static uint32_t s_bleDbgPrevDrop = 0;
static uint32_t s_bleDbgWarnUntilMs = 0;
static Rect g_midiDbgSourceChip{};
static Rect g_midiDbgTypeChip{};
static Rect g_midiDbgResetChip{};
static uint32_t s_panicTriggerCounts[static_cast<size_t>(PanicTrigger::Count)] = {};

Rect g_trPlay;
Rect g_trStop;
Rect g_trRec;
Rect g_trMetro;
Rect g_trCntIn;
Rect g_trMidiOutBtn;
Rect g_trMidiInBtn;
Rect g_trSyncBtn;
Rect g_trStrumBtn;
Rect g_trBpmTap;
Rect g_trStepDisplay;

static uint16_t g_bpmEditValue = 120;
static Rect g_bpmEditM5;
static Rect g_bpmEditP5;
static Rect g_bpmEditM1;
static Rect g_bpmEditP1;
static Rect g_bpmEditOk;
static Rect g_bpmEditCancel;

static uint32_t settingsEntryHoldMs() {
  switch (g_settings.settingsEntryHoldPreset) {
    case 0:
      return 500U;
    case 1:
      return 650U;
    case 2:
      return 800U;
    case 3:
      return 1000U;
    default:
      return 1200U;
  }
}

static uint32_t seqLongPressMs() {
  switch (g_settings.seqLongPressPreset) {
    case 0:
      return 250U;
    case 1:
      return 300U;
    case 2:
      return 400U;
    case 3:
      return 500U;
    default:
      return 700U;
  }
}

static uint32_t transportBpmLongMs() {
  switch (g_settings.bpmHoldPreset) {
    case 0:
      return 350U;
    case 1:
      return 500U;
    case 2:
      return 700U;
    default:
      return 900U;
  }
}

static uint8_t applyOutputVelocityCurve(uint8_t raw) {
  uint8_t v = raw;
  if (v > 127U) v = 127U;
  switch (g_settings.velocityCurve) {
    case 1: {  // soft
      const uint16_t curved = (static_cast<uint16_t>(v) * static_cast<uint16_t>(v)) / 127U;
      return static_cast<uint8_t>(curved == 0U && v > 0U ? 1U : curved);
    }
    case 2: {  // hard
      const uint16_t inv = static_cast<uint16_t>(127U - v);
      const uint16_t curved = 127U - ((inv * inv) / 127U);
      return static_cast<uint8_t>(curved);
    }
    default:
      return v;
  }
}

struct SeqArpLaneRuntime {
  bool active = false;
  uint8_t channel0 = 0;
  uint8_t noteCount = 0;
  uint8_t notes[6] = {};
  uint8_t orderCount = 0;
  uint8_t orderIdx[16] = {};
  uint8_t emitPos = 0;
  uint32_t nextMs = 0;
  uint32_t endMs = 0;
  uint32_t intervalMs = 1;
};

static SeqArpLaneRuntime s_seqArpRt[3];

static void seqArpStopLane(uint8_t lane, bool silence) {
  if (lane > 2) return;
  SeqArpLaneRuntime& rt = s_seqArpRt[lane];
  if (silence) {
    midiSilenceLastChord(rt.channel0 & 0x0F);
  }
  rt.active = false;
  rt.orderCount = 0;
  rt.emitPos = 0;
}

static void seqArpStopAll(bool silence) {
  for (uint8_t lane = 0; lane < 3; ++lane) {
    seqArpStopLane(lane, silence);
  }
}

static uint8_t seqArpBuildOrder(uint8_t pattern, uint8_t noteCount, uint8_t pulses, uint8_t out[16]) {
  if (noteCount == 0 || pulses == 0) return 0;
  if (pulses > 16) pulses = 16;
  pattern &= 0x03U;
  for (uint8_t i = 0; i < pulses; ++i) {
    if (pattern == 0) {  // up
      out[i] = static_cast<uint8_t>(i % noteCount);
    } else if (pattern == 1) {  // down
      out[i] = static_cast<uint8_t>((noteCount - 1U) - (i % noteCount));
    } else if (pattern == 2) {  // up/down bounce
      if (noteCount <= 1) {
        out[i] = 0;
      } else {
        const uint8_t span = static_cast<uint8_t>((noteCount * 2U) - 2U);
        const uint8_t p = static_cast<uint8_t>(i % span);
        out[i] = (p < noteCount) ? p : static_cast<uint8_t>(span - p);
      }
    } else {  // random
      out[i] = static_cast<uint8_t>(rand() % noteCount);
    }
  }
  return pulses;
}

static void seqArpProcessDue(uint32_t nowMs) {
  for (uint8_t lane = 0; lane < 3; ++lane) {
    SeqArpLaneRuntime& rt = s_seqArpRt[lane];
    if (!rt.active) continue;
    while (rt.active && static_cast<int32_t>(nowMs - rt.nextMs) >= 0) {
      if (rt.emitPos >= rt.orderCount || static_cast<int32_t>(rt.nextMs - rt.endMs) >= 0) {
        rt.active = false;  // Truncate policy: never spill into next step.
        break;
      }
      const uint8_t idx = rt.orderIdx[rt.emitPos];
      if (idx >= rt.noteCount) {
        rt.active = false;
        break;
      }
      midiSilenceLastChord(rt.channel0 & 0x0F);
      midiSendNoteOnTracked(rt.channel0 & 0x0F, rt.notes[idx], applyOutputVelocityCurve(g_settings.outputVelocity));
      rt.emitPos++;
      rt.nextMs += rt.intervalMs;
    }
  }
}

static void transportEmitSequencerStepMidi(uint8_t step) {
  uint8_t selectedLane = g_seqLane;
  if (selectedLane > 2) selectedLane = 0;
  const uint32_t stepWindowMs = transportStepWindowMs();
  int8_t primaryLive = -1;
  for (uint8_t lane = 0; lane < 3; ++lane) {
    // End prior step scheduling for this lane before evaluating the new step.
    s_seqArpRt[lane].active = false;
    const uint8_t cell = g_seqPattern[lane][step & 0x0F];
    uint8_t midiCh = g_seqMidiCh[lane];
    if (midiCh < 1 || midiCh > 16) {
      midiCh = 1;
    }
    const uint8_t ch0 = static_cast<uint8_t>(midiCh - 1U);

    if (cell == kSeqTie) {
      // Tie policy: no new attack; keep previously sounding notes (including loop wrap).
      continue;
    }
    if (cell == kSeqRest) {
      midiSilenceLastChord(ch0);
      continue;
    }

    if (cell == kSeqSurprise || cell < ChordModel::kSurroundCount) {
      const uint8_t prob = g_seqExtras.stepProb[lane][step & 0x0F];
      if (!seqRollStepFires(prob)) {
        midiSilenceLastChord(ch0);
        continue;
      }
    }

    uint8_t vcap = g_chordVoicing;
    if (vcap < 1) vcap = 1;
    if (vcap > 4) vcap = 4;
    const uint8_t vel = applyOutputVelocityCurve(g_settings.outputVelocity);
    const bool stepArp = g_seqExtras.arpEnabled[lane][step & 0x0F] != 0U;

    if (stepArp && (cell == kSeqSurprise || cell < ChordModel::kSurroundCount)) {
      int8_t rel[6] = {};
      uint8_t n = 0;
      int rootMidi = 48;
      if (cell < ChordModel::kSurroundCount) {
        g_model.getSurroundSpell(static_cast<int>(cell), rel, &n);
        rootMidi = 48 + g_model.surroundChordRootPc(static_cast<int>(cell));
      } else {
        const int poolIdx = g_model.surprisePeekIndex();
        g_model.nextSurprise();
        g_model.getSurpriseSpell(poolIdx, rel, &n);
        rootMidi = 48 + g_model.surpriseChordRootPc(poolIdx);
      }
      int8_t ordered[6] = {};
      uint8_t oc = 0;
      arpeggiatorOrderedIntervals(g_settings.arpeggiatorMode, rel, n, vcap, ordered, &oc);
      if (oc == 0) {
        midiSilenceLastChord(ch0);
        continue;
      }
      SeqArpLaneRuntime& rt = s_seqArpRt[lane];
      rt.channel0 = ch0;
      rt.noteCount = oc;
      for (uint8_t i = 0; i < oc; ++i) {
        int note = rootMidi + static_cast<int>(ordered[i]) + static_cast<int>(g_settings.transposeSemitones);
        if (note < 0) note = 0;
        if (note > 127) note = 127;
        rt.notes[i] = static_cast<uint8_t>(note);
      }
      const uint8_t pulses = static_cast<uint8_t>(1U << (g_seqExtras.arpClockDiv[lane][step & 0x0F] & 0x03U));
      rt.orderCount =
          seqArpBuildOrder(g_seqExtras.arpPattern[lane][step & 0x0F], rt.noteCount, pulses, rt.orderIdx);
      if (rt.orderCount == 0) {
        midiSilenceLastChord(ch0);
        continue;
      }
      rt.emitPos = 0;
      rt.nextMs = millis();
      rt.endMs = rt.nextMs + (stepWindowMs == 0 ? 1U : stepWindowMs);
      rt.intervalMs = stepWindowMs / rt.orderCount;
      if (rt.intervalMs == 0) rt.intervalMs = 1;
      rt.active = true;
      if (primaryLive < 0 && lane == selectedLane && cell < ChordModel::kSurroundCount) {
        primaryLive = static_cast<int8_t>(cell);
      }
      continue;
    }

    if (cell < ChordModel::kSurroundCount) {
      midiPlaySurroundPad(g_model, static_cast<int>(cell), ch0, vel, vcap, g_settings.arpeggiatorMode,
                          g_settings.transposeSemitones);
      if (primaryLive < 0 && lane == selectedLane) {
        primaryLive = static_cast<int8_t>(cell);
      }
      continue;
    }

    if (cell == kSeqSurprise) {
      const int poolIdx = g_model.surprisePeekIndex();
      g_model.nextSurprise();
      midiPlaySurprisePad(g_model, poolIdx, ch0, vel, vcap, g_settings.arpeggiatorMode,
                          g_settings.transposeSemitones);
      continue;
    }
  }

  transportSetLiveChord(primaryLive);
}

static uint32_t s_transportTouchStartMs = 0;
static int s_transportTouchStartX = 0;
static int s_transportTouchStartY = 0;
static bool s_transportTouchDown = false;

static uint32_t s_tapTempoPrevTapMs = 0;
static uint16_t s_tapTempoPreviewBpm = 120;
Rect g_trTapTempoDone;

/// `s_transportDropKind`: -1 = none; else one of kTransportDrop*.
constexpr int8_t kTransportDropMidiOut = 0;
constexpr int8_t kTransportDropMidiIn = 1;
constexpr int8_t kTransportDropClock = 2;
constexpr int8_t kTransportDropStrum = 3;

static int8_t s_transportDropKind = -1;
static int s_transportDropScroll = 0;
static int s_transportDropDragLastY = -1;
/// Finger-drag steps that changed list scroll this touch (suppress row pick on lift).
static int s_transportDropFingerScrollCount = 0;

void layoutBottomBezels(int w, int h);
void beforeLeaveSequencer();
void drawPlaySurface(int fingerChord = -100, bool fingerOnKey = false);
void drawSequencerSurface(int fingerCell = -1);
void drawXyPadSurface();

void drawTransportSurface();
void processTransportTouch(uint8_t touchCount, int w, int h);
void drawTransportBpmEdit();
void processTransportBpmEditTouch(uint8_t touchCount, int w, int h);
void drawTransportTapTempo();
void processTransportTapTempoTouch(uint8_t touchCount, int w, int h);
static void transportDropDismiss();
static void transportDropOpen(int8_t kind, int w, int h);

// --- Brightness ---

void applyBrightness() {
  uint8_t v = (uint8_t)((uint16_t)g_settings.brightnessPercent * 255 / 100);
  M5.Display.setBrightness(v);
}

void resetSettingsNav() {
  g_settingsPanel = SettingsPanel::Menu;
  g_settingsCursorRow = 0;
  g_settingsListScroll = 0;
  s_settingsDropRowId = -1;
  s_settingsDropOptScroll = 0;
  g_factoryResetConfirmArmed = false;
  g_settingsConfirmAction = SettingsConfirmAction::None;
  g_settingsRow = 0;
  g_settingsFeedback[0] = '\0';
}

// =====================================================================
//  PLAY SURFACE
// =====================================================================

void drawRoundedButton(const Rect& r, uint16_t bg, const char* label,
                       uint8_t textSize = 2) {
  const int rad = max(4, min(r.w, r.h) / 8);
  M5.Display.fillRoundRect(r.x, r.y, r.w, r.h, rad, bg);
  M5.Display.drawRoundRect(r.x, r.y, r.w, r.h, rad,
                            (uint16_t)(bg == g_uiPalette.bg ? g_uiPalette.subtle : TFT_WHITE));
  M5.Display.setTextColor(TFT_WHITE, bg);
  M5.Display.setTextSize(textSize);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
}

void drawKeyCenterTwoLine(const Rect& r, const char* line1, const char* line2) {
  const int rad = max(4, min(r.w, r.h) / 8);
  M5.Display.fillRoundRect(r.x, r.y, r.w, r.h, rad, g_uiPalette.keyCenter);
  M5.Display.drawRoundRect(r.x, r.y, r.w, r.h, rad, TFT_WHITE);
  M5.Display.setFont(nullptr);
  M5.Display.setTextColor(TFT_WHITE, g_uiPalette.keyCenter);
  const int cx = r.x + r.w / 2;
  const int cy = r.y + r.h / 2;
  const uint8_t sz1 = (r.h >= 52) ? 2 : 1;
  const uint8_t sz2 = 1;
  M5.Display.setTextSize(sz1);
  M5.Display.setTextDatum(bottom_center);
  M5.Display.drawString(line1, cx, cy - 1);
  M5.Display.setTextSize(sz2);
  M5.Display.setTextDatum(top_center);
  M5.Display.drawString(line2, cx, cy + 1);
}

// Surround slots 0..7 clockwise from top-left; center (1,1) is the key.
static constexpr int kChordSlotRow[ChordModel::kSurroundCount] = {0, 0, 0, 1, 2, 2,
                                                                   2, 1};
static constexpr int kChordSlotCol[ChordModel::kSurroundCount] = {0, 1, 2, 2, 2, 1,
                                                                    0, 0};

void layoutBottomBezels(int w, int h) {
  constexpr int deadZone = 12;
  const int bezelY = h - kBezelBarH;
  constexpr int kTouchOvershoot = 60;
  const int touchH = kBezelBarH + kTouchOvershoot;
  const int zone = w / 3;
  g_bezelBack = {0, bezelY, zone - deadZone / 2, touchH};
  g_bezelSelect = {zone + deadZone / 2, bezelY, zone - deadZone, touchH};
  g_bezelFwd = {2 * zone + deadZone / 2, bezelY, zone - deadZone / 2, touchH};
}

void drawBezelBarStrip() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.fillRect(0, h - kBezelBarH, w, kBezelBarH, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(bottom_center);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("BACK", g_bezelBack.x + g_bezelBack.w / 2, h - 2);
  M5.Display.drawString(s_shiftActive ? "SHIFT" : "SELECT", g_bezelSelect.x + g_bezelSelect.w / 2, h - 2);
  M5.Display.drawString("FWD", g_bezelFwd.x + g_bezelFwd.w / 2, h - 2);
}

static void updateShiftHoldState(uint8_t touchCount) {
  bool selectDown = false;
  for (uint8_t i = 0; i < touchCount; ++i) {
    const auto& d = M5.Touch.getDetail(i);
    if (!d.isPressed()) continue;
    if (pointInRect(d.x, d.y, g_bezelSelect)) {
      selectDown = true;
      break;
    }
  }
  if (selectDown) {
    if (!s_shiftSelectDown) {
      s_shiftSelectDown = true;
      s_shiftTriggeredThisHold = false;
      s_shiftSelectDownMs = millis();
    } else if (!s_shiftActive && (millis() - s_shiftSelectDownMs >= kShiftHoldMs)) {
      s_shiftActive = true;
      s_shiftTriggeredThisHold = true;
      s_seqSelectHeld = true;
    }
  } else {
    if (s_shiftActive) {
      s_shiftActive = false;
      s_seqSelectHeld = false;
    }
    s_shiftSelectDown = false;
    s_shiftSelectDownMs = 0;
  }
}

static void shiftSeqAdjustFocusedParam(int delta) {
  if (!s_shiftActive) return;
  if (s_shiftSeqFocusStep < 0 || s_shiftSeqFocusStep > 15) return;
  uint8_t L = g_seqLane;
  if (L > 2) L = 0;
  const uint8_t step = static_cast<uint8_t>(s_shiftSeqFocusStep);
  switch (s_shiftSeqFocusField & 0x03U) {
    case 0: {
      int v = static_cast<int>(g_seqExtras.stepClockDiv[L][step]) + delta;
      while (v < 0) v += 4;
      v %= 4;
      g_seqExtras.stepClockDiv[L][step] = static_cast<uint8_t>(v);
      break;
    }
    case 1:
      g_seqExtras.arpEnabled[L][step] = static_cast<uint8_t>(g_seqExtras.arpEnabled[L][step] ? 0U : 1U);
      break;
    case 2: {
      int v = static_cast<int>(g_seqExtras.arpPattern[L][step]) + delta;
      while (v < 0) v += 4;
      v %= 4;
      g_seqExtras.arpPattern[L][step] = static_cast<uint8_t>(v);
      break;
    }
    case 3: {
      int v = static_cast<int>(g_seqExtras.arpClockDiv[L][step]) + delta;
      while (v < 0) v += 4;
      v %= 4;
      g_seqExtras.arpClockDiv[L][step] = static_cast<uint8_t>(v);
      break;
    }
  }
}

static bool shiftSeqHandleBackFwdNudgeHold(int direction) {
  if (!s_shiftActive) return false;
  if (direction != -1 && direction != 1) return false;
  if (s_shiftSeqFocusStep < 0 || s_shiftSeqFocusStep > 15) return false;
  const uint32_t now = millis();
  if (s_shiftSeqNudgeDir != direction) {
    s_shiftSeqNudgeDir = static_cast<int8_t>(direction);
    s_shiftSeqNudgeStartMs = now;
    s_shiftSeqNudgeLastMs = 0;
    s_shiftSeqNudgeConsumed = false;
    return false;
  }
  if (now - s_shiftSeqNudgeStartMs < kShiftSeqNudgeHoldMs) {
    return false;
  }
  const uint32_t held = now - s_shiftSeqNudgeStartMs;
  uint32_t interval = 180U;
  if (held > 1600U) interval = 70U;
  else if (held > 1100U) interval = 95U;
  else if (held > 700U) interval = 130U;
  if (s_shiftSeqNudgeLastMs != 0 && (now - s_shiftSeqNudgeLastMs) < interval) {
    return true;
  }
  s_shiftSeqNudgeConsumed = true;
  s_shiftSeqNudgeLastMs = now;
  shiftSeqAdjustFocusedParam(direction);
  drawSequencerSurface();
  return true;
}

static void shiftSeqResetBackFwdNudgeHold() {
  s_shiftSeqNudgeDir = 0;
  s_shiftSeqNudgeStartMs = 0;
  s_shiftSeqNudgeLastMs = 0;
}

static void drawHoldProgressStrip(HoldProgressOwner owner, uint32_t elapsedMs, uint32_t targetMs,
                                  const char* label) {
  if (targetMs == 0) return;
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  const int x = 6;
  const int y = h - kBezelBarH + 2;
  const int bw = max(20, w - 12);
  const int bh = 6;
  uint32_t clamped = elapsedMs;
  if (clamped > targetMs) clamped = targetMs;
  const int fillW = static_cast<int>((static_cast<uint64_t>(bw) * clamped) / targetMs);
  M5.Display.fillRoundRect(x, y, bw, bh, 2, g_uiPalette.panelMuted);
  if (fillW > 0) {
    M5.Display.fillRoundRect(x, y, fillW, bh, 2, g_uiPalette.settingsBtnActive);
  }
  M5.Display.drawRoundRect(x, y, bw, bh, 2, g_uiPalette.subtle);
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString(label, 8, y + bh + 1);
  s_holdProgressOwner = owner;
}

static void clearHoldProgressStrip(HoldProgressOwner owner) {
  if (s_holdProgressOwner != owner) return;
  drawBezelBarStrip();
  s_holdProgressOwner = HoldProgressOwner::None;
}

bool checkBezelLongPressSettings(uint8_t touchCount) {
  if (g_screen == Screen::Play || g_screen == Screen::PlayCategory) {
    s_bezelLongPressMs = 0;
    s_bezelLongPressFired = false;
    clearHoldProgressStrip(HoldProgressOwner::SettingsSingle);
    return false;
  }
  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    if (d.isPressed() && pointInRect(d.x, d.y, g_bezelFwd)) {
      if (s_bezelLongPressMs == 0) s_bezelLongPressMs = millis();
      const uint32_t elapsed = millis() - s_bezelLongPressMs;
      drawHoldProgressStrip(HoldProgressOwner::SettingsSingle, elapsed, settingsEntryHoldMs(),
                            "Hold FWD: settings");
      if (!s_bezelLongPressFired && millis() - s_bezelLongPressMs >= settingsEntryHoldMs()) {
        s_bezelLongPressFired = true;
        clearHoldProgressStrip(HoldProgressOwner::SettingsSingle);
        suppressNextPlayTap = true;
        panicForTrigger(PanicTrigger::EnterSettings);
        g_screen = Screen::Settings;
        resetSettingsNav();
        return true;
      }
      return false;
    }
  }
  s_bezelLongPressMs = 0;
  s_bezelLongPressFired = false;
  clearHoldProgressStrip(HoldProgressOwner::SettingsSingle);
  return false;
}

void computePlaySurfaceLayout(int w, int h) {
  layoutBottomBezels(w, h);
  constexpr int hintH = 14;
  constexpr int padAfterHint = 2;
  constexpr int hGap = 4;
  constexpr int vGap = 2;
  const int gridTop = hintH + padAfterHint;
  const int gridBottom = h - kBezelBarH;
  const int availH = gridBottom - gridTop - 2 * vGap;
  const int availW = w - 4 - 2 * hGap;
  const int cellH = availH / 3;
  const int cellW = availW / 3;
  const int cell = min(cellW, cellH);
  const int totalW = cell * 3 + hGap * 2;
  const int totalH = cell * 3 + vGap * 2;
  const int ox = (w - totalW) / 2;
  const int oy = gridBottom - totalH;

  g_keyRect = {ox + (cell + hGap), oy + (cell + vGap), cell, cell};
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    const int row = kChordSlotRow[i];
    const int col = kChordSlotCol[i];
    g_chordRects[i] = {ox + col * (cell + hGap), oy + row * (cell + vGap), cell, cell};
  }
}

static Rect playVoicingTrackRect() {
  const Rect& p = g_playVoicingPanel;
  const int trackTop = p.y + 14;
  const int trackBottom = p.y + p.h - 4;
  const int trackH = max(10, trackBottom - trackTop);
  return {p.x + 8, trackTop, max(8, p.w - 50), trackH};
}

static void playSetVoicingFromX(int px) {
  const Rect tr = playVoicingTrackRect();
  const int rw = max(1, tr.w);
  int rel = px - tr.x;
  if (rel < 0) rel = 0;
  if (rel > rw) rel = rw;
  uint8_t v = static_cast<uint8_t>(1 + (static_cast<int64_t>(rel) * 3) / rw);
  if (v < 1) v = 1;
  if (v > 4) v = 4;
  g_chordVoicing = v;
}

void computeSequencerLayout(int w, int h) {
  layoutBottomBezels(w, h);
  constexpr int margin = 4;
  constexpr int hintH = 14;
  constexpr int tabH = 20;
  constexpr int tabGap = 4;
  const int tabY = hintH + 2;
  const int tabW = (w - margin * 2 - tabGap * 2) / 3;
  for (int t = 0; t < 3; ++t) {
    g_seqTabRects[t] = {margin + t * (tabW + tabGap), tabY, tabW, tabH};
  }
  g_seqSliderActive = (g_seqTool != SeqTool::None);
  const int gridTop = tabY + tabH + 2;
  int gridBottom = h - kBezelBarH - 2;
  if (g_seqSliderActive) {
    constexpr int panelH = 52;
    const int panelY = gridBottom - panelH;
    g_seqSliderPanel = {margin, panelY, w - 2 * margin, panelH};
    gridBottom = panelY - 2;
  }
  const int availH = gridBottom - gridTop;
  constexpr int hGap = 4;
  constexpr int vGap = 2;
  const int cellW = max(22, (w - 2 * margin - 3 * hGap) / 4);
  const int cellH = max(22, (availH - 3 * vGap) / 4);
  const int totalW = cellW * 4 + hGap * 3;
  const int ox = (w - totalW) / 2;
  const int oy = gridTop;
  for (int i = 0; i < 16; ++i) {
    const int row = i / 4;
    const int col = i % 4;
    g_seqCellRects[i] = {ox + col * (cellW + hGap), oy + row * (cellH + vGap), cellW, cellH};
  }
}

int hitTestSeqTab(int px, int py) {
  for (int t = 0; t < 3; ++t) {
    if (pointInRect(px, py, g_seqTabRects[t])) return t;
  }
  return -1;
}

uint8_t seqLaneClamped() { return g_seqLane > 2 ? 0 : g_seqLane; }

static void shiftSeqFocusSetForCurrentLane(int8_t step) {
  uint8_t L = seqLaneClamped();
  if (step < 0 || step > 15) {
    step = -1;
  }
  s_shiftSeqFocusStep = step;
  s_shiftSeqFocusStepByLane[L] = step;
}

static void shiftSeqFocusSyncFromCurrentLane() {
  const uint8_t L = seqLaneClamped();
  int8_t step = s_shiftSeqFocusStepByLane[L];
  if (step < 0 || step > 15) {
    step = -1;
  }
  s_shiftSeqFocusStep = step;
}

Rect seqSliderTrackRect() {
  const Rect& p = g_seqSliderPanel;
  const bool sub = (g_seqTool == SeqTool::StepProb);
  const int trackTop = sub ? (p.y + 23) : (p.y + 13);
  const int trackBottom = p.y + p.h - 3;
  const int trackH = max(10, trackBottom - trackTop);
  return {p.x + 6, trackTop, max(8, p.w - 44), trackH};
}

void seqSetSliderValueFromX(int px) {
  const Rect tr = seqSliderTrackRect();
  int rw = max(1, tr.w);
  int rel = px - tr.x;
  if (rel < 0) rel = 0;
  if (rel > rw) rel = rw;
  const uint8_t v = static_cast<uint8_t>((int64_t)rel * 100 / rw);
  const uint8_t L = seqLaneClamped();
  switch (g_seqTool) {
    case SeqTool::Quantize:
      g_seqExtras.quantizePct[L] = v;
      break;
    case SeqTool::Swing:
      g_seqExtras.swingPct[L] = v;
      break;
    case SeqTool::ChordRand:
      g_seqExtras.chordRandPct[L] = v;
      break;
    case SeqTool::StepProb:
      if (g_seqProbFocusStep < 0) {
        for (int i = 0; i < 16; ++i) {
          g_seqExtras.stepProb[L][i] = v;
        }
      } else {
        g_seqExtras.stepProb[L][g_seqProbFocusStep] = v;
      }
      break;
    default:
      break;
  }
}

uint8_t seqSliderDisplayedValue() {
  const uint8_t L = seqLaneClamped();
  switch (g_seqTool) {
    case SeqTool::Quantize:
      return g_seqExtras.quantizePct[L];
    case SeqTool::Swing:
      return g_seqExtras.swingPct[L];
    case SeqTool::ChordRand:
      return g_seqExtras.chordRandPct[L];
    case SeqTool::StepProb:
      if (g_seqProbFocusStep >= 0 && g_seqProbFocusStep < 16) {
        return g_seqExtras.stepProb[L][g_seqProbFocusStep];
      }
      return g_seqExtras.stepProb[L][0];
    default:
      return 0;
  }
}

void drawSelectionEdge(const Rect& r) {
  const int rad = max(4, min(r.w, r.h) / 8);
  M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, rad + 2, g_uiPalette.highlightRing);
  M5.Display.drawRoundRect(r.x - 3, r.y - 3, r.w + 6, r.h + 6, rad + 3, g_uiPalette.highlightRing);
}

// Last-played ring on the tonic: use white so it never reads as SELECT latch (pink/magenta).
void drawLastPlayedKeyEdge(const Rect& r) {
  const int rad = max(4, min(r.w, r.h) / 8);
  M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, rad + 2, TFT_WHITE);
  M5.Display.drawRoundRect(r.x - 3, r.y - 3, r.w + 6, r.h + 6, rad + 3, TFT_WHITE);
}

static void drawPlayKeyCell(bool fingerOnKey, bool showLastPlayedOutline) {
  const int cell = g_keyRect.w;
  if (g_model.heartAvailable) {
    const int rad = max(4, min(g_keyRect.w, g_keyRect.h) / 8);
    M5.Display.fillRoundRect(g_keyRect.x, g_keyRect.y, g_keyRect.w, g_keyRect.h, rad,
                             g_uiPalette.heart);
    M5.Display.drawRoundRect(g_keyRect.x, g_keyRect.y, g_keyRect.w, g_keyRect.h, rad, TFT_WHITE);
    char modeLine[20];
    g_model.formatKeyCenterLine2(modeLine, sizeof(modeLine));
    M5.Display.setFont(nullptr);
    M5.Display.setTextColor(TFT_WHITE, g_uiPalette.heart);
    const int cx = g_keyRect.x + g_keyRect.w / 2;
    const int cy = g_keyRect.y + g_keyRect.h / 2;
    const uint8_t heartSz = (cell >= 56) ? 3 : 2;
    M5.Display.setTextSize(heartSz);
    M5.Display.setTextDatum(bottom_center);
    M5.Display.drawString("\x03", cx, cy - 1);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_center);
    char line2[28];
    snprintf(line2, sizeof(line2), "%s  %s", g_model.keyName(), modeLine);
    M5.Display.drawString(line2, cx, cy + 1);
  } else if (fingerOnKey) {
    const int rad = max(4, cell / 8);
    M5.Display.fillRoundRect(g_keyRect.x, g_keyRect.y, g_keyRect.w, g_keyRect.h, rad,
                             g_uiPalette.accentPress);
    char modeLine[20];
    g_model.formatKeyCenterLine2(modeLine, sizeof(modeLine));
    M5.Display.setFont(nullptr);
    M5.Display.setTextColor(TFT_WHITE, g_uiPalette.accentPress);
    const int cx = g_keyRect.x + g_keyRect.w / 2;
    const int cy = g_keyRect.y + g_keyRect.h / 2;
    M5.Display.setTextSize((cell >= 52) ? 2 : 1);
    M5.Display.setTextDatum(bottom_center);
    M5.Display.drawString(g_model.keyName(), cx, cy - 1);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_center);
    M5.Display.drawString(modeLine, cx, cy + 1);
  } else if (s_playSelectLatched) {
    constexpr uint16_t kKeyEditYellow = 0xFFE0;
    constexpr uint16_t kKeyEditPink = 0xF81F;
    const int rad = max(4, cell / 8);
    M5.Display.fillRoundRect(g_keyRect.x, g_keyRect.y, g_keyRect.w, g_keyRect.h, rad,
                             kKeyEditYellow);
    M5.Display.drawRoundRect(g_keyRect.x - 1, g_keyRect.y - 1, g_keyRect.w + 2, g_keyRect.h + 2,
                             rad + 1, kKeyEditPink);
    M5.Display.drawRoundRect(g_keyRect.x - 2, g_keyRect.y - 2, g_keyRect.w + 4, g_keyRect.h + 4,
                             rad + 2, kKeyEditPink);
    char modeLine[20];
    g_model.formatKeyCenterLine2(modeLine, sizeof(modeLine));
    M5.Display.setFont(nullptr);
    M5.Display.setTextColor(TFT_BLACK, kKeyEditYellow);
    const int cx = g_keyRect.x + g_keyRect.w / 2;
    const int cy = g_keyRect.y + g_keyRect.h / 2;
    M5.Display.setTextSize((cell >= 52) ? 2 : 1);
    M5.Display.setTextDatum(bottom_center);
    M5.Display.drawString(g_model.keyName(), cx, cy - 1);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_center);
    M5.Display.drawString(modeLine, cx, cy + 1);
  } else {
    char modeLine[20];
    g_model.formatKeyCenterLine2(modeLine, sizeof(modeLine));
    drawKeyCenterTwoLine(g_keyRect, g_model.keyName(), modeLine);
  }
  if (showLastPlayedOutline && g_lastPlayedOutline == -2 && !s_playSelectLatched) {
    drawLastPlayedKeyEdge(g_keyRect);
  }
}

static void drawPlayChordCell(int i, int fingerChord, bool showLastPlayedOutline) {
  const int cell = g_keyRect.w;
  const uint8_t chordText = (cell >= 52) ? 2 : 1;
  if (s_shiftActive) {
    char lab[12];
    if (i < 4) {
      snprintf(lab, sizeof(lab), "CC%u", static_cast<unsigned>(kShiftPadCcMap[i]));
    } else {
      snprintf(lab, sizeof(lab), "Pgm%u", static_cast<unsigned>(kShiftPadProgramMap[i - 4] + 1U));
    }
    uint16_t bg = g_uiPalette.seqLaneTab;
    if (fingerChord >= 0 && i == fingerChord) {
      bg = g_uiPalette.accentPress;
    }
    drawRoundedButton(g_chordRects[i], bg, lab, 1);
    return;
  }
  if (s_playSelectLatched) {
    if (i == 0) {
      char lv[12];
      snprintf(lv, sizeof(lv), "V%u", (unsigned)g_chordVoicing);
      drawRoundedButton(g_chordRects[0], g_uiPalette.seqLaneTab, lv, chordText);
      return;
    }
    if (i == 2) {
      drawRoundedButton(g_chordRects[2], g_uiPalette.seqLaneTab, "+", (cell >= 52) ? 3 : 2);
      return;
    }
    if (i == 6) {
      drawRoundedButton(g_chordRects[6], g_uiPalette.seqLaneTab, "-", (cell >= 52) ? 3 : 2);
      return;
    }
  }
  if (s_drawPlayVoicingShift && i == 0) {
    char lv[12];
    snprintf(lv, sizeof(lv), "V%u", (unsigned)g_chordVoicing);
    drawRoundedButton(g_chordRects[0], g_uiPalette.accentPress, lv, chordText);
    return;
  }
  uint16_t bg = colorForRole(g_model.surround[i].role);
  if (fingerChord >= 0 && i == fingerChord) {
    bg = g_uiPalette.accentPress;
  }
  drawRoundedButton(g_chordRects[i], bg, g_model.surround[i].name, chordText);
  if (showLastPlayedOutline && fingerChord < -50 && g_lastPlayedOutline == i) {
    drawSelectionEdge(g_chordRects[i]);
  }
}

static void layoutPlayVoicingPanel(int w, int h) {
  constexpr int panelH = 52;
  const int panelY = h - kBezelBarH - panelH - 2;
  g_playVoicingPanel = {8, panelY, w - 16, panelH};
}

static void drawPlayVoicingPanelOverlay() {
  const Rect& p = g_playVoicingPanel;
  M5.Display.fillRoundRect(p.x, p.y, p.w, p.h, 6, g_uiPalette.panelMuted);
  M5.Display.drawRoundRect(p.x, p.y, p.w, p.h, 6, g_uiPalette.settingsBtnBorder);
  M5.Display.setFont(nullptr);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.panelMuted);
  M5.Display.drawString("Chord voicing", p.x + p.w / 2, p.y + 4);
  const Rect tr = playVoicingTrackRect();
  M5.Display.fillRoundRect(tr.x, tr.y, tr.w, tr.h, 4, g_uiPalette.bg);
  M5.Display.drawRoundRect(tr.x, tr.y, tr.w, tr.h, 4, g_uiPalette.hintText);
  const int seg = max(1, tr.w / 4);
  const uint8_t vi =
      (g_chordVoicing >= 1 && g_chordVoicing <= 4) ? static_cast<uint8_t>(g_chordVoicing - 1) : 3;
  const int tx = tr.x + static_cast<int>(vi) * seg + seg / 2;
  M5.Display.fillCircle(tx, tr.y + tr.h / 2, 6, g_uiPalette.seqLaneTab);
  M5.Display.drawCircle(tx, tr.y + tr.h / 2, 6, TFT_WHITE);
  char vlabel[8];
  snprintf(vlabel, sizeof(vlabel), "V%u", (unsigned)g_chordVoicing);
  M5.Display.setTextDatum(middle_right);
  M5.Display.setTextColor(TFT_WHITE, g_uiPalette.panelMuted);
  M5.Display.drawString(vlabel, p.x + p.w - 6, tr.y + tr.h / 2);
}

// Union of key + chord pads, padded so clearing covers drawSelectionEdge / drawLastPlayedKeyEdge
// spill (rings extend a few pixels outside each rect).
static Rect playSurfaceGridBoundsPadded(int pad) {
  int x0 = g_keyRect.x;
  int y0 = g_keyRect.y;
  int x1 = g_keyRect.x + g_keyRect.w;
  int y1 = g_keyRect.y + g_keyRect.h;
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    const Rect& c = g_chordRects[i];
    x0 = min(x0, c.x);
    y0 = min(y0, c.y);
    x1 = max(x1, c.x + c.w);
    y1 = max(y1, c.y + c.h);
  }
  return {x0 - pad, y0 - pad, (x1 - x0) + 2 * pad, (y1 - y0) + 2 * pad};
}

// Redraw only the play grid (not the hint strip): clears selection-ring spill, then paints all pads.
static void playRedrawGridBand() {
  constexpr int kRingSpillPad = 4;
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  computePlaySurfaceLayout(w, h);
  Rect band = playSurfaceGridBoundsPadded(kRingSpillPad);
  const int maxY = h - kBezelBarH;
  if (band.x < 0) {
    band.w += band.x;
    band.x = 0;
  }
  if (band.y < 0) {
    band.h += band.y;
    band.y = 0;
  }
  if (band.x + band.w > w) {
    band.w = w - band.x;
  }
  if (band.y + band.h > maxY) {
    band.h = maxY - band.y;
  }
  if (band.w > 0 && band.h > 0) {
    M5.Display.fillRect(band.x, band.y, band.w, band.h, g_uiPalette.bg);
  }
  drawPlayKeyCell(false, true);
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    drawPlayChordCell(i, -100, true);
  }
  if (s_playVoicingPanelOpen) {
    layoutPlayVoicingPanel(w, h);
    drawPlayVoicingPanelOverlay();
  }
}

static void playRedrawAfterOutlineChange() {
  if (transportIsCountIn() || transportIsRecordingLive()) {
    drawPlaySurface();
    return;
  }
  M5.Display.startWrite();
  playRedrawGridBand();
  drawBezelBarStrip();
  M5.Display.endWrite();
}

static void playRedrawClearFingerHighlight() {
  if (transportIsCountIn() || transportIsRecordingLive()) {
    drawPlaySurface();
    return;
  }
  M5.Display.startWrite();
  playRedrawGridBand();
  drawBezelBarStrip();
  M5.Display.endWrite();
}

// fingerChord: -100 = none, -2 = key, 0-7 = chord index (finger-down highlight)
void drawPlaySurface(int fingerChord, bool fingerOnKey) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  computePlaySurfaceLayout(w, h);
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);

  M5.Display.setFont(nullptr);
  M5.Display.setTextWrap(false);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("BACK/FWD: Transport / Pad / Seq / XY   SELECT tap:key  hold:Shift", 4, 2);

  const bool showLp = fingerChord < -50;
  drawPlayKeyCell(fingerOnKey, showLp);
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    drawPlayChordCell(i, fingerChord, showLp);
  }

  if (s_playVoicingPanelOpen) {
    layoutPlayVoicingPanel(w, h);
    drawPlayVoicingPanelOverlay();
  }

  if (transportIsCountIn()) {
    char cn[8];
    snprintf(cn, sizeof(cn), "%u", (unsigned)transportCountInNumber());
    M5.Display.setFont(nullptr);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(4);
    M5.Display.setTextColor(g_uiPalette.accentPress, g_uiPalette.bg);
    M5.Display.drawString(cn, w / 2, h / 2 - 24);
  }
  if (transportIsRecordingLive()) {
    M5.Display.setFont(nullptr);
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(top_right);
    M5.Display.setTextColor(g_uiPalette.danger, g_uiPalette.bg);
    M5.Display.drawString("REC", w - 8, 28);
  }
  if (s_midiDetectedChord[0] != '\0' && millis() < s_midiDetectedChordUntilMs) {
    char lab[20];
    snprintf(lab, sizeof(lab), "IN %s", s_midiDetectedChord);
    M5.Display.setFont(nullptr);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
    M5.Display.drawString(lab, 4, 14);
  }
  if (s_midiDetectedSuggest[0] != '\0' && millis() < s_midiDetectedSuggestUntilMs) {
    char sug[20];
    snprintf(sug, sizeof(sug), "SUG %s", s_midiDetectedSuggest);
    M5.Display.setFont(nullptr);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
    M5.Display.drawString(sug, 4, 24);
  }
  if (g_settings.playInMonitor && s_playIngressInfo[0] != '\0' && millis() < s_playIngressInfoUntilMs) {
    const uint32_t nowMs = millis();
    uint16_t monitorColor = g_uiPalette.subtle;
    if (g_settings.playInMonitorMode != 0) {
      const uint32_t ageMs =
          (nowMs >= s_playIngressInfoEventMs) ? (nowMs - s_playIngressInfoEventMs) : 0U;
      if (ageMs < 300U) {
        monitorColor = g_uiPalette.accentPress;
      } else if (ageMs < 1200U) {
        monitorColor = g_uiPalette.rowNormal;
      } else {
        monitorColor = g_uiPalette.subtle;
      }
    }
    M5.Display.setFont(nullptr);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(monitorColor, g_uiPalette.bg);
    M5.Display.drawString(s_playIngressInfo, 4, 34);
  }

  // Discrete external-clock BPM readout in top-right (spec Phase 2 UX).
  const bool extClockSelected = (g_settings.midiClockSource != 0) && (g_settings.clkFollow != 0);
  const bool extClockActive = transportExternalClockActive();
  if (extClockSelected) {
    char bpmCorner[8];
    const uint16_t extBpm = transportExternalClockBpm();
    if (extClockActive && extBpm >= 40 && extBpm <= 300) {
      snprintf(bpmCorner, sizeof(bpmCorner), "%u", static_cast<unsigned>(extBpm));
    } else {
      snprintf(bpmCorner, sizeof(bpmCorner), "--");
    }
    const uint32_t nowMs = millis();
    const bool flashOn = nowMs < s_playClockFlashUntilMs;
    M5.Display.setFont(nullptr);
    M5.Display.setTextDatum(top_right);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(flashOn ? g_uiPalette.rowNormal : g_uiPalette.subtle, g_uiPalette.bg);
    M5.Display.drawString(bpmCorner, w - 6, 4);
  }

  drawBezelBarStrip();
  M5.Display.endWrite();
}

int hitTestPlay(int px, int py) {
  if (pointInRectPad(px, py, g_keyRect, kPlayHitPadPx)) return -2;  // key center
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    if (pointInRectPad(px, py, g_chordRects[i], kPlayHitPadPx)) return i;
  }
  return -1;  // nothing
}

static const char* playCategoryTitle(PlayCategoryPage p) {
  switch (p) {
    case PlayCategoryPage::History:
      return "History";
    case PlayCategoryPage::Diatonic:
      return "Diatonic";
    case PlayCategoryPage::Functional:
      return "Functional";
    case PlayCategoryPage::Related:
      return "Related";
    case PlayCategoryPage::Chromatic:
      return "Chromatic";
    default:
      return "Category";
  }
}

static void playHistoryRecord(uint8_t idx) {
  if (idx >= ChordModel::kSurroundCount) return;
  s_playChordHistory[s_playChordHistoryHead] = idx;
  s_playChordHistoryHead = static_cast<uint8_t>((s_playChordHistoryHead + 1U) % 16U);
  if (s_playChordHistoryCount < 16U) {
    ++s_playChordHistoryCount;
  }
}

static void playCategoryBuildItems() {
  s_playCategoryCount = 0;
  auto addUnique = [](uint8_t idx) {
    if (idx >= ChordModel::kSurroundCount) return;
    for (uint8_t i = 0; i < s_playCategoryCount; ++i) {
      if (s_playCategoryItems[i] == idx) return;
    }
    if (s_playCategoryCount < ChordModel::kSurroundCount) {
      s_playCategoryItems[s_playCategoryCount++] = idx;
    }
  };

  switch (s_playCategoryPage) {
    case PlayCategoryPage::History:
      for (uint8_t i = 0; i < s_playChordHistoryCount; ++i) {
        const uint8_t histPos =
            static_cast<uint8_t>((s_playChordHistoryHead + 16U - 1U - i) % 16U);
        addUnique(s_playChordHistory[histPos]);
      }
      break;
    case PlayCategoryPage::Diatonic:
      for (uint8_t i = 0; i < ChordModel::kSurroundCount; ++i) {
        const ChordRole role = g_model.surround[i].role;
        if (role == ChordRole::Principal || role == ChordRole::Standard) addUnique(i);
      }
      break;
    case PlayCategoryPage::Functional:
      for (uint8_t i = 0; i < ChordModel::kSurroundCount; ++i) {
        if (g_model.surround[i].role == ChordRole::Principal) addUnique(i);
      }
      for (uint8_t i = 0; i < ChordModel::kSurroundCount; ++i) {
        if (g_model.surround[i].role == ChordRole::Tension) addUnique(i);
      }
      break;
    case PlayCategoryPage::Related:
      for (uint8_t i = 0; i < ChordModel::kSurroundCount; ++i) {
        if (g_model.surround[i].role == ChordRole::Standard) addUnique(i);
      }
      for (uint8_t i = 0; i < ChordModel::kSurroundCount; ++i) {
        if (g_model.surround[i].role == ChordRole::Surprise) addUnique(i);
      }
      break;
    case PlayCategoryPage::Chromatic:
      for (uint8_t i = 0; i < ChordModel::kSurroundCount; ++i) addUnique(i);
      break;
    default:
      break;
  }

  if (s_playCategoryCount == 0) {
    for (uint8_t i = 0; i < ChordModel::kSurroundCount; ++i) addUnique(i);
  }
}

static void playCategoryStep(int direction) {
  const int count = static_cast<int>(PlayCategoryPage::Count);
  int idx = static_cast<int>(s_playCategoryPage);
  idx = (idx + direction + count) % count;
  s_playCategoryPage = static_cast<PlayCategoryPage>(idx);
  playCategoryBuildItems();
}

static bool playHandleBezelFastCycle(int direction) {
  if (direction != -1 && direction != 1) return false;
  const uint32_t now = millis();
  if (s_playBezelHoldDir != direction) {
    s_playBezelHoldDir = static_cast<int8_t>(direction);
    s_playBezelHoldStartMs = now;
    s_playBezelRepeatMs = 0;
    s_playBezelLongPressConsumed = false;
    return false;
  }
  if (now - s_playBezelHoldStartMs < kPlayBezelCycleHoldMs) {
    return false;
  }
  if (s_playBezelRepeatMs != 0 && (now - s_playBezelRepeatMs) < kPlayBezelCycleRepeatMs) {
    return true;
  }
  s_playBezelLongPressConsumed = true;
  s_playBezelRepeatMs = now;
  if (g_screen == Screen::PlayCategory) {
    playCategoryStep(direction);
    drawPlayCategorySurface();
  } else {
    navigateMainRing(direction);
  }
  return true;
}

static void playResetBezelFastCycle() {
  s_playBezelHoldStartMs = 0;
  s_playBezelRepeatMs = 0;
  s_playBezelHoldDir = 0;
}

static void playTriggerSurroundByIdx(int hit) {
  if (hit < 0 || hit >= ChordModel::kSurroundCount) return;
  const uint8_t mch = static_cast<uint8_t>(g_settings.midiOutChannel - 1);
  const uint8_t vel = applyOutputVelocityCurve(g_settings.outputVelocity);
  uint8_t vcap = g_chordVoicing;
  if (vcap < 1) vcap = 1;
  if (vcap > 4) vcap = 4;
  midiPlaySurroundPad(g_model, hit, mch, vel, vcap, g_settings.arpeggiatorMode, g_settings.transposeSemitones);
  g_lastPlayedOutline = hit;
  g_model.registerPlay();
  playHistoryRecord(static_cast<uint8_t>(hit));
  transportSetLiveChord(static_cast<int8_t>(hit));
}

void drawPlayCategorySurface(int fingerCell) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutBottomBezels(w, h);
  playCategoryBuildItems();
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);

  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("Chord categories", 4, 2);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString(playCategoryTitle(s_playCategoryPage), 4, 14);

  constexpr int pad = 6;
  const int gridTop = 32;
  const int cols = 4;
  const int rows = 2;
  const int cellW = (w - ((cols + 1) * pad)) / cols;
  const int cellH = (h - kBezelBarH - gridTop - ((rows + 1) * pad));
  const int rowH = cellH / rows;
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    const int col = i % cols;
    const int row = i / cols;
    const int x = pad + col * (cellW + pad);
    const int y = gridTop + pad + row * (rowH + pad);
    g_playCategoryCells[i] = {x, y, cellW, rowH};
    const bool active = (i < s_playCategoryCount);
    uint16_t bg = g_uiPalette.panelMuted;
    const char* lab = "-";
    if (active) {
      const int idx = static_cast<int>(s_playCategoryItems[i]);
      const ChordRole role = g_model.surround[idx].role;
      bg = colorForRole(role);
      lab = g_model.surround[idx].name;
    }
    if (fingerCell == i) {
      bg = g_uiPalette.accentPress;
    }
    drawRoundedButton(g_playCategoryCells[i], bg, lab, 1);
  }
  drawBezelBarStrip();
  M5.Display.endWrite();
}

int hitTestPlayCategoryCell(int px, int py) {
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    if (pointInRectPad(px, py, g_playCategoryCells[i], kPlayCategoryHitPadPx)) return i;
  }
  return -1;
}

int hitTestSeq(int px, int py) {
  for (int i = 0; i < 16; ++i) {
    if (pointInRectPad(px, py, g_seqCellRects[i], kSeqHitPadPx)) return i;
  }
  return -1;
}

void seqCycleStep(int idx) {
  if (idx < 0 || idx >= 16) return;
  uint8_t& cell = g_seqPattern[g_seqLane][idx];
  uint8_t v = cell;
  if (v == kSeqRest) {
    cell = kSeqTie;
  } else if (v == kSeqTie) {
    cell = 0;
  } else if (v < 7) {
    cell = static_cast<uint8_t>(v + 1);
  } else {
    cell = kSeqRest;
  }
}

const char* seqStepLabel(uint8_t v) {
  if (v == kSeqRest) return "-";
  if (v == kSeqTie) return "~";
  if (v == kSeqSurprise) return "S?";
  if (v <= 7) return g_model.surround[v].name;
  return "?";
}

static int seqChordDropItemCount() {
  return 11;  // 8 surrounds + tie + rest + optional surprise pool token
}

void drawSequencerSurface(int fingerCell) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  computeSequencerLayout(w, h);

  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString(s_shiftActive ? "Shift: tap step focus, top row edits params"
                                      : "tap=chord  hold=clear  hold SELECT=Shift tools",
                        4, 2);

  for (int t = 0; t < 3; ++t) {
    char lab[20];
    snprintf(lab, sizeof(lab), "S%u ch%u", t + 1, (unsigned)g_seqMidiCh[t]);
    const bool on = (static_cast<int>(g_seqLane) == t);
    uint16_t bg = on ? g_uiPalette.seqLaneTab : g_uiPalette.panelMuted;
    drawRoundedButton(g_seqTabRects[t], bg, lab, 1);
    if (on) {
      M5.Display.drawRoundRect(g_seqTabRects[t].x, g_seqTabRects[t].y, g_seqTabRects[t].w,
                               g_seqTabRects[t].h, max(4, g_seqTabRects[t].h / 8),
                               g_uiPalette.highlightRing);
    }
  }

  static const char* kToolLabs[4] = {"Qnt", "Swg", "Prb", "Rnd"};
  static const SeqTool kToolMap[4] = {SeqTool::Quantize, SeqTool::Swing, SeqTool::StepProb,
                                      SeqTool::ChordRand};

  for (int i = 0; i < 16; ++i) {
    const Rect& r = g_seqCellRects[i];

    if (s_seqSelectHeld && i < 4) {
      if (s_shiftActive) {
        const uint8_t L = seqLaneClamped();
        char lab[16];
        if (s_shiftSeqFocusStep < 0 || s_shiftSeqFocusStep > 15) {
          snprintf(lab, sizeof(lab), "%s",
                   i == 0 ? "Div --" : (i == 1 ? "Arp --" : (i == 2 ? "Pat --" : "ADiv --")));
        } else if (i == 0) {
          snprintf(lab, sizeof(lab), "Div %s",
                   kStepDivLabs[g_seqExtras.stepClockDiv[L][s_shiftSeqFocusStep] & 0x03U]);
        } else if (i == 1) {
          snprintf(lab, sizeof(lab), "Arp %s", g_seqExtras.arpEnabled[L][s_shiftSeqFocusStep] ? "On" : "Off");
        } else if (i == 2) {
          snprintf(lab, sizeof(lab), "Pat %s",
                   kArpPatLabs[g_seqExtras.arpPattern[L][s_shiftSeqFocusStep] & 0x03U]);
        } else {
          snprintf(lab, sizeof(lab), "ADiv %s",
                   kStepDivLabs[g_seqExtras.arpClockDiv[L][s_shiftSeqFocusStep] & 0x03U]);
        }
        drawRoundedButton(r, g_uiPalette.accentPress, lab, 1);
        if (s_shiftSeqFocusField == static_cast<uint8_t>(i)) {
          M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                                   g_uiPalette.highlightRing);
        }
      } else {
        const bool on = (g_seqTool == kToolMap[i]);
        uint16_t bg = on ? g_uiPalette.seqLaneTab : g_uiPalette.accentPress;
        drawRoundedButton(r, bg, kToolLabs[i], 1);
        if (on) {
          M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                                   g_uiPalette.highlightRing);
        }
      }
      continue;
    }

    const uint8_t v = g_seqPattern[g_seqLane][i];
    uint16_t bg = g_uiPalette.panelMuted;
    if (v == kSeqRest) {
      bg = g_uiPalette.seqRest;
    } else if (v == kSeqTie) {
      bg = g_uiPalette.seqTie;
    } else if (v == kSeqSurprise) {
      bg = g_uiPalette.surprise;
    } else if (v <= 7) {
      bg = colorForRole(g_model.surround[v].role);
    } else {
      bg = g_uiPalette.panelMuted;
    }
    if (i == fingerCell) {
      bg = g_uiPalette.accentPress;
    }
    drawRoundedButton(r, bg, seqStepLabel(v), 1);
    const uint8_t lane = seqLaneClamped();
    const uint8_t stepDiv = g_seqExtras.stepClockDiv[lane][i] & 0x03U;
    const bool arpOn = g_seqExtras.arpEnabled[lane][i] != 0U;
    const uint8_t arpPat = g_seqExtras.arpPattern[lane][i] & 0x03U;
    const uint8_t arpDiv = g_seqExtras.arpClockDiv[lane][i] & 0x03U;
    if (r.h >= 24 && (stepDiv > 0U || arpOn)) {
      M5.Display.setFont(nullptr);
      M5.Display.setTextSize(1);
      M5.Display.setTextColor(g_uiPalette.hintText, bg);
      if (stepDiv > 0U) {
        char db[8];
        snprintf(db, sizeof(db), "D%s", kStepDivLabs[stepDiv]);
        M5.Display.setTextDatum(top_left);
        M5.Display.drawString(db, r.x + 2, r.y + 2);
      }
      if (arpOn) {
        char ab[12];
        if (arpPat == 0U && arpDiv == 0U) {
          snprintf(ab, sizeof(ab), "A");
        } else {
          snprintf(ab, sizeof(ab), "A%s%s", kArpPatLabs[arpPat], kStepDivLabs[arpDiv]);
        }
        M5.Display.setTextDatum(top_right);
        M5.Display.drawString(ab, r.x + r.w - 2, r.y + 2);
      }
    }
    const uint8_t probV = g_seqExtras.stepProb[lane][i];
    if (probV < 100 && r.h >= 24) {
      M5.Display.setFont(nullptr);
      M5.Display.setTextDatum(bottom_right);
      M5.Display.setTextSize(1);
      M5.Display.setTextColor(g_uiPalette.hintText, bg);
      char pr[8];
      snprintf(pr, sizeof(pr), "%u", (unsigned)probV);
      M5.Display.drawString(pr, r.x + r.w - 2, r.y + r.h - 2);
    }
    if (g_seqTool == SeqTool::StepProb && g_seqProbFocusStep == i) {
      M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                             g_uiPalette.highlightRing);
    }
    if (s_shiftActive && s_shiftSeqFocusStep == i) {
      M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                               g_uiPalette.settingsBtnActive);
    }
    if (transportIsPlaying() && transportAudibleStep() == i) {
      M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                             g_uiPalette.feedback);
    }
  }

  if (g_seqSliderActive) {
    M5.Display.fillRoundRect(g_seqSliderPanel.x, g_seqSliderPanel.y, g_seqSliderPanel.w,
                             g_seqSliderPanel.h, 6, g_uiPalette.panelMuted);
    M5.Display.drawRoundRect(g_seqSliderPanel.x, g_seqSliderPanel.y, g_seqSliderPanel.w,
                             g_seqSliderPanel.h, 6, g_uiPalette.settingsBtnBorder);
    const char* title = "";
    switch (g_seqTool) {
      case SeqTool::Quantize: title = "Quantize"; break;
      case SeqTool::Swing: title = "Swing"; break;
      case SeqTool::StepProb: title = "Step probability"; break;
      case SeqTool::ChordRand: title = "Chord random"; break;
      default: break;
    }
    M5.Display.setTextDatum(top_center);
    M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.panelMuted);
    M5.Display.setTextSize(1);
    M5.Display.drawString(title, g_seqSliderPanel.x + g_seqSliderPanel.w / 2,
                          g_seqSliderPanel.y + 4);
    if (g_seqTool == SeqTool::StepProb) {
      char sub[40];
      if (g_seqProbFocusStep >= 0) {
        snprintf(sub, sizeof(sub), "step %u", (unsigned)g_seqProbFocusStep + 1);
      } else {
        snprintf(sub, sizeof(sub), "all steps");
      }
      M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.panelMuted);
      M5.Display.drawString(sub, g_seqSliderPanel.x + g_seqSliderPanel.w / 2,
                            g_seqSliderPanel.y + 14);
    }
    const Rect tr = seqSliderTrackRect();
    M5.Display.fillRoundRect(tr.x, tr.y, tr.w, tr.h, 6, g_uiPalette.bg);
    const uint8_t sv = seqSliderDisplayedValue();
    const int fillW = max(0, (tr.w * static_cast<int>(sv)) / 100);
    if (fillW > 0) {
      M5.Display.fillRoundRect(tr.x, tr.y, fillW, tr.h, 6, g_uiPalette.seqLaneTab);
    }
    M5.Display.drawRoundRect(tr.x, tr.y, tr.w, tr.h, 6, g_uiPalette.settingsBtnBorder);
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", (unsigned)sv);
    M5.Display.setTextDatum(middle_right);
    M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.panelMuted);
    M5.Display.drawString(pct, g_seqSliderPanel.x + g_seqSliderPanel.w - 6,
                          tr.y + tr.h / 2);
  }

  if (s_seqChordDropStep >= 0 && s_seqChordDropStep < 16) {
    const int items = seqChordDropItemCount();
    const int rowH = max(16, (h - kBezelBarH - 4) / items);
    const int totalH = rowH * items;
    const int dropW = 72;
    const Rect& anchor = g_seqCellRects[s_seqChordDropStep];
    int dx = anchor.x + anchor.w / 2 - dropW / 2;
    if (dx < 2) dx = 2;
    if (dx + dropW > w - 2) dx = w - 2 - dropW;
    int dy = (h - kBezelBarH - totalH) / 2;
    if (dy < 2) dy = 2;
    M5.Display.fillRect(dx - 1, dy - 1, dropW + 2, totalH + 2, g_uiPalette.bg);
    for (int r = 0; r < items; ++r) {
      const Rect dr = {dx, dy + r * rowH, dropW, rowH};
      const char* lab;
      if (r < 8) {
        lab = g_model.surround[r].name;
      } else if (r == 8) {
        lab = "~ tie";
      } else if (r == 9) {
        lab = "- rest";
      } else {
        lab = "S? surprise";
      }
      uint16_t bg;
      if (r < 8) {
        bg = colorForRole(g_model.surround[r].role);
      } else if (r == 8) {
        bg = g_uiPalette.seqTie;
      } else if (r == 9) {
        bg = g_uiPalette.seqRest;
      } else {
        bg = g_uiPalette.surprise;
      }
      drawRoundedButton(dr, bg, lab, 1);
    }
  }

  drawBezelBarStrip();
  M5.Display.endWrite();
}

void computeXyLayout(int w, int h) {
  layoutBottomBezels(w, h);
  constexpr int margin = 8;
  constexpr int topBlock = 78;
  constexpr int coordH = 12;
  const int cfgY = 58;
  const int cfgGap = 4;
  const int cfgH = 14;
  const int cfgW = (w - (2 * margin) - (4 * cfgGap)) / 5;
  g_xyCfgChRect = {margin + (cfgW + cfgGap) * 0, cfgY, cfgW, cfgH};
  g_xyCfgCcARect = {margin + (cfgW + cfgGap) * 1, cfgY, cfgW, cfgH};
  g_xyCfgCcBRect = {margin + (cfgW + cfgGap) * 2, cfgY, cfgW, cfgH};
  g_xyCfgCvARect = {margin + (cfgW + cfgGap) * 3, cfgY, cfgW, cfgH};
  g_xyCfgCvBRect = {margin + (cfgW + cfgGap) * 4, cfgY, cfgW, cfgH};
  const int padTop = topBlock;
  const int padBottom = h - kBezelBarH - coordH - 2;
  const int padH = padBottom - padTop;
  const int padW = w - 2 * margin;
  g_xyPadRect = {margin, padTop, padW, max(40, padH)};
}

bool hitTestXyPad(int px, int py) { return pointInRect(px, py, g_xyPadRect); }

void xyMapTouchToVals(int px, int py, uint8_t* outX, uint8_t* outY) {
  const int rw = max(1, g_xyPadRect.w - 1);
  const int rh = max(1, g_xyPadRect.h - 1);
  int relX = px - g_xyPadRect.x;
  int relY = py - g_xyPadRect.y;
  if (relX < 0) relX = 0;
  if (relX > rw) relX = rw;
  if (relY < 0) relY = 0;
  if (relY > rh) relY = rh;
  *outX = static_cast<uint8_t>((int64_t)relX * 127 / rw);
  *outY = static_cast<uint8_t>(127 - (int64_t)relY * 127 / rh);
}

void drawXyCrosshairOnly() {
  const Rect& pr = g_xyPadRect;
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillRoundRect(pr.x + 1, pr.y + 1, pr.w - 2, pr.h - 2, 7, g_uiPalette.xyPadFill);
  M5.Display.drawRoundRect(pr.x, pr.y, pr.w, pr.h, 8, TFT_WHITE);
  const int cx = pr.x + (int)((int64_t)g_xyValX * (pr.w - 1) / 127);
  const int cy = pr.y + (pr.h - 1) - (int)((int64_t)g_xyValY * (pr.h - 1) / 127);
  M5.Display.drawFastVLine(cx, pr.y, pr.h, g_uiPalette.xyAxis);
  M5.Display.drawFastHLine(pr.x, cy, pr.w, g_uiPalette.xyAxis);
  char vals[24];
  snprintf(vals, sizeof(vals), "%u  %u", (unsigned)g_xyValX, (unsigned)g_xyValY);
  const int coordY = pr.y + pr.h + 2;
  M5.Display.fillRect(0, coordY, w, h - kBezelBarH - coordY, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
  M5.Display.drawString(vals, w / 2, coordY);
  M5.Display.endWrite();
}

static const char* xyCurveLabel(uint8_t curve) {
  switch (curve) {
    case 1:
      return "Log";
    case 2:
      return "Inv";
    default:
      return "Lin";
  }
}

static uint8_t applyXyCurve(uint8_t raw, uint8_t curve) {
  uint8_t v = raw > 127 ? 127 : raw;
  switch (curve) {
    case 1: {
      const uint16_t curved = (static_cast<uint16_t>(v) * static_cast<uint16_t>(v)) / 127U;
      return static_cast<uint8_t>(curved == 0U && v > 0U ? 1U : curved);
    }
    case 2:
      return static_cast<uint8_t>(127U - v);
    default:
      return v;
  }
}

static void xySendAxisCc(uint8_t channel0_15, uint8_t cc, uint8_t value7, uint8_t* lastSent7) {
  if (!lastSent7) return;
  if (*lastSent7 == value7) return;
  if (cc <= 31U) {
    // 14-bit CC pair: MSB at cc, LSB at cc+32. Fallback input is 7-bit, expanded to 14-bit.
    const uint16_t v14 = static_cast<uint16_t>(value7) * 129U;
    midiSendControlChange(channel0_15, cc, static_cast<uint8_t>((v14 >> 7) & 0x7F));
    midiSendControlChange(channel0_15, static_cast<uint8_t>(cc + 32U), static_cast<uint8_t>(v14 & 0x7F));
  } else {
    midiSendControlChange(channel0_15, cc, value7);
  }
  *lastSent7 = value7;
}

static void xyQueueAxisCc(uint8_t channel0_15, uint8_t cc, uint8_t value7, uint8_t* lastSent7,
                          uint32_t* lastSendMs, XyAxisPending* pending, uint32_t nowMs, bool force) {
  if (!lastSent7 || !lastSendMs || !pending) return;
  if (*lastSent7 == value7) {
    pending->dirty = false;
    return;
  }
  if (force || (nowMs - *lastSendMs) >= kXyCcMinIntervalMs) {
    xySendAxisCc(channel0_15, cc, value7, lastSent7);
    *lastSendMs = nowMs;
    pending->dirty = false;
    return;
  }
  pending->dirty = true;
  pending->channel = channel0_15;
  pending->cc = cc;
  pending->value = value7;
}

static void xyFlushPending(uint32_t nowMs) {
  if (s_xyPendingX.dirty && (nowMs - s_xyLastSendMsX) >= kXyCcMinIntervalMs) {
    xySendAxisCc(s_xyPendingX.channel, s_xyPendingX.cc, s_xyPendingX.value, &s_xyMidiSentX);
    s_xyLastSendMsX = nowMs;
    s_xyPendingX.dirty = false;
  }
  if (s_xyPendingY.dirty && (nowMs - s_xyLastSendMsY) >= kXyCcMinIntervalMs) {
    xySendAxisCc(s_xyPendingY.channel, s_xyPendingY.cc, s_xyPendingY.value, &s_xyMidiSentY);
    s_xyLastSendMsY = nowMs;
    s_xyPendingY.dirty = false;
  }
}

static const char* xyReleaseModeLabel() {
  return g_settings.xyReturnToCenter ? "Return center" : "Hold last";
}

void drawXyPadSurface() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  computeXyLayout(w, h);

  M5.Display.setFont(nullptr);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString("X-Y MIDI", w / 2, 2);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  char sub[48];
  snprintf(sub, sizeof(sub), "drag pad  CH%u  CC%u / CC%u", (unsigned)g_xyOutChannel, (unsigned)g_xyCcA,
           (unsigned)g_xyCcB);
  M5.Display.drawString(sub, w / 2, 22);
  M5.Display.drawString("BACK/FWD = Transport / Pad / Seq / XY", w / 2, 34);
  M5.Display.drawString("CC 0-31 auto uses 14-bit (MSB/LSB)", w / 2, 42);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  char mode[80];
  snprintf(mode, sizeof(mode), "%s | Release: %s (SELECT)",
           g_xyRecordToSeq ? "Mode: REC->SEQ (SELECT+pad)" : "Mode: LIVE CC", xyReleaseModeLabel());
  M5.Display.drawString(mode, w / 2, 50);

  char bl[24];
  snprintf(bl, sizeof(bl), "CH%u", (unsigned)g_xyOutChannel);
  drawRoundedButton(g_xyCfgChRect, g_uiPalette.panelMuted, bl, 1);
  snprintf(bl, sizeof(bl), "X CC%u", (unsigned)g_xyCcA);
  drawRoundedButton(g_xyCfgCcARect, g_uiPalette.panelMuted, bl, 1);
  snprintf(bl, sizeof(bl), "Y CC%u", (unsigned)g_xyCcB);
  drawRoundedButton(g_xyCfgCcBRect, g_uiPalette.panelMuted, bl, 1);
  snprintf(bl, sizeof(bl), "X %s", xyCurveLabel(g_xyCurveA));
  drawRoundedButton(g_xyCfgCvARect, g_uiPalette.panelMuted, bl, 1);
  snprintf(bl, sizeof(bl), "Y %s", xyCurveLabel(g_xyCurveB));
  drawRoundedButton(g_xyCfgCvBRect, g_uiPalette.panelMuted, bl, 1);

  const Rect& pr = g_xyPadRect;
  M5.Display.fillRoundRect(pr.x, pr.y, pr.w, pr.h, 8, g_uiPalette.xyPadFill);
  M5.Display.drawRoundRect(pr.x, pr.y, pr.w, pr.h, 8, TFT_WHITE);

  const int cx = pr.x + (int)((int64_t)g_xyValX * (pr.w - 1) / 127);
  const int cy = pr.y + (pr.h - 1) - (int)((int64_t)g_xyValY * (pr.h - 1) / 127);
  M5.Display.drawFastVLine(cx, pr.y, pr.h, g_uiPalette.xyAxis);
  M5.Display.drawFastHLine(pr.x, cy, pr.w, g_uiPalette.xyAxis);

  char vals[24];
  snprintf(vals, sizeof(vals), "%u  %u", (unsigned)g_xyValX, (unsigned)g_xyValY);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
  M5.Display.drawString(vals, w / 2, pr.y + pr.h + 2);

  drawBezelBarStrip();
  M5.Display.endWrite();
}

static const char* midiSrcShortLabel(MidiSource src) {
  switch (src) {
    case MidiSource::Usb:
      return "USB";
    case MidiSource::Ble:
      return "BLE";
    case MidiSource::Din:
      return "DIN";
    default:
      return "?";
  }
}

static const char* midiTypeShortLabel(MidiEventType t) {
  switch (t) {
    case MidiEventType::NoteOn:
      return "On";
    case MidiEventType::NoteOff:
      return "Off";
    case MidiEventType::ControlChange:
      return "CC";
    case MidiEventType::ProgramChange:
      return "PC";
    case MidiEventType::PitchBend:
      return "PB";
    case MidiEventType::Clock:
      return "Clk";
    case MidiEventType::Start:
      return "Start";
    case MidiEventType::Continue:
      return "Cont";
    case MidiEventType::Stop:
      return "Stop";
    case MidiEventType::SongPosition:
      return "SPP";
    default:
      return "?";
  }
}

static const char* midiDbgSourceFilterLabel() {
  switch (s_midiDbgSourceFilter) {
    case -1:
      return "Src:All";
    case 0:
      return "Src:USB";
    case 1:
      return "Src:BLE";
    case 2:
      return "Src:DIN";
    default:
      return "Src:?";
  }
}

static const char* midiDbgTypeFilterLabel() {
  if (s_midiDbgTypeFilter < 0) return "Typ:All";
  return midiTypeShortLabel(static_cast<MidiEventType>(s_midiDbgTypeFilter));
}

static bool midiDbgEventMatchesFilters(const MidiEventHistoryItem& ev) {
  if (s_midiDbgSourceFilter >= 0) {
    if (static_cast<int>(ev.source) != s_midiDbgSourceFilter) return false;
  }
  if (s_midiDbgTypeFilter >= 0) {
    if (static_cast<int>(ev.type) != s_midiDbgTypeFilter) return false;
  }
  return true;
}

void formatPanicDebugLine(uint32_t nowMs, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  auto panicDebugRotateMs = []() -> uint32_t {
    switch (g_settings.panicDebugRotate) {
      case 1:
        return 1000U;
      case 2:
        return 2000U;
      case 3:
        return 4000U;
      default:
        return 0U;
    }
  };
  constexpr size_t kShowPerPage = 4;
  const size_t total = static_cast<size_t>(PanicTrigger::Count);
  const size_t pageCount = (total + kShowPerPage - 1U) / kShowPerPage;
  const uint32_t pageMs = panicDebugRotateMs();
  const size_t page = pageCount > 0
                          ? (pageMs == 0U ? 0U : static_cast<size_t>((nowMs / pageMs) % pageCount))
                          : 0U;
  const size_t start = page * kShowPerPage;
  const size_t i0 = start + 0U;
  const size_t i1 = start + 1U;
  const size_t i2 = start + 2U;
  const size_t i3 = start + 3U;
  snprintf(out, outSize, "Panic %s:%lu %s:%lu %s:%lu %s:%lu",
           i0 < total ? panicTriggerShortLabel(static_cast<PanicTrigger>(i0)) : "-",
           i0 < total ? static_cast<unsigned long>(s_panicTriggerCounts[i0]) : 0UL,
           i1 < total ? panicTriggerShortLabel(static_cast<PanicTrigger>(i1)) : "-",
           i1 < total ? static_cast<unsigned long>(s_panicTriggerCounts[i1]) : 0UL,
           i2 < total ? panicTriggerShortLabel(static_cast<PanicTrigger>(i2)) : "-",
           i2 < total ? static_cast<unsigned long>(s_panicTriggerCounts[i2]) : 0UL,
           i3 < total ? panicTriggerShortLabel(static_cast<PanicTrigger>(i3)) : "-",
           i3 < total ? static_cast<unsigned long>(s_panicTriggerCounts[i3]) : 0UL);
}

void formatPanicDebugPage(uint32_t nowMs, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  auto panicDebugRotateMs = []() -> uint32_t {
    switch (g_settings.panicDebugRotate) {
      case 1:
        return 1000U;
      case 2:
        return 2000U;
      case 3:
        return 4000U;
      default:
        return 0U;
    }
  };
  constexpr size_t kShowPerPage = 4;
  const size_t total = static_cast<size_t>(PanicTrigger::Count);
  const size_t pageCount = (total + kShowPerPage - 1U) / kShowPerPage;
  const uint32_t pageMs = panicDebugRotateMs();
  const size_t page = pageCount > 0
                          ? (pageMs == 0U ? 0U : static_cast<size_t>((nowMs / pageMs) % pageCount))
                          : 0U;
  snprintf(out, outSize, "P%u/%u", static_cast<unsigned>(page + 1U), static_cast<unsigned>(pageCount));
}

void drawMidiDebugSurface() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString("MIDI Debug", w / 2, 2);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("SEL: back settings   BACK/FWD: ring pages", w / 2, 20);

  char stat[64];
  char bpmTxt[8];
  const uint16_t extBpm = transportExternalClockBpm();
  if (extBpm >= 40 && extBpm <= 300) {
    snprintf(bpmTxt, sizeof(bpmTxt), "%u", static_cast<unsigned>(extBpm));
  } else {
    snprintf(bpmTxt, sizeof(bpmTxt), "--");
  }
  snprintf(stat, sizeof(stat), "Clk:%s F:%s E:%s BPM:%s", transportExternalClockActive() ? "ext" : "int",
           g_settings.clkFollow ? "on" : "off", g_settings.clkFlashEdge ? "1/8" : "1/4", bpmTxt);
  M5.Display.drawString(stat, w / 2, 32);

  uint16_t aUsb = 0;
  uint16_t aBle = 0;
  uint16_t aDin = 0;
  for (uint8_t ch = 0; ch < 16; ++ch) {
    aUsb = static_cast<uint16_t>(aUsb + s_midiInState.activeNotesOnChannel(MidiSource::Usb, ch));
    aBle = static_cast<uint16_t>(aBle + s_midiInState.activeNotesOnChannel(MidiSource::Ble, ch));
    aDin = static_cast<uint16_t>(aDin + s_midiInState.activeNotesOnChannel(MidiSource::Din, ch));
  }
  char act[64];
  snprintf(act, sizeof(act), "A USB:%u BLE:%u DIN:%u", (unsigned)aUsb, (unsigned)aBle, (unsigned)aDin);
  M5.Display.drawString(act, w / 2, 44);
  const uint32_t nowMs = millis();
  const uint32_t rxNow = bleMidiRxBytesTotal();
  if (s_bleDbgRatePrevMs == 0) {
    s_bleDbgRatePrevMs = nowMs;
    s_bleDbgRatePrevRx = rxNow;
    s_bleDbgRateBps = 0;
  } else {
    const uint32_t dt = nowMs - s_bleDbgRatePrevMs;
    if (dt >= 1000U) {
      const uint32_t dr = rxNow - s_bleDbgRatePrevRx;
      s_bleDbgRateBps = static_cast<uint16_t>((dr * 1000U) / dt);
      if (dr > 0) {
        s_bleDbgLastActivityMs = nowMs;
        s_bleDbgPeakHeld = true;
      }
      if (s_bleDbgRateBps > s_bleDbgRatePeakBps) {
        s_bleDbgRatePeakBps = s_bleDbgRateBps;
      }
      s_bleDbgRatePrevMs = nowMs;
      s_bleDbgRatePrevRx = rxNow;
    }
  }
  uint32_t peakDecayMs = 0;
  switch (g_settings.bleDebugPeakDecay) {
    case 1:
      peakDecayMs = 5000U;
      break;
    case 2:
      peakDecayMs = 10000U;
      break;
    case 3:
      peakDecayMs = 30000U;
      break;
    default:
      peakDecayMs = 0U;
      break;
  }
  // Auto-decay peak after prolonged inactivity (unless disabled).
  if (peakDecayMs > 0 && s_bleDbgLastActivityMs > 0 &&
      (nowMs - s_bleDbgLastActivityMs) > peakDecayMs && s_bleDbgRatePeakBps > s_bleDbgRateBps) {
    s_bleDbgRatePeakBps = s_bleDbgRateBps;
    s_bleDbgPeakHeld = false;
  }
  const uint32_t dinRxNow = dinMidiRxBytesTotal();
  if (s_dinDbgRatePrevMs == 0) {
    s_dinDbgRatePrevMs = nowMs;
    s_dinDbgRatePrevRx = dinRxNow;
    s_dinDbgRateBps = 0;
  } else {
    const uint32_t dt = nowMs - s_dinDbgRatePrevMs;
    if (dt >= 1000U) {
      const uint32_t dr = dinRxNow - s_dinDbgRatePrevRx;
      s_dinDbgRateBps = static_cast<uint16_t>((dr * 1000U) / dt);
      if (dr > 0) {
        s_dinDbgLastActivityMs = nowMs;
        s_dinDbgPeakHeld = true;
      }
      if (s_dinDbgRateBps > s_dinDbgRatePeakBps) {
        s_dinDbgRatePeakBps = s_dinDbgRateBps;
      }
      s_dinDbgRatePrevMs = nowMs;
      s_dinDbgRatePrevRx = dinRxNow;
    }
  }
  if (peakDecayMs > 0 && s_dinDbgLastActivityMs > 0 &&
      (nowMs - s_dinDbgLastActivityMs) > peakDecayMs && s_dinDbgRatePeakBps > s_dinDbgRateBps) {
    s_dinDbgRatePeakBps = s_dinDbgRateBps;
    s_dinDbgPeakHeld = false;
  }
  const uint32_t outTxNow = midiOutTxBytesTotal();
  if (s_outDbgRatePrevMs == 0) {
    s_outDbgRatePrevMs = nowMs;
    s_outDbgRatePrevTx = outTxNow;
    s_outDbgRateBps = 0;
  } else {
    const uint32_t dt = nowMs - s_outDbgRatePrevMs;
    if (dt >= 1000U) {
      const uint32_t db = outTxNow - s_outDbgRatePrevTx;
      s_outDbgRateBps = static_cast<uint16_t>((db * 1000U) / dt);
      if (s_outDbgRateBps > s_outDbgRatePeakBps) {
        s_outDbgRatePeakBps = s_outDbgRateBps;
      }
      s_outDbgRatePrevMs = nowMs;
      s_outDbgRatePrevTx = outTxNow;
    }
  }
  const uint32_t bleDrop = bleMidiDecodeDropTotal();
  if (bleDrop > s_bleDbgPrevDrop && bleMidiConnected()) {
    s_bleDbgWarnUntilMs = nowMs + 1500U;
  }
  s_bleDbgPrevDrop = bleDrop;
  const bool bleWarn = nowMs < s_bleDbgWarnUntilMs;
  M5.Display.setTextColor(bleWarn ? g_uiPalette.accentPress : g_uiPalette.hintText, g_uiPalette.bg);
  char bleStat[72];
  snprintf(bleStat, sizeof(bleStat), "BLE:%s RX:%lu %u/s P:%u%c D:%lu Q:%lu",
           bleMidiConnected() ? "Conn" : "Adv",
           static_cast<unsigned long>(rxNow), static_cast<unsigned>(s_bleDbgRateBps),
           static_cast<unsigned>(s_bleDbgRatePeakBps), s_bleDbgPeakHeld ? 'h' : 'a',
           static_cast<unsigned long>(bleDrop),
           static_cast<unsigned long>(midiIngressBleQueueDropTotal()));
  M5.Display.drawString(bleStat, w / 2, 54);
  char bleDropStat[72];
  snprintf(bleDropStat, sizeof(bleDropStat), "Drop ts:%lu dat:%lu tr:%lu",
           static_cast<unsigned long>(bleMidiDecodeDropTimestampTotal()),
           static_cast<unsigned long>(bleMidiDecodeDropInvalidDataTotal()),
           static_cast<unsigned long>(bleMidiDecodeDropTruncatedTotal()));
  M5.Display.drawString(bleDropStat, w / 2, 64);
  char dinStat[64];
  snprintf(dinStat, sizeof(dinStat), "DIN:%s RX:%lu %u/s P:%u%c D:%lu", dinMidiReady() ? "Ready" : "Off",
           static_cast<unsigned long>(dinRxNow), static_cast<unsigned>(s_dinDbgRateBps),
           static_cast<unsigned>(s_dinDbgRatePeakBps), s_dinDbgPeakHeld ? 'h' : 'a',
           static_cast<unsigned long>(midiIngressDinQueueDropTotal()));
  M5.Display.drawString(dinStat, w / 2, 74);
  char panicStat[96];
  formatPanicDebugLine(nowMs, panicStat, sizeof(panicStat));
  M5.Display.drawString(panicStat, w / 2, 84);
  char panicPage[12];
  formatPanicDebugPage(nowMs, panicPage, sizeof(panicPage));
  M5.Display.setTextDatum(top_right);
  M5.Display.drawString(panicPage, w - 6, 84);
  M5.Display.setTextDatum(top_center);
  char outStat[96];
  snprintf(outStat, sizeof(outStat), "OUT TX:%lu %u/s P:%u NOn:%lu NOff:%lu CC:%lu",
           static_cast<unsigned long>(outTxNow), static_cast<unsigned>(s_outDbgRateBps),
           static_cast<unsigned>(s_outDbgRatePeakBps),
           static_cast<unsigned long>(midiOutTxNoteOnTotal()),
           static_cast<unsigned long>(midiOutTxNoteOffTotal()),
           static_cast<unsigned long>(midiOutTxControlChangeTotal()));
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString(outStat, w / 2, 94);
  const char* sugWnd = g_settings.suggestStableWindow == 0
                           ? "0.6s"
                           : (g_settings.suggestStableWindow == 1
                                  ? "1.0s"
                                  : (g_settings.suggestStableWindow == 2
                                         ? "1.4s"
                                         : (g_settings.suggestStableWindow == 3 ? "2.0s" : "3.0s")));
  const char* sugGap = g_settings.suggestStableGap == 0
                           ? "8"
                           : (g_settings.suggestStableGap == 1
                                  ? "12"
                                  : (g_settings.suggestStableGap == 2
                                         ? "18"
                                         : (g_settings.suggestStableGap == 3 ? "24" : "32")));
  char sugTune[40];
  snprintf(sugTune, sizeof(sugTune), "Prf:%c Lk:%c Wnd:%s Gap:%s",
           g_settings.suggestProfile == 0 ? 'A' : 'B', g_settings.suggestProfileLock ? 'Y' : 'N', sugWnd,
           sugGap);
  M5.Display.drawString(sugTune, w / 2, 104);
  if (s_midiSuggestTop3UntilMs > nowMs && s_midiSuggestTop3[0][0] != '\0') {
    char sugStat[96];
    snprintf(sugStat, sizeof(sugStat), "SUG%c %s(%d) %s(%d) %s(%d)",
             (s_midiSuggestStableUntilMs > nowMs) ? 'S' : '-', s_midiSuggestTop3[0],
             static_cast<int>(s_midiSuggestTop3Score[0]), s_midiSuggestTop3[1],
             static_cast<int>(s_midiSuggestTop3Score[1]), s_midiSuggestTop3[2],
             static_cast<int>(s_midiSuggestTop3Score[2]));
    M5.Display.drawString(sugStat, w / 2, 114);
  }
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);

  const int chipY = 126;
  const int chipH = 14;
  const int chipGap = 8;
  const int chipW = (w - 12 - 2 * chipGap) / 3;
  g_midiDbgSourceChip = {6, chipY, chipW, chipH};
  g_midiDbgTypeChip = {6 + chipW + chipGap, chipY, chipW, chipH};
  g_midiDbgResetChip = {6 + 2 * (chipW + chipGap), chipY, chipW, chipH};
  drawRoundedButton(g_midiDbgSourceChip, g_uiPalette.panelMuted, midiDbgSourceFilterLabel(), 1);
  drawRoundedButton(g_midiDbgTypeChip, g_uiPalette.panelMuted, midiDbgTypeFilterLabel(), 1);
  drawRoundedButton(g_midiDbgResetChip, g_uiPalette.panelMuted, "Filters:Reset", 1);

  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
  M5.Display.drawString("Recent events:", 6, chipY + chipH + 3);
  int y = chipY + chipH + 15;
  const int maxRows = max(1, (h - kBezelBarH - y - 4) / 12);
  int shown = 0;
  for (int i = 0; i < 64 && shown < maxRows; ++i) {
    MidiEventHistoryItem ev{};
    if (!s_midiEventHistory.getNewestFirst(static_cast<size_t>(i), &ev)) break;
    if (!midiDbgEventMatchesFilters(ev)) continue;
    char line[80];
    const uint32_t nowMs = millis();
    const uint32_t ageMs = nowMs >= ev.atMs ? (nowMs - ev.atMs) : 0;
    if (ev.type == MidiEventType::SongPosition) {
      snprintf(line, sizeof(line), "%4lums %s %-5s %u", (unsigned long)ageMs,
               midiSrcShortLabel(ev.source), midiTypeShortLabel(ev.type), (unsigned)ev.value14);
    } else {
      snprintf(line, sizeof(line), "%4lums %s %-5s ch%u %3u %3u", (unsigned long)ageMs,
               midiSrcShortLabel(ev.source), midiTypeShortLabel(ev.type), (unsigned)(ev.channel + 1),
               (unsigned)ev.data1, (unsigned)ev.data2);
    }
    M5.Display.drawString(line, 6, y);
    y += 12;
    shown++;
  }
  drawBezelBarStrip();
  M5.Display.endWrite();
}

void processMidiDebugTouch(uint8_t touchCount, int w, int h) {
  if (touchCount > 0) {
    if (!wasTouchActive) {
      s_playCategoryTouchStartMs = millis();
    }
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
    wasTouchActive = true;
    return;
  }
  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;
  layoutBottomBezels(w, h);
  if (pointInRect(hx, hy, g_bezelSelect)) {
    panicForTrigger(PanicTrigger::EnterSettings);
    g_screen = Screen::Settings;
    drawSettingsUi();
    return;
  }
  if (pointInRect(hx, hy, g_bezelBack)) {
    navigateMainRing(-1);
    return;
  }
  if (pointInRect(hx, hy, g_bezelFwd)) {
    navigateMainRing(1);
    return;
  }
  if (pointInRect(hx, hy, g_midiDbgSourceChip)) {
    s_midiDbgSourceFilter++;
    if (s_midiDbgSourceFilter > 2) s_midiDbgSourceFilter = -1;
    drawMidiDebugSurface();
    return;
  }
  if (pointInRect(hx, hy, g_midiDbgTypeChip)) {
    static const MidiEventType kTypeCycle[] = {
        MidiEventType::NoteOn, MidiEventType::NoteOff, MidiEventType::ControlChange, MidiEventType::Clock,
        MidiEventType::Start,  MidiEventType::Stop,    MidiEventType::SongPosition};
    if (s_midiDbgTypeFilter < 0) {
      s_midiDbgTypeFilter = static_cast<int8_t>(kTypeCycle[0]);
    } else {
      int idx = -1;
      for (int i = 0; i < static_cast<int>(sizeof(kTypeCycle) / sizeof(kTypeCycle[0])); ++i) {
        if (s_midiDbgTypeFilter == static_cast<int8_t>(kTypeCycle[i])) {
          idx = i;
          break;
        }
      }
      if (idx < 0 || idx + 1 >= static_cast<int>(sizeof(kTypeCycle) / sizeof(kTypeCycle[0]))) {
        s_midiDbgTypeFilter = -1;
      } else {
        s_midiDbgTypeFilter = static_cast<int8_t>(kTypeCycle[idx + 1]);
      }
    }
    drawMidiDebugSurface();
    return;
  }
  if (pointInRect(hx, hy, g_midiDbgResetChip)) {
    s_midiDbgSourceFilter = -1;
    s_midiDbgTypeFilter = -1;
    drawMidiDebugSurface();
    return;
  }
  drawMidiDebugSurface();
}

void beforeLeaveSequencer() {
  if (g_screen == Screen::Sequencer) {
    seqPatternSave(g_seqPattern);
    seqExtrasSave(&g_seqExtras);
  }
}

bool tryEnterSettingsTwoFingerLong(uint8_t touchCount, int w, int h);

void midiPanicAllChannels() {
  for (uint8_t ch = 0; ch < 16; ++ch) {
    midiSendAllNotesOff(ch);
  }
  midiSilenceLastChord(0);
}

const char* panicTriggerShortLabel(PanicTrigger trigger) {
  switch (trigger) {
    case PanicTrigger::EnterSettings:
      return "Set";
    case PanicTrigger::RingNavigation:
      return "Ring";
    case PanicTrigger::TransportStop:
      return "Stop";
    case PanicTrigger::ExternalTransportStop:
      return "ExtStop";
    case PanicTrigger::KeyPickerTransition:
      return "Key";
    case PanicTrigger::ProjectNameTransition:
      return "Proj";
    case PanicTrigger::RestoreTransition:
      return "Rest";
    case PanicTrigger::FactoryReset:
      return "Reset";
    case PanicTrigger::SettingsSaveExit:
      return "Save";
    case PanicTrigger::ErrorPath:
      return "Err";
    default:
      return "?";
  }
}

void panicForTrigger(PanicTrigger trigger) {
  const size_t idx = static_cast<size_t>(trigger);
  if (idx < static_cast<size_t>(PanicTrigger::Count)) {
    s_panicTriggerCounts[idx]++;
  }
  midiPanicAllChannels();
}

void navigateMainRing(int direction) {
  panicForTrigger(PanicTrigger::RingNavigation);
  static const Screen kRing[] = {Screen::Transport, Screen::Play, Screen::Sequencer, Screen::XyPad};
  beforeLeaveSequencer();
  if (g_screen == Screen::Play) {
    s_playVoicingPanelOpen = false;
  }
  if (g_screen == Screen::Transport) {
    transportDropDismiss();
  }
  int idx = 0;
  bool found = false;
  for (int i = 0; i < 4; ++i) {
    if (kRing[i] == g_screen) {
      idx = i;
      found = true;
      break;
    }
  }
  if (!found) {
    idx = 1;
  }
  idx = (idx + direction + 400) % 4;
  g_screen = kRing[idx];
  s_xyMidiSentX = 255;
  s_xyMidiSentY = 255;
  s_xyLastSendMsX = 0;
  s_xyLastSendMsY = 0;
  s_xyPendingX.dirty = false;
  s_xyPendingY.dirty = false;
  switch (g_screen) {
    case Screen::Transport:
      drawTransportSurface();
      break;
    case Screen::Play:
      drawPlaySurface();
      break;
    case Screen::Sequencer:
      drawSequencerSurface();
      break;
    case Screen::XyPad:
      drawXyPadSurface();
      break;
    default:
      break;
  }
}

void processXyTouch(uint8_t touchCount, int w, int h) {
  computeXyLayout(w, h);

  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
  }

  if (touchCount >= 2) {
    playResetBezelFastCycle();
    wasTouchActive = true;
    layoutBottomBezels(w, h);
    int sel = -1;
    int pad = -1;
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (!d.isPressed()) continue;
      if (pointInRect(d.x, d.y, g_bezelSelect)) {
        sel = static_cast<int>(i);
      } else if (hitTestXyPad(d.x, d.y)) {
        pad = static_cast<int>(i);
      }
    }
    if (sel >= 0 && pad >= 0) {
      const auto& dm = M5.Touch.getDetail(static_cast<uint8_t>(pad));
      if (!s_xyCombo.armed) {
        s_xyCombo.sx = dm.x;
        s_xyCombo.sy = dm.y;
        s_xyCombo.armed = true;
      }
      s_xyCombo.lx = dm.x;
      s_xyCombo.ly = dm.y;
    }
    wasTouchActive = true;
    if (!s_xyTwoFingerSurfaceDrawn) {
      drawXyPadSurface();
      s_xyTwoFingerSurfaceDrawn = true;
    }
    return;
  }

  const uint8_t midiCh = static_cast<uint8_t>((g_xyOutChannel < 1 || g_xyOutChannel > 16) ? 0U
                                                                                           : (g_xyOutChannel - 1U));

  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    const bool cfgTap = pointInRect(d.x, d.y, g_xyCfgChRect) || pointInRect(d.x, d.y, g_xyCfgCcARect) ||
                        pointInRect(d.x, d.y, g_xyCfgCcBRect) || pointInRect(d.x, d.y, g_xyCfgCvARect) ||
                        pointInRect(d.x, d.y, g_xyCfgCvBRect);
    const bool bezel = pointInRect(d.x, d.y, g_bezelBack) || pointInRect(d.x, d.y, g_bezelSelect) ||
                       pointInRect(d.x, d.y, g_bezelFwd);
    if (bezel || cfgTap) {
      if (s_xyTouchZone == 1) {
        drawXyPadSurface();
      }
      s_xyTouchZone = 0;
    } else if (hitTestXyPad(d.x, d.y)) {
      const uint8_t ox = g_xyValX;
      const uint8_t oy = g_xyValY;
      uint8_t vx, vy;
      xyMapTouchToVals(d.x, d.y, &vx, &vy);
      g_xyValX = vx;
      g_xyValY = vy;
      const bool suppressMidi =
          g_xyRecordToSeq && transportIsPlaying() && transportIsRecordingLive();
      if (!suppressMidi) {
        const uint8_t sx = applyXyCurve(vx, g_xyCurveA);
        const uint8_t sy = applyXyCurve(vy, g_xyCurveB);
        const uint32_t nowMs = millis();
        xyQueueAxisCc(midiCh, g_xyCcA, sx, &s_xyMidiSentX, &s_xyLastSendMsX, &s_xyPendingX, nowMs, false);
        xyQueueAxisCc(midiCh, g_xyCcB, sy, &s_xyMidiSentY, &s_xyLastSendMsY, &s_xyPendingY, nowMs, false);
      }
      if (s_xyTouchZone != 1) {
        drawXyPadSurface();
      } else if (vx != ox || vy != oy) {
        drawXyCrosshairOnly();
      }
      s_xyTouchZone = 1;
    } else {
      if (s_xyTouchZone == 1) {
        drawXyPadSurface();
      }
      s_xyTouchZone = 0;
    }
    wasTouchActive = true;
    return;
  }

  if (touchCount == 0) {
    if (!wasTouchActive) return;
    const int8_t zoneBeforeRelease = s_xyTouchZone;
    const bool suppressMidi = g_xyRecordToSeq && transportIsPlaying() && transportIsRecordingLive();
    s_xyTouchZone = -1;
    s_xyTwoFingerSurfaceDrawn = false;
    wasTouchActive = false;
    if (suppressNextPlayTap) {
      suppressNextPlayTap = false;
      drawXyPadSurface();
      return;
    }

    if (s_xyCombo.armed) {
      const int dx = s_xyCombo.lx - s_xyCombo.sx;
      const int dy = s_xyCombo.ly - s_xyCombo.sy;
      s_xyCombo.armed = false;
      if (abs(dx) < 22 && abs(dy) < 22) {
        g_xyRecordToSeq = !g_xyRecordToSeq;
        transportPrefsSave();
        drawXyPadSurface();
        return;
      }
    }

    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;
    if (pointInRect(hx, hy, g_bezelBack)) {
      navigateMainRing(-1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      navigateMainRing(1);
      return;
    }
    if (pointInRect(hx, hy, g_xyCfgChRect)) {
      g_xyOutChannel = static_cast<uint8_t>((g_xyOutChannel % 16U) + 1U);
      xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
      drawXyPadSurface();
      return;
    }
    if (pointInRect(hx, hy, g_xyCfgCcARect)) {
      g_xyCcA = static_cast<uint8_t>((g_xyCcA + 1U) & 0x7FU);
      xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
      drawXyPadSurface();
      return;
    }
    if (pointInRect(hx, hy, g_xyCfgCcBRect)) {
      g_xyCcB = static_cast<uint8_t>((g_xyCcB + 1U) & 0x7FU);
      xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
      drawXyPadSurface();
      return;
    }
    if (pointInRect(hx, hy, g_xyCfgCvARect)) {
      g_xyCurveA = static_cast<uint8_t>((g_xyCurveA + 1U) % 3U);
      xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
      drawXyPadSurface();
      return;
    }
    if (pointInRect(hx, hy, g_xyCfgCvBRect)) {
      g_xyCurveB = static_cast<uint8_t>((g_xyCurveB + 1U) % 3U);
      xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
      drawXyPadSurface();
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      g_settings.xyReturnToCenter = g_settings.xyReturnToCenter ? 0U : 1U;
      settingsSave(g_settings);
      drawXyPadSurface();
      return;
    }
    if (zoneBeforeRelease == 1 && g_settings.xyReturnToCenter) {
      g_xyValX = 64;
      g_xyValY = 64;
      if (!suppressMidi) {
        const uint8_t sx = applyXyCurve(g_xyValX, g_xyCurveA);
        const uint8_t sy = applyXyCurve(g_xyValY, g_xyCurveB);
        const uint32_t nowMs = millis();
        xyQueueAxisCc(midiCh, g_xyCcA, sx, &s_xyMidiSentX, &s_xyLastSendMsX, &s_xyPendingX, nowMs, true);
        xyQueueAxisCc(midiCh, g_xyCcB, sy, &s_xyMidiSentY, &s_xyLastSendMsY, &s_xyPendingY, nowMs, true);
      }
    }
    drawXyPadSurface();
  }
}

bool tryEnterSettingsTwoFingerLong(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);
  constexpr uint8_t kMaxTouch = 5;
  int xs[kMaxTouch];
  int ys[kMaxTouch];
  bool pr[kMaxTouch];
  const uint8_t n = touchCount < kMaxTouch ? touchCount : kMaxTouch;
  for (uint8_t i = 0; i < n; ++i) {
    const auto& d = M5.Touch.getDetail(i);
    xs[i] = d.x;
    ys[i] = d.y;
    pr[i] = d.isPressed();
  }
  const IntRect back = {g_bezelBack.x, g_bezelBack.y, g_bezelBack.w, g_bezelBack.h};
  const IntRect fwd = {g_bezelFwd.x, g_bezelFwd.y, g_bezelFwd.w, g_bezelFwd.h};
  const bool covers = settingsEntryGesture_touchesCoverBackAndFwd(n, xs, ys, pr, back, fwd);
  if (!g_settingsEntryGesture.needRelease && covers) {
    const uint32_t now = millis();
    const uint32_t start = (g_settingsEntryGesture.holdStartMs == 0) ? now : g_settingsEntryGesture.holdStartMs;
    drawHoldProgressStrip(HoldProgressOwner::SettingsDual, now - start, settingsEntryHoldMs(),
                          "Hold BACK+FWD: settings");
  } else {
    clearHoldProgressStrip(HoldProgressOwner::SettingsDual);
  }
  const SettingsEntryGestureResult r = settingsEntryGesture_update(
      g_settingsEntryGesture, millis(), n, xs, ys, pr, back, fwd, settingsEntryHoldMs());
  if (r.enteredSettings) {
    clearHoldProgressStrip(HoldProgressOwner::SettingsDual);
    panicForTrigger(PanicTrigger::EnterSettings);
    suppressNextPlayTap = r.suppressNextPlayTap;
    transportDropDismiss();
    g_screen = Screen::Settings;
    resetSettingsNav();
    return true;
  }
  return false;
}

int hitTestSeqChordDrop(int px, int py) {
  if (s_seqChordDropStep < 0 || s_seqChordDropStep >= 16) return -1;
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  const int items = seqChordDropItemCount();
  const int rowH = max(16, (h - kBezelBarH - 4) / items);
  const int totalH = rowH * items;
  const int dropW = 72;
  const Rect& anchor = g_seqCellRects[s_seqChordDropStep];
  int dx = anchor.x + anchor.w / 2 - dropW / 2;
  if (dx < 2) dx = 2;
  if (dx + dropW > w - 2) dx = w - 2 - dropW;
  int dy = (h - kBezelBarH - totalH) / 2;
  if (dy < 2) dy = 2;
  for (int r = 0; r < items; ++r) {
    if (px >= dx && px < dx + dropW && py >= dy + r * rowH && py < dy + (r + 1) * rowH) {
      return r;
    }
  }
  return -1;
}

void processSequencerTouch(uint8_t touchCount, int w, int h) {
  computeSequencerLayout(w, h);

  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    if (!wasTouchActive) {
      s_seqTouchStartMs = millis();
    }
    if (touchCount > s_seqGestureMaxTouches) {
      s_seqGestureMaxTouches = static_cast<uint8_t>(touchCount);
    }
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
  }

  bool selectDown = false;
  for (uint8_t i = 0; i < touchCount; ++i) {
    const auto& d = M5.Touch.getDetail(i);
    if (d.isPressed() && pointInRect(d.x, d.y, g_bezelSelect)) selectDown = true;
  }
  if (touchCount >= 2) {
    shiftSeqResetBackFwdNudgeHold();
    wasTouchActive = true;
    s_seqComboTab = -1;
    if (selectDown) {
      for (uint8_t i = 0; i < touchCount; ++i) {
        const auto& d = M5.Touch.getDetail(i);
        if (!d.isPressed()) continue;
        const int th = hitTestSeqTab(d.x, d.y);
        if (th >= 0) { s_seqComboTab = th; break; }
      }
    }
    s_wasSeqMultiFinger = true;
    return;
  }

  if (s_wasSeqMultiFinger) {
    s_wasSeqMultiFinger = false;
  }

  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    if (s_shiftActive && d.isPressed()) {
      if (pointInRect(d.x, d.y, g_bezelBack)) {
        wasTouchActive = true;
        shiftSeqHandleBackFwdNudgeHold(-1);
        return;
      }
      if (pointInRect(d.x, d.y, g_bezelFwd)) {
        wasTouchActive = true;
        shiftSeqHandleBackFwdNudgeHold(1);
        return;
      }
      shiftSeqResetBackFwdNudgeHold();
    }
    if (g_seqTool != SeqTool::None && g_seqSliderActive &&
        pointInRect(d.x, d.y, g_seqSliderPanel)) {
      const bool wasDragging = g_seqDraggingSlider;
      g_seqDraggingSlider = true;
      const uint8_t svBefore = seqSliderDisplayedValue();
      seqSetSliderValueFromX(d.x);
      const uint8_t svAfter = seqSliderDisplayedValue();
      if (!wasDragging || svBefore != svAfter) {
        drawSequencerSurface();
      }
      wasTouchActive = true;
      return;
    }

    const uint32_t now = millis();
    if (s_seqChordDropStep < 0) {
      const int ht = hitTestSeq(d.x, d.y);
      if (ht >= 0 && !(s_seqSelectHeld && ht < 4)) {
        if (s_seqStepDownIdx != ht) {
          s_seqStepDownIdx = ht;
          s_seqStepDownMs = now;
          s_seqLongPressHandled = false;
        }
        if (!s_seqLongPressHandled && now - s_seqStepDownMs >= seqLongPressMs()) {
          g_seqPattern[g_seqLane][ht] = kSeqRest;
          s_seqLongPressHandled = true;
          drawSequencerSurface();
        }
      } else {
        s_seqStepDownIdx = -1;
      }
    }
    wasTouchActive = true;
    return;
  }

  if (touchCount == 0) {
    if (!wasTouchActive) return;
    shiftSeqResetBackFwdNudgeHold();
    if (s_shiftSeqNudgeConsumed) {
      s_shiftSeqNudgeConsumed = false;
      s_lastSeqPreviewCell = -9999;
      s_wasSeqMultiFinger = false;
      wasTouchActive = false;
      return;
    }
    if (millis() - s_seqTouchStartMs < kTapDebounceMs) {
      s_lastSeqPreviewCell = -9999;
      s_wasSeqMultiFinger = false;
      wasTouchActive = false;
      s_seqStepDownIdx = -1;
      s_seqLongPressHandled = false;
      return;
    }
    s_lastSeqPreviewCell = -9999;
    s_wasSeqMultiFinger = false;
    wasTouchActive = false;
    if (suppressNextPlayTap) {
      suppressNextPlayTap = false;
      drawSequencerSurface();
      return;
    }

    if (g_seqDraggingSlider) {
      g_seqDraggingSlider = false;
      seqExtrasSave(&g_seqExtras);
      drawSequencerSurface();
      return;
    }

    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;

    if (s_seqGestureMaxTouches >= 2 && s_seqComboTab >= 0) {
      uint8_t ch = g_seqMidiCh[s_seqComboTab];
      ch = static_cast<uint8_t>((ch % 16) + 1);
      g_seqMidiCh[s_seqComboTab] = ch;
      seqLaneChannelsSave(g_seqMidiCh);
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      drawSequencerSurface();
      return;
    }

    if (pointInRect(hx, hy, g_bezelBack)) {
      if (s_shiftActive && s_shiftSeqFocusStep >= 0) {
        shiftSeqAdjustFocusedParam(-1);
        drawSequencerSurface();
        return;
      }
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      s_seqChordDropStep = -1;
      s_seqSelectHeld = false;
      navigateMainRing(-1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      if (s_shiftActive && s_shiftSeqFocusStep >= 0) {
        shiftSeqAdjustFocusedParam(1);
        drawSequencerSurface();
        return;
      }
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      s_seqChordDropStep = -1;
      s_seqSelectHeld = false;
      navigateMainRing(1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      if (s_shiftTriggeredThisHold) {
        s_shiftTriggeredThisHold = false;
        drawSequencerSurface();
        return;
      }
      const uint8_t maxT = s_seqGestureMaxTouches;
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      s_seqChordDropStep = -1;
      if (maxT <= 1) s_seqSelectHeld = s_shiftActive;
      drawSequencerSurface();
      return;
    }

    s_seqGestureMaxTouches = 0;
    s_seqComboTab = -1;

    if (s_seqChordDropStep >= 0) {
      const int pick = hitTestSeqChordDrop(hx, hy);
      if (pick >= 0) {
        uint8_t v;
        if (pick < 8) v = static_cast<uint8_t>(pick);
        else if (pick == 8) v = kSeqTie;
        else if (pick == 9) v = kSeqRest;
        else v = kSeqSurprise;
        g_seqPattern[g_seqLane][s_seqChordDropStep] = v;
      }
      s_seqChordDropStep = -1;
      drawSequencerSurface();
      return;
    }

    if (s_seqSelectHeld) {
      const int ht = hitTestSeq(hx, hy);
      if (ht >= 0 && ht < 4) {
        if (s_shiftActive) {
          const uint8_t L = seqLaneClamped();
          if (s_shiftSeqFocusStep >= 0 && s_shiftSeqFocusStep < 16) {
            s_shiftSeqFocusField = static_cast<uint8_t>(ht);
            if (ht == 0) {
              g_seqExtras.stepClockDiv[L][s_shiftSeqFocusStep] =
                  static_cast<uint8_t>((g_seqExtras.stepClockDiv[L][s_shiftSeqFocusStep] + 1U) & 0x03U);
            } else if (ht == 1) {
              g_seqExtras.arpEnabled[L][s_shiftSeqFocusStep] =
                  static_cast<uint8_t>(g_seqExtras.arpEnabled[L][s_shiftSeqFocusStep] ? 0U : 1U);
            } else if (ht == 2) {
              g_seqExtras.arpPattern[L][s_shiftSeqFocusStep] =
                  static_cast<uint8_t>((g_seqExtras.arpPattern[L][s_shiftSeqFocusStep] + 1U) & 0x03U);
            } else {
              g_seqExtras.arpClockDiv[L][s_shiftSeqFocusStep] =
                  static_cast<uint8_t>((g_seqExtras.arpClockDiv[L][s_shiftSeqFocusStep] + 1U) & 0x03U);
            }
          }
        } else {
          static const SeqTool kMap[4] = {SeqTool::Quantize, SeqTool::Swing, SeqTool::StepProb,
                                          SeqTool::ChordRand};
          const SeqTool tapped = kMap[ht];
          if (g_seqTool == tapped) {
            g_seqTool = SeqTool::None;
            g_seqProbFocusStep = -1;
          } else {
            g_seqTool = tapped;
            if (tapped != SeqTool::StepProb) g_seqProbFocusStep = -1;
          }
        }
        seqExtrasSave(&g_seqExtras);
        drawSequencerSurface();
        return;
      }
    }

    const int tabHit = hitTestSeqTab(hx, hy);
    if (tabHit >= 0) {
      g_seqLane = static_cast<uint8_t>(tabHit);
      shiftSeqFocusSyncFromCurrentLane();
      drawSequencerSurface();
      return;
    }

    if (s_seqLongPressHandled) {
      s_seqStepDownIdx = -1;
      s_seqLongPressHandled = false;
      drawSequencerSurface();
      return;
    }

    const int cell = hitTestSeq(hx, hy);
    if (cell >= 0) {
      if (s_shiftActive) {
        shiftSeqFocusSetForCurrentLane(static_cast<int8_t>(cell));
        drawSequencerSurface();
        return;
      }
      if (g_seqTool == SeqTool::StepProb) {
        g_seqProbFocusStep = static_cast<int8_t>(cell);
        seqExtrasSave(&g_seqExtras);
        drawSequencerSurface();
        return;
      }
      s_seqChordDropStep = cell;
      drawSequencerSurface();
      return;
    }
    s_seqStepDownIdx = -1;
    drawSequencerSurface();
  }
}

void processPlayTouch(uint8_t touchCount, int w, int h) {
  computePlaySurfaceLayout(w, h);

  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    if (!wasTouchActive) {
      s_playTouchStartMs = millis();
    }
    if (touchCount > s_playGestureMaxTouches) {
      s_playGestureMaxTouches = static_cast<uint8_t>(touchCount);
    }
  }

  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
  }

  bool keyDown = false;
  bool selectDown = false;
  bool pad0Down = false;
  for (uint8_t i = 0; i < touchCount; ++i) {
    const auto& d = M5.Touch.getDetail(i);
    if (!d.isPressed()) continue;
    if (pointInRect(d.x, d.y, g_bezelSelect)) selectDown = true;
    if (pointInRect(d.x, d.y, g_keyRect)) keyDown = true;
    if (hitTestPlay(d.x, d.y) == 0) pad0Down = true;
  }

  s_drawPlayVoicingShift =
      (touchCount >= 2 && selectDown && pad0Down && !keyDown && !s_playSelectLatched);
  if (touchCount >= 2) {
    s_playVoicingCombo = selectDown && pad0Down && !keyDown && !s_playSelectLatched;
  }

  if (touchCount >= 2) {
    wasTouchActive = true;
    bool fk = false;
    int ch = -100;
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (!d.isPressed()) continue;
      if (pointInRect(d.x, d.y, g_keyRect)) fk = true;
      const int ht = hitTestPlay(d.x, d.y);
      if (ht >= 0) ch = ht;
    }
    const bool vo = s_drawPlayVoicingShift;
    if (ch != s_playTouchDrawChord || fk != s_playTouchDrawOnKey || vo != s_playTouchDrawVoicing) {
      s_playTouchDrawChord = ch;
      s_playTouchDrawOnKey = fk;
      s_playTouchDrawVoicing = vo;
    }
    return;
  }

  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    if (d.isPressed()) {
      if (pointInRect(d.x, d.y, g_bezelBack)) {
        wasTouchActive = true;
        playHandleBezelFastCycle(-1);
        return;
      }
      if (pointInRect(d.x, d.y, g_bezelFwd)) {
        wasTouchActive = true;
        playHandleBezelFastCycle(1);
        return;
      }
      playResetBezelFastCycle();
    }
    if (d.isPressed()) {
      const int ht = hitTestPlay(d.x, d.y);
      if (ht == -2 && !s_playSelectLatched) {
        if (!s_playKeyHoldArmed) {
          s_playKeyHoldArmed = true;
          s_playKeyHoldStartMs = millis();
        } else if (millis() - s_playKeyHoldStartMs >= 700U) {
          s_playOpenCategoryOnRelease = true;
        }
      } else {
        s_playKeyHoldArmed = false;
      }
    }
    if (s_playVoicingPanelOpen) {
      layoutPlayVoicingPanel(w, h);
      if (pointInRect(d.x, d.y, g_playVoicingPanel)) {
        playSetVoicingFromX(d.x);
        if (g_chordVoicing != s_playVoicingDisplayed) {
          s_playVoicingDisplayed = g_chordVoicing;
          M5.Display.startWrite();
          playRedrawGridBand();
          drawBezelBarStrip();
          M5.Display.endWrite();
        }
        s_playVoicingDragging = true;
        wasTouchActive = true;
        return;
      }
    }
    const int ht = hitTestPlay(d.x, d.y);
    int dc = -100;
    bool dok = false;
    if (pointInRect(d.x, d.y, g_bezelBack) || pointInRect(d.x, d.y, g_bezelSelect) ||
        pointInRect(d.x, d.y, g_bezelFwd)) {
      dc = -100;
      dok = false;
    } else if (ht == -2) {
      dc = -100;
      dok = true;
    } else if (ht >= 0) {
      dc = ht;
      dok = false;
    } else {
      dc = -100;
      dok = false;
    }
    if (dc != s_playTouchDrawChord || dok != s_playTouchDrawOnKey || s_playTouchDrawVoicing) {
      s_playTouchDrawChord = dc;
      s_playTouchDrawOnKey = dok;
      s_playTouchDrawVoicing = false;
      const bool onBezel = pointInRect(d.x, d.y, g_bezelBack) || pointInRect(d.x, d.y, g_bezelSelect) ||
                           pointInRect(d.x, d.y, g_bezelFwd);
      // Avoid full-screen redraw while holding chord/key/bezel — redraw on release only.
      if (!onBezel && ht != -2 && ht < 0) {
        drawPlaySurface();
      }
    }
    wasTouchActive = true;
    return;
  }

  if (touchCount == 0) {
    if (!wasTouchActive) return;
    if (millis() - s_playTouchStartMs < kTapDebounceMs) {
      s_playTouchDrawChord = -100;
      s_playTouchDrawOnKey = false;
      s_playTouchDrawVoicing = false;
      wasTouchActive = false;
      s_playGestureMaxTouches = 0;
      s_playVoicingCombo = false;
      s_playKeyHoldArmed = false;
      playResetBezelFastCycle();
      return;
    }
    const uint8_t gestureMax = s_playGestureMaxTouches;
    const bool hadHighlight = (s_playTouchDrawChord >= 0 ||
                               s_playTouchDrawOnKey ||
                               s_playTouchDrawVoicing);
    s_playTouchDrawChord = -100;
    s_playTouchDrawOnKey = false;
    s_playTouchDrawVoicing = false;
    wasTouchActive = false;
    if (suppressNextPlayTap) {
      suppressNextPlayTap = false;
      if (hadHighlight) {
        playRedrawClearFingerHighlight();
      }
      return;
    }

    if (s_playVoicingDragging) {
      s_playVoicingDragging = false;
      chordVoicingSave(g_chordVoicing);
      s_playVoicingDisplayed = g_chordVoicing;
      drawPlaySurface();
      return;
    }

    if (s_playGestureMaxTouches >= 2 && s_playVoicingCombo && !s_playSelectLatched) {
      if (g_chordVoicing <= 1) {
        g_chordVoicing = 4;
      } else {
        --g_chordVoicing;
      }
      chordVoicingSave(g_chordVoicing);
      s_playGestureMaxTouches = 0;
      s_playVoicingCombo = false;
      s_drawPlayVoicingShift = false;
      drawPlaySurface();
      return;
    }
    s_playGestureMaxTouches = 0;
    s_playVoicingCombo = false;
    s_playKeyHoldArmed = false;
    playResetBezelFastCycle();
    if (s_playBezelLongPressConsumed) {
      s_playBezelLongPressConsumed = false;
      return;
    }

    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;
    const bool openCategoryNow = s_playOpenCategoryOnRelease;
    s_playOpenCategoryOnRelease = false;
    if (pointInRect(hx, hy, g_bezelBack)) {
      s_playSelectLatched = false;
      s_playVoicingPanelOpen = false;
      navigateMainRing(-1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      s_playSelectLatched = false;
      s_playVoicingPanelOpen = false;
      navigateMainRing(1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      if (s_shiftTriggeredThisHold) {
        s_shiftTriggeredThisHold = false;
        drawPlaySurface();
        return;
      }
      if (gestureMax <= 1) {
        s_playSelectLatched = !s_playSelectLatched;
        if (!s_playSelectLatched) {
          s_playVoicingPanelOpen = false;
        }
      }
      drawPlaySurface();
      return;
    }

    const int hit = hitTestPlay(hx, hy);
    if (hit == -2) {
      if (openCategoryNow && !s_playSelectLatched) {
        s_playCategoryPage = PlayCategoryPage::History;
        g_screen = Screen::PlayCategory;
        drawPlayCategorySurface();
        return;
      }
      if (s_playSelectLatched) {
        panicForTrigger(PanicTrigger::KeyPickerTransition);
        g_pickTonic = g_model.keyIndex;
        g_pickMode = g_model.mode;
        g_screen = Screen::KeyPicker;
        s_playSelectLatched = false;
        s_playVoicingPanelOpen = false;
        drawKeyPicker();
        return;
      }
      {
        const uint8_t mch = static_cast<uint8_t>(g_settings.midiOutChannel - 1);
        const uint8_t vel = applyOutputVelocityCurve(g_settings.outputVelocity);
        uint8_t vcap = g_chordVoicing;
        if (vcap < 1) {
          vcap = 1;
        }
        if (vcap > 4) {
          vcap = 4;
        }
        if (g_model.heartAvailable) {
          const int poolIdx = g_model.surprisePeekIndex();
          g_model.nextSurprise();
          g_model.consumeHeart();
          midiPlaySurprisePad(g_model, poolIdx, mch, vel, vcap, g_settings.arpeggiatorMode,
                              g_settings.transposeSemitones);
        } else {
          g_model.registerPlay();
          midiPlayTonicChord(g_model, mch, vel, vcap, g_settings.arpeggiatorMode, g_settings.transposeSemitones);
        }
      }
      g_lastPlayedOutline = -2;
      transportSetLiveChord(-1);
      playRedrawAfterOutlineChange();
      return;
    }
    if (hit >= 0 && hit < ChordModel::kSurroundCount) {
      if (s_shiftActive) {
        const uint8_t ch0 = static_cast<uint8_t>((g_settings.midiOutChannel - 1U) & 0x0F);
        if (hit < 4) {
          midiSendControlChange(ch0, kShiftPadCcMap[hit], 127);
        } else {
          midiSendProgramChange(ch0, kShiftPadProgramMap[hit - 4]);
        }
        drawPlaySurface();
        return;
      }
      if (s_playVoicingPanelOpen && hit != 0) {
        s_playVoicingPanelOpen = false;
        chordVoicingSave(g_chordVoicing);
      }
      if (s_playSelectLatched) {
        if (hit == 0) {
          s_playVoicingPanelOpen = !s_playVoicingPanelOpen;
          if (s_playVoicingPanelOpen) {
            s_playVoicingDisplayed = g_chordVoicing;
          } else {
            chordVoicingSave(g_chordVoicing);
          }
          drawPlaySurface();
          return;
        }
        if (hit == 2) {
          int v = static_cast<int>(g_settings.transposeSemitones) + 1;
          if (v > 24) v = 24;
          g_settings.transposeSemitones = static_cast<int8_t>(v);
          settingsSave(g_settings);
          drawPlaySurface();
          return;
        }
        if (hit == 6) {
          int v = static_cast<int>(g_settings.transposeSemitones) - 1;
          if (v < -24) v = -24;
          g_settings.transposeSemitones = static_cast<int8_t>(v);
          settingsSave(g_settings);
          drawPlaySurface();
          return;
        }
      }
      playTriggerSurroundByIdx(hit);
      playRedrawAfterOutlineChange();
      return;
    }
    if (hadHighlight) {
      playRedrawClearFingerHighlight();
    }
  }
}

void processPlayCategoryTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);
  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
  }

  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    if (d.isPressed()) {
      if (pointInRect(d.x, d.y, g_bezelBack)) {
        wasTouchActive = true;
        playHandleBezelFastCycle(-1);
        return;
      }
      if (pointInRect(d.x, d.y, g_bezelFwd)) {
        wasTouchActive = true;
        playHandleBezelFastCycle(1);
        return;
      }
      playResetBezelFastCycle();
    }
    const int cell = hitTestPlayCategoryCell(g_lastTouchX, g_lastTouchY);
    drawPlayCategorySurface(cell);
    wasTouchActive = true;
    return;
  }

  if (touchCount > 1) {
    playResetBezelFastCycle();
    wasTouchActive = true;
    return;
  }

  if (touchCount == 0) {
    if (!wasTouchActive) return;
    if (millis() - s_playCategoryTouchStartMs < kTapDebounceMs) {
      wasTouchActive = false;
      playResetBezelFastCycle();
      return;
    }
    wasTouchActive = false;
    playResetBezelFastCycle();
    if (s_playBezelLongPressConsumed) {
      s_playBezelLongPressConsumed = false;
      return;
    }
    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;
    if (pointInRect(hx, hy, g_bezelBack)) {
      playCategoryStep(-1);
      drawPlayCategorySurface();
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      playCategoryStep(1);
      drawPlayCategorySurface();
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      g_screen = Screen::Play;
      drawPlaySurface();
      return;
    }
    const int cell = hitTestPlayCategoryCell(hx, hy);
    if (cell >= 0 && cell < s_playCategoryCount) {
      playTriggerSurroundByIdx(static_cast<int>(s_playCategoryItems[cell]));
      drawPlayCategorySurface();
      return;
    }
    drawPlayCategorySurface();
  }
}

// =====================================================================
//  KEY + MODE PICKER (full-screen “dropdown”)
// =====================================================================

void drawKeyPicker() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillScreen(g_uiPalette.bg);

  M5.Display.setFont(nullptr);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Key & mode", w / 2, 4);

  constexpr int pad = 4;
  const int keyTop = 28;
  const int kw = (w - pad * 5) / 4;
  const int kh = 30;
  for (int i = 0; i < ChordModel::kKeyCount; ++i) {
    const int col = i % 4;
    const int row = i / 4;
    const int x = pad + col * (kw + pad);
    const int y = keyTop + row * (kh + pad);
    g_keyPickCells[i] = {x, y, kw, kh};
    const bool sel = (i == g_pickTonic);
    uint16_t bg = sel ? g_uiPalette.keyPickSelected : g_uiPalette.panelMuted;
    drawRoundedButton(g_keyPickCells[i], bg, ChordModel::kKeyNames[i], 2);
  }

  const int modeTop = keyTop + 3 * (kh + pad) + pad;
  const int modeH = 28;
  const int modeGap = 4;
  const int modeW = (w - pad * 6) / 5;
  for (int m = 0; m < static_cast<int>(KeyMode::kCount); ++m) {
    const int x = pad + m * (modeW + modeGap);
    g_modePickCells[m] = {x, modeTop, modeW, modeH};
    const bool sel = (static_cast<int>(g_pickMode) == m);
    uint16_t bg = sel ? g_uiPalette.keyPickSelected : g_uiPalette.panelMuted;
    drawRoundedButton(g_modePickCells[m], bg, ChordModel::modeShortLabel(static_cast<KeyMode>(m)),
                      1);
  }

  const int btnH = 36;
  const int btnY = h - btnH - 8;
  const int btnW = (w - pad * 3) / 2;
  g_keyPickCancel = {pad, btnY, btnW, btnH};
  g_keyPickDone = {w - pad - btnW, btnY, btnW, btnH};
  drawRoundedButton(g_keyPickCancel, g_uiPalette.keyPickCancel, "Cancel", 2);
  drawRoundedButton(g_keyPickDone, g_uiPalette.keyPickDone, "Done", 2);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.setTextDatum(bottom_center);
  M5.Display.drawString("Pick tonic, mode, then Done", w / 2, btnY - 4);
  M5.Display.endWrite();
}

void processKeyPickerTouch(uint8_t touchCount, int w, int h) {
  (void)w;
  (void)h;
  if (touchCount == 0) {
    if (wasTouchActive) {
      wasTouchActive = false;
      auto last = M5.Touch.getDetail();
      const int px = last.x;
      const int py = last.y;

      if (pointInRect(px, py, g_keyPickDone)) {
        panicForTrigger(PanicTrigger::KeyPickerTransition);
        g_model.setTonicAndMode(g_pickTonic, g_pickMode);
        chordStateSave(g_model);
        seqPatternSave(g_seqPattern);
        g_lastAction = "Key saved";
        g_screen = Screen::Play;
        drawPlaySurface();
        return;
      }
      if (pointInRect(px, py, g_keyPickCancel)) {
        panicForTrigger(PanicTrigger::KeyPickerTransition);
        g_screen = Screen::Play;
        drawPlaySurface();
        return;
      }
      for (int i = 0; i < ChordModel::kKeyCount; ++i) {
        if (pointInRect(px, py, g_keyPickCells[i])) {
          g_pickTonic = i;
          drawKeyPicker();
          return;
        }
      }
      for (int m = 0; m < static_cast<int>(KeyMode::kCount); ++m) {
        if (pointInRect(px, py, g_modePickCells[m])) {
          g_pickMode = static_cast<KeyMode>(m);
          drawKeyPicker();
          return;
        }
      }
      drawKeyPicker();
    }
    return;
  }

  if (!wasTouchActive) wasTouchActive = true;
}

// =====================================================================
//  SETTINGS (preserved from previous implementation)
// =====================================================================

void applyProjectNameFromEditor() {
  int start = 0;
  while (start < kProjEditLen && g_projEditBuf[start] == ' ') {
    ++start;
  }
  int end = kProjEditLen - 1;
  while (end > start && g_projEditBuf[end] == ' ') {
    --end;
  }
  char tmp[48];
  int j = 0;
  for (int i = start; i <= end && j < 47; ++i) {
    tmp[j++] = g_projEditBuf[i];
  }
  tmp[j] = '\0';
  strncpy(g_projectCustomName, tmp, 47);
  g_projectCustomName[47] = '\0';
  projectCustomNameSave(g_projectCustomName);
}

void openProjectNameEditor() {
  panicForTrigger(PanicTrigger::ProjectNameTransition);
  memset(g_projEditBuf, ' ', static_cast<size_t>(kProjEditLen));
  g_projEditBuf[kProjEditLen] = '\0';
  const size_t len = strlen(g_projectCustomName);
  const size_t copyLen = len > static_cast<size_t>(kProjEditLen) ? static_cast<size_t>(kProjEditLen) : len;
  memcpy(g_projEditBuf, g_projectCustomName, copyLen);
  g_projEditCursor = 0;
  g_screen = Screen::ProjectNameEdit;
  drawProjectNameEdit();
}

void resolveBackupFolder(char out[48]) {
  if (g_projectCustomName[0] != '\0') {
    strncpy(out, g_projectCustomName, 47);
    out[47] = '\0';
    sdBackupSanitizeFolderName(out);
    if (out[0] == '\0') {
      sdBackupFormatDefaultProjectFolder(out, 48, g_model, g_projectBpm);
    }
  } else {
    sdBackupFormatDefaultProjectFolder(out, 48, g_model, g_projectBpm);
  }
}

void applySdRestoreFromFolder(const char* folder) {
  uint16_t bpm = g_projectBpm;
  if (!sdBackupReadAll(g_settings, g_model, g_seqPattern, g_seqMidiCh, &g_xyCcA, &g_xyCcB,
                       &g_xyOutChannel, &g_xyCurveA, &g_xyCurveB, &bpm, &g_chordVoicing, &g_seqExtras,
                       folder)) {
    panicForTrigger(PanicTrigger::ErrorPath);
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Restore failed");
    return;
  }
  g_projectBpm = bpm;
  projectBpmSave(g_projectBpm);
  settingsSave(g_settings);
  chordStateSave(g_model);
  seqPatternSave(g_seqPattern);
  seqLaneChannelsSave(g_seqMidiCh);
  chordVoicingSave(g_chordVoicing);
  seqExtrasSave(&g_seqExtras);
  xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
  lastProjectFolderSave(folder);
  applyBrightness();
  snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Restored OK");
}

static const char kNameCycle[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

void cycleProjectNameChar() {
  char c = g_projEditBuf[g_projEditCursor];
  const char* p = strchr(kNameCycle, c);
  int idx = p ? static_cast<int>(p - kNameCycle) : 0;
  idx = (idx + 1) % static_cast<int>(strlen(kNameCycle));
  g_projEditBuf[g_projEditCursor] = kNameCycle[idx];
}

void layoutProjectNameButtons(int w, int h) {
  const int btnH = 28;
  const int btnY = h - kBezelBarH - btnH - 6;
  const int gap = 6;
  const int bw = (w - gap * 4) / 3;
  g_peSave = {gap, btnY, bw, btnH};
  g_peClear = {gap * 2 + bw, btnY, bw, btnH};
  g_peCancel = {gap * 3 + bw * 2, btnY, bw, btnH};
}

void drawProjectNameEdit() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillScreen(g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString("Project folder name", w / 2, 4);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("empty = auto (date_key_mode_bpm)", w / 2, 18);

  const int baseY = 40;
  int cx = 8;
  for (int i = 0; i < kProjEditLen; ++i) {
    char ch[2] = {g_projEditBuf[i], '\0'};
    if (i == g_projEditCursor) {
      M5.Display.fillRect(cx - 1, baseY - 2, 10, 14, g_uiPalette.rowSelect);
      M5.Display.setTextColor(TFT_BLACK, g_uiPalette.rowSelect);
    } else {
      M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
    }
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString(ch, cx, baseY);
    cx += 11;
  }

  layoutProjectNameButtons(w, h);
  drawRoundedButton(g_peSave, g_uiPalette.settingsBtnIdle, "Save", 1);
  drawRoundedButton(g_peClear, g_uiPalette.settingsBtnIdle, "Clear", 1);
  drawRoundedButton(g_peCancel, g_uiPalette.settingsBtnIdle, "Cancel", 1);

  layoutBottomBezels(w, h);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("BACK/FWD=cursor SEL=char", w / 2, g_peSave.y - 10);
  drawBezelBarStrip();
  M5.Display.endWrite();
}

void processProjectNameEditTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);
  layoutProjectNameButtons(w, h);

  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
  }

  if (touchCount == 0) {
    if (!wasTouchActive) return;
    wasTouchActive = false;
    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;
    if (pointInRect(hx, hy, g_peSave)) {
      panicForTrigger(PanicTrigger::ProjectNameTransition);
      applyProjectNameFromEditor();
      g_screen = Screen::Settings;
      drawSettingsUi();
      return;
    }
    if (pointInRect(hx, hy, g_peClear)) {
      memset(g_projEditBuf, ' ', static_cast<size_t>(kProjEditLen));
      g_projEditBuf[kProjEditLen] = '\0';
      g_projEditCursor = 0;
      drawProjectNameEdit();
      return;
    }
    if (pointInRect(hx, hy, g_peCancel)) {
      panicForTrigger(PanicTrigger::ProjectNameTransition);
      g_screen = Screen::Settings;
      drawSettingsUi();
      return;
    }
    if (pointInRect(hx, hy, g_bezelBack)) {
      if (g_projEditCursor > 0) {
        --g_projEditCursor;
      }
      drawProjectNameEdit();
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      if (g_projEditCursor < kProjEditLen - 1) {
        ++g_projEditCursor;
      }
      drawProjectNameEdit();
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      cycleProjectNameChar();
      drawProjectNameEdit();
      return;
    }
    drawProjectNameEdit();
    return;
  }

  wasTouchActive = true;
}

void drawSdProjectPick() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillScreen(g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString("Choose project to restore", w / 2, 4);

  const int rowH = 26;
  const int top = 24;
  for (int i = 0; i < g_sdPickCount && i < 8; ++i) {
    g_sdPickRows[i] = {6, top + i * rowH, w - 12, rowH - 2};
    drawRoundedButton(g_sdPickRows[i], g_uiPalette.panelMuted, g_sdPickNames[i], 1);
  }
  g_sdPickCancel = {6, h - 44, w - 12, 36};
  drawRoundedButton(g_sdPickCancel, g_uiPalette.keyPickDone, "Cancel", 2);
  M5.Display.endWrite();
}

void processSdProjectPickTouch(uint8_t touchCount, int w, int h) {
  (void)w;
  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
    wasTouchActive = true;
    return;
  }
  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;
  if (pointInRect(hx, hy, g_sdPickCancel)) {
    panicForTrigger(PanicTrigger::RestoreTransition);
    g_screen = Screen::Settings;
    drawSettingsUi();
    return;
  }
  for (int i = 0; i < g_sdPickCount && i < 8; ++i) {
    if (pointInRect(hx, hy, g_sdPickRows[i])) {
      panicForTrigger(PanicTrigger::RestoreTransition);
      applySdRestoreFromFolder(g_sdPickNames[i]);
      g_screen = Screen::Settings;
      drawSettingsUi();
      return;
    }
  }
  drawSdProjectPick();
}

static int transportDropOptionCount(int8_t kind) {
  switch (kind) {
    case kTransportDropMidiOut:
      return 16;
    case kTransportDropMidiIn:
      return 17;
    case kTransportDropClock:
      return 4;
    case kTransportDropStrum:
      return kArpeggiatorModeCount;
    default:
      return 0;
  }
}

static int transportDropCurrentIndex(int8_t kind) {
  switch (kind) {
    case kTransportDropMidiOut:
      return static_cast<int>(g_settings.midiOutChannel) - 1;
    case kTransportDropMidiIn:
      return (g_settings.midiInChannelUsb == 0) ? 0 : static_cast<int>(g_settings.midiInChannelUsb);
    case kTransportDropClock:
      return static_cast<int>(g_settings.midiClockSource);
    case kTransportDropStrum:
      return static_cast<int>(g_settings.arpeggiatorMode);
    default:
      return 0;
  }
}

static void transportDropFormatOption(int8_t kind, int opt, char* buf, size_t n) {
  switch (kind) {
    case kTransportDropMidiOut:
      snprintf(buf, n, "Channel %u", static_cast<unsigned>(opt + 1));
      break;
    case kTransportDropMidiIn:
      if (opt == 0) {
        snprintf(buf, n, "OMNI");
      } else {
        snprintf(buf, n, "Channel %u", static_cast<unsigned>(opt));
      }
      break;
    case kTransportDropClock:
      snprintf(buf, n, "%s", midiClockSourceLabel(static_cast<uint8_t>(opt)));
      break;
    case kTransportDropStrum:
      snprintf(buf, n, "%s", arpeggiatorModeLabel(static_cast<uint8_t>(opt)));
      break;
    default:
      buf[0] = '\0';
      break;
  }
}

static void transportDropApply(int8_t kind, int opt) {
  switch (kind) {
    case kTransportDropMidiOut:
      g_settings.midiOutChannel = static_cast<uint8_t>(opt + 1);
      break;
    case kTransportDropMidiIn:
      g_settings.midiInChannelUsb = static_cast<uint8_t>(opt == 0 ? 0 : opt);
      break;
    case kTransportDropClock:
      g_settings.midiClockSource = static_cast<uint8_t>(opt);
      break;
    case kTransportDropStrum:
      g_settings.arpeggiatorMode = static_cast<uint8_t>(opt);
      break;
    default:
      break;
  }
  g_settings.normalize();
  settingsSave(g_settings);
}

static int transportDropVisibleRows(int h) {
  constexpr int kRowH = 38;
  const int listTop = 50;
  const int listBottom = h - kBezelBarH - 22;
  const int maxH = max(kRowH * 3, listBottom - listTop);
  return max(3, maxH / kRowH);
}

static void transportComputeDropLayout(int w, int h, int* outX, int* outY, int* outW, int* rowH,
                                       int* firstOpt, int* visCount, int* totalOpts) {
  *rowH = 38;
  *totalOpts = transportDropOptionCount(s_transportDropKind);
  *visCount = min(*totalOpts, transportDropVisibleRows(h));
  *outW = w - 16;
  *outX = 8;
  *outY = 50;
  *firstOpt = s_transportDropScroll;
  if (*firstOpt + *visCount > *totalOpts) {
    *firstOpt = max(0, *totalOpts - *visCount);
    s_transportDropScroll = *firstOpt;
  }
}

static int transportHitDropdown(int px, int py, int w, int h) {
  int dx, dy, dw, rh, first, vis, tot;
  transportComputeDropLayout(w, h, &dx, &dy, &dw, &rh, &first, &vis, &tot);
  if (px < dx || px >= dx + dw || py < dy || py >= dy + vis * rh) return -1;
  const int row = (py - dy) / rh;
  const int idx = first + row;
  if (idx < 0 || idx >= tot) return -1;
  return idx;
}

static void transportDropScrollInit(int w, int h) {
  if (s_transportDropKind < 0) return;
  const int cnt = transportDropOptionCount(s_transportDropKind);
  const int idx = transportDropCurrentIndex(s_transportDropKind);
  const int vis = transportDropVisibleRows(h);
  int scroll = idx - vis / 2;
  if (scroll < 0) scroll = 0;
  const int maxScroll = max(0, cnt - vis);
  if (scroll > maxScroll) scroll = maxScroll;
  s_transportDropScroll = scroll;
}

static void transportDropDismiss() {
  s_transportDropKind = -1;
  s_transportDropDragLastY = -1;
  s_transportDropFingerScrollCount = 0;
}

static void transportDropOpen(int8_t kind, int w, int h) {
  if (kind < kTransportDropMidiOut || kind > kTransportDropStrum) {
    transportDropDismiss();
    return;
  }
  s_transportDropKind = kind;
  s_transportDropDragLastY = -1;
  s_transportDropFingerScrollCount = 0;
  transportDropScrollInit(w, h);
}

static void layoutTransportRects(int w, int h) {
  constexpr int margin = 8;
  constexpr int row1y = 34;
  constexpr int btnH = 38;
  constexpr int gap = 8;
  const int totalW = w - 2 * margin;
  const int third = (totalW - 2 * gap) / 3;
  g_trPlay = {margin, row1y, third, btnH};
  g_trStop = {margin + third + gap, row1y, third, btnH};
  g_trRec = {margin + 2 * (third + gap), row1y, third, btnH};

  constexpr int subGap = 4;
  const int row2y = row1y + btnH + gap;
  const int smallH = 26;
  const int miniW = (third - subGap) / 2;
  g_trMetro = {margin, row2y, miniW, smallH};
  g_trCntIn = {margin + miniW + subGap, row2y, miniW, smallH};

  const int row3y = row2y + smallH + gap;
  constexpr int midiRowH = 34;
  const int ddw = (totalW - 3 * gap) / 4;
  g_trMidiOutBtn = {margin, row3y, ddw, midiRowH};
  g_trMidiInBtn = {margin + ddw + gap, row3y, ddw, midiRowH};
  g_trSyncBtn = {margin + 2 * (ddw + gap), row3y, ddw, midiRowH};
  g_trStrumBtn = {margin + 3 * (ddw + gap), row3y, ddw, midiRowH};

  const int infoY = row3y + midiRowH + 6;
  const int infoH = h - kBezelBarH - infoY - 4;
  const int halfCol = (totalW - gap) / 2;
  g_trBpmTap = {margin, infoY, halfCol, infoH};
  g_trStepDisplay = {margin + halfCol + gap, infoY, halfCol, infoH};
}

void drawTransportSurface() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutTransportRects(w, h);
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  layoutBottomBezels(w, h);

  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  if (s_transportDropKind >= 0) {
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
    M5.Display.drawString("Transport", w / 2, 2);
    const char* dropTitle = "MIDI out";
    if (s_transportDropKind == kTransportDropMidiIn) {
      dropTitle = "MIDI in USB";
    } else if (s_transportDropKind == kTransportDropClock) {
      dropTitle = "Clock source";
    } else if (s_transportDropKind == kTransportDropStrum) {
      dropTitle = "Default strum";
    }
    M5.Display.drawString(dropTitle, w / 2, 22);

    int dx, dy, dw, rh, first, vis, tot;
    transportComputeDropLayout(w, h, &dx, &dy, &dw, &rh, &first, &vis, &tot);
    for (int i = 0; i < vis; ++i) {
      const int opt = first + i;
      const Rect r = {dx, dy + i * rh, dw, rh - 2};
      char lab[40];
      transportDropFormatOption(s_transportDropKind, opt, lab, sizeof(lab));
      const bool sel = (opt == transportDropCurrentIndex(s_transportDropKind));
      const uint16_t bg = sel ? g_uiPalette.settingsBtnActive : g_uiPalette.panelMuted;
      drawRoundedButton(r, bg, lab, 2);
    }
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
    const int hintY = min(h - kBezelBarH - 52, dy + vis * rh + 2);
    if (tot > vis) {
      M5.Display.drawString("BACK / FWD / drag = scroll list", w / 2, hintY);
      M5.Display.drawString("Tap row to select   SEL: cancel", w / 2, hintY + 12);
    } else {
      M5.Display.drawString("Tap row to select   SEL: cancel", w / 2, hintY);
    }
    drawBezelBarStrip();
    M5.Display.endWrite();
    return;
  }

  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Transport", w / 2, 2);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("BACK/FWD: Transport / Pad / Seq / XY", w / 2, 20);

  const char* playLab = "Play";
  if (transportIsPaused()) {
    playLab = "Play";
  } else if (transportIsPlaying() || transportIsCountIn()) {
    playLab = "Pause";
  }

  drawRoundedButton(g_trPlay, g_uiPalette.settingsBtnActive, playLab, 2);
  drawRoundedButton(g_trStop, g_uiPalette.settingsBtnIdle, "Stop", 2);
  char rlab[16];
  snprintf(rlab, sizeof(rlab), "Rec %s", transportRecordArmed() ? "ARM" : "");
  drawRoundedButton(g_trRec, transportRecordArmed() ? g_uiPalette.danger : g_uiPalette.panelMuted, rlab,
                    2);

  char ml[20];
  char cl[24];
  snprintf(ml, sizeof(ml), "M %s", g_prefsMetronome ? "on" : "off");
  snprintf(cl, sizeof(cl), "C %s", g_prefsCountIn ? "on" : "off");
  drawRoundedButton(g_trMetro, g_prefsMetronome ? g_uiPalette.seqLaneTab : g_uiPalette.panelMuted, ml,
                    1);
  drawRoundedButton(g_trCntIn, g_prefsCountIn ? g_uiPalette.seqLaneTab : g_uiPalette.panelMuted, cl,
                    1);

  char mo[20];
  char mi[20];
  char sy[24];
  char st[24];
  snprintf(mo, sizeof(mo), "Out %u", (unsigned)g_settings.midiOutChannel);
  if (g_settings.midiInChannelUsb == 0) {
    snprintf(mi, sizeof(mi), "InUSB OMNI");
  } else {
    snprintf(mi, sizeof(mi), "InUSB %u", (unsigned)g_settings.midiInChannelUsb);
  }
  snprintf(sy, sizeof(sy), "%s", midiClockSourceLabel(g_settings.midiClockSource));
  snprintf(st, sizeof(st), "%s", arpeggiatorModeLabel(g_settings.arpeggiatorMode));
  drawRoundedButton(g_trMidiOutBtn, g_uiPalette.panelMuted, mo, 2);
  drawRoundedButton(g_trMidiInBtn, g_uiPalette.panelMuted, mi, 2);
  drawRoundedButton(g_trSyncBtn, g_uiPalette.panelMuted, sy, 1);
  drawRoundedButton(g_trStrumBtn, g_uiPalette.panelMuted, st, 1);

  M5.Display.drawRoundRect(g_trBpmTap.x, g_trBpmTap.y, g_trBpmTap.w, g_trBpmTap.h, 6,
                           g_uiPalette.settingsBtnBorder);
  M5.Display.drawRoundRect(g_trStepDisplay.x, g_trStepDisplay.y, g_trStepDisplay.w, g_trStepDisplay.h, 6,
                           g_uiPalette.settingsBtnBorder);

  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_center);
  M5.Display.drawString("BPM tap / hold", g_trBpmTap.x + g_trBpmTap.w / 2, g_trBpmTap.y + 2);

  char bpmStr[8];
  snprintf(bpmStr, sizeof(bpmStr), "%u", static_cast<unsigned>(g_projectBpm));
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(bpmStr, g_trBpmTap.x + g_trBpmTap.w / 2, g_trBpmTap.y + g_trBpmTap.h / 2 + 4);

  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("Step 1-16", g_trStepDisplay.x + g_trStepDisplay.w / 2, g_trStepDisplay.y + 2);

  char stepMain[8];
  char stepSub[16];
  if (transportIsCountIn()) {
    const uint8_t cn = transportCountInNumber();
    snprintf(stepMain, sizeof(stepMain), "%u", static_cast<unsigned>(cn));
    snprintf(stepSub, sizeof(stepSub), "cnt in");
  } else if (transportIsPlaying() || transportIsPaused()) {
    const uint8_t au = transportAudibleStep();
    const unsigned disp = static_cast<unsigned>(au) + 1U;
    const unsigned beatInBar = static_cast<unsigned>(au) / 4U + 1U;
    const unsigned subInBeat = static_cast<unsigned>(au) % 4U + 1U;
    snprintf(stepMain, sizeof(stepMain), "%u", disp);
    snprintf(stepSub, sizeof(stepSub), "b%u s%u", beatInBar, subInBeat);
  } else {
    snprintf(stepMain, sizeof(stepMain), "--");
    snprintf(stepSub, sizeof(stepSub), " ");
  }
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(stepMain, g_trStepDisplay.x + g_trStepDisplay.w / 2,
                         g_trStepDisplay.y + g_trStepDisplay.h / 2 - 2);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
  M5.Display.drawString(stepSub, g_trStepDisplay.x + g_trStepDisplay.w / 2,
                         g_trStepDisplay.y + g_trStepDisplay.h / 2 + 22);

  drawBezelBarStrip();
  M5.Display.endWrite();
}

void drawTransportBpmEdit() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutBottomBezels(w, h);
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString("BPM", w / 2, 8);

  char nb[8];
  snprintf(nb, sizeof(nb), "%u", static_cast<unsigned>(g_bpmEditValue));
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(nb, w / 2, 52);

  constexpr int m = 8;
  const int bw = (w - m * 5) / 4;
  const int rowY = 88;
  g_bpmEditM5 = {m, rowY, bw, 32};
  g_bpmEditM1 = {m + (bw + m), rowY, bw, 32};
  g_bpmEditP1 = {m + 2 * (bw + m), rowY, bw, 32};
  g_bpmEditP5 = {m + 3 * (bw + m), rowY, bw, 32};
  drawRoundedButton(g_bpmEditM5, g_uiPalette.panelMuted, "-5", 2);
  drawRoundedButton(g_bpmEditM1, g_uiPalette.panelMuted, "-1", 2);
  drawRoundedButton(g_bpmEditP1, g_uiPalette.panelMuted, "+1", 2);
  drawRoundedButton(g_bpmEditP5, g_uiPalette.panelMuted, "+5", 2);

  const int btnY = rowY + 42;
  const int btnW = (w - 3 * m) / 2;
  g_bpmEditOk = {m, btnY, btnW, 36};
  g_bpmEditCancel = {m * 2 + btnW, btnY, btnW, 36};
  drawRoundedButton(g_bpmEditOk, g_uiPalette.settingsBtnActive, "OK", 2);
  drawRoundedButton(g_bpmEditCancel, g_uiPalette.keyPickCancel, "Cancel", 2);

  drawBezelBarStrip();
  M5.Display.endWrite();
}

void drawTransportTapTempo() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutBottomBezels(w, h);
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString("Tap tempo", w / 2, 8);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("Two or more taps set BPM", w / 2, 30);

  char nb[8];
  snprintf(nb, sizeof(nb), "%u", static_cast<unsigned>(s_tapTempoPreviewBpm));
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString(nb, w / 2, h / 2 - 24);

  g_trTapTempoDone = {8, h - kBezelBarH - 44, w - 16, 36};
  drawRoundedButton(g_trTapTempoDone, g_uiPalette.settingsBtnActive, "Use BPM", 2);

  drawBezelBarStrip();
  M5.Display.endWrite();
}

static void bpmEditAdjust(int delta) {
  int v = static_cast<int>(g_bpmEditValue) + delta;
  if (v < 40) v = 40;
  if (v > 300) v = 300;
  g_bpmEditValue = static_cast<uint16_t>(v);
}

void processTransportBpmEditTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);
  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    const auto& d = M5.Touch.getDetail(0);
    if (d.isPressed()) {
      g_lastTouchX = d.x;
      g_lastTouchY = d.y;
    }
    wasTouchActive = true;
    return;
  }

  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;

  if (pointInRect(hx, hy, g_bpmEditOk)) {
    g_projectBpm = g_bpmEditValue;
    projectBpmSave(g_projectBpm);
    g_screen = Screen::Transport;
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_bpmEditCancel)) {
    g_screen = Screen::Transport;
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_bpmEditM5)) {
    bpmEditAdjust(-5);
    drawTransportBpmEdit();
    return;
  }
  if (pointInRect(hx, hy, g_bpmEditM1)) {
    bpmEditAdjust(-1);
    drawTransportBpmEdit();
    return;
  }
  if (pointInRect(hx, hy, g_bpmEditP1)) {
    bpmEditAdjust(1);
    drawTransportBpmEdit();
    return;
  }
  if (pointInRect(hx, hy, g_bpmEditP5)) {
    bpmEditAdjust(5);
    drawTransportBpmEdit();
    return;
  }
  drawTransportBpmEdit();
}

void processTransportTapTempoTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);
  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    const auto& d = M5.Touch.getDetail(0);
    if (d.isPressed()) {
      g_lastTouchX = d.x;
      g_lastTouchY = d.y;
    }
    wasTouchActive = true;
    return;
  }

  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;
  const uint32_t now = millis();

  if (pointInRect(hx, hy, g_trTapTempoDone)) {
    g_projectBpm = s_tapTempoPreviewBpm;
    projectBpmSave(g_projectBpm);
    g_screen = Screen::Transport;
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_bezelBack) || pointInRect(hx, hy, g_bezelFwd) ||
      pointInRect(hx, hy, g_bezelSelect)) {
    g_screen = Screen::Transport;
    drawTransportSurface();
    return;
  }

  if (s_tapTempoPrevTapMs > 0U) {
    const uint32_t dt = now - s_tapTempoPrevTapMs;
    if (dt >= 200U && dt <= 2500U) {
      uint32_t bpm = 60000UL / dt;
      if (bpm < 40U) bpm = 40U;
      if (bpm > 300U) bpm = 300U;
      s_tapTempoPreviewBpm = static_cast<uint16_t>(bpm);
    }
  }
  s_tapTempoPrevTapMs = now;
  drawTransportTapTempo();
}

void processTransportTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);
  layoutTransportRects(w, h);

  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    const auto& d = M5.Touch.getDetail(0);
    if (d.isPressed()) {
      g_lastTouchX = d.x;
      g_lastTouchY = d.y;
      if (s_transportDropKind >= 0) {
        int ddx, ddy, ddw, drh, dfirst, dvis, dtot;
        transportComputeDropLayout(w, h, &ddx, &ddy, &ddw, &drh, &dfirst, &dvis, &dtot);
        const Rect listHit = {ddx, ddy, ddw, dvis * drh};
        if (pointInRect(d.x, d.y, listHit)) {
          if (s_transportDropDragLastY >= 0) {
            const int delta = d.y - s_transportDropDragLastY;
            if (delta > 22) {
              if (s_transportDropScroll > 0) {
                --s_transportDropScroll;
                ++s_transportDropFingerScrollCount;
                drawTransportSurface();
              }
              s_transportDropDragLastY = d.y;
            } else if (delta < -22) {
              const int cnt = transportDropOptionCount(s_transportDropKind);
              if (s_transportDropScroll + dvis < cnt) {
                ++s_transportDropScroll;
                ++s_transportDropFingerScrollCount;
                drawTransportSurface();
              }
              s_transportDropDragLastY = d.y;
            }
          } else {
            s_transportDropDragLastY = d.y;
          }
        } else {
          s_transportDropDragLastY = -1;
        }
      }
      if (!s_transportTouchDown) {
        s_transportTouchDown = true;
        s_transportTouchStartMs = millis();
        s_transportTouchStartX = d.x;
        s_transportTouchStartY = d.y;
      }
      if (s_transportDropKind < 0 && pointInRect(s_transportTouchStartX, s_transportTouchStartY, g_trBpmTap)) {
        const uint32_t elapsed = millis() - s_transportTouchStartMs;
        drawHoldProgressStrip(HoldProgressOwner::TransportBpm, elapsed, transportBpmLongMs(),
                              "Hold BPM: tap-tempo");
      } else {
        clearHoldProgressStrip(HoldProgressOwner::TransportBpm);
      }
    }
    wasTouchActive = true;
    return;
  }

  if (!wasTouchActive) return;
  wasTouchActive = false;
  clearHoldProgressStrip(HoldProgressOwner::TransportBpm);
  s_transportTouchDown = false;
  s_transportDropDragLastY = -1;
  const int fingerScrollsThisGesture = s_transportDropFingerScrollCount;
  s_transportDropFingerScrollCount = 0;

  const int hx = s_transportTouchStartX;
  const int hy = s_transportTouchStartY;
  const int relX = g_lastTouchX;
  const int relY = g_lastTouchY;
  const uint32_t dur = millis() - s_transportTouchStartMs;
  const uint32_t now = millis();

  if (s_transportDropKind >= 0) {
    if (pointInRect(hx, hy, g_bezelBack)) {
      if (s_transportDropScroll > 0) {
        --s_transportDropScroll;
        drawTransportSurface();
      } else {
        transportDropDismiss();
        drawTransportSurface();
      }
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      const int cnt = transportDropOptionCount(s_transportDropKind);
      const int vis = transportDropVisibleRows(h);
      if (s_transportDropScroll + vis < cnt) {
        ++s_transportDropScroll;
        drawTransportSurface();
      }
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      transportDropDismiss();
      drawTransportSurface();
      return;
    }
    const int pick = transportHitDropdown(relX, relY, w, h);
    if (pick >= 0) {
      if (fingerScrollsThisGesture > 0) {
        drawTransportSurface();
        return;
      }
      transportDropApply(s_transportDropKind, pick);
      transportDropDismiss();
      drawTransportSurface();
      return;
    }
    transportDropDismiss();
    drawTransportSurface();
    return;
  }

  if (pointInRect(hx, hy, g_bezelBack)) {
    navigateMainRing(-1);
    return;
  }
  if (pointInRect(hx, hy, g_bezelFwd)) {
    navigateMainRing(1);
    return;
  }
  if (pointInRect(hx, hy, g_bezelSelect)) {
    drawTransportSurface();
    return;
  }

  if (pointInRect(hx, hy, g_trBpmTap)) {
    transportDropDismiss();
    if (dur >= transportBpmLongMs()) {
      s_tapTempoPrevTapMs = 0;
      s_tapTempoPreviewBpm = g_projectBpm;
      g_screen = Screen::TransportTapTempo;
      drawTransportTapTempo();
      return;
    }
    g_bpmEditValue = g_projectBpm;
    g_screen = Screen::TransportBpmEdit;
    drawTransportBpmEdit();
    return;
  }

  if (pointInRect(hx, hy, g_trMidiOutBtn)) {
    transportDropOpen(kTransportDropMidiOut, w, h);
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_trMidiInBtn)) {
    transportDropOpen(kTransportDropMidiIn, w, h);
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_trSyncBtn)) {
    transportDropOpen(kTransportDropClock, w, h);
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_trStrumBtn)) {
    transportDropOpen(kTransportDropStrum, w, h);
    drawTransportSurface();
    return;
  }

  if (pointInRect(hx, hy, g_trPlay)) {
    if (transportIsPaused()) {
      transportTogglePlayPause(now);
    } else if (transportIsPlaying() || transportIsCountIn()) {
      transportTogglePlayPause(now);
    } else {
      if (transportRecordArmed()) {
        g_screen = Screen::Play;
        transportBeginLiveRecording(now);
        drawPlaySurface();
      } else {
        transportStartMetronomeOnly(now);
        drawTransportSurface();
      }
    }
    return;
  }
  if (pointInRect(hx, hy, g_trStop)) {
    panicForTrigger(PanicTrigger::TransportStop);
    transportStop();
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_trRec)) {
    transportSetRecordArmed(!transportRecordArmed());
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_trMetro)) {
    g_prefsMetronome = !g_prefsMetronome;
    transportPrefsSave();
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_trCntIn)) {
    g_prefsCountIn = !g_prefsCountIn;
    transportPrefsSave();
    drawTransportSurface();
    return;
  }
  drawTransportSurface();
}

// ----- Settings UI (sections, large type, dropdowns, bottom bezel bar) -----

static const uint8_t kSetMidi[] = {0, 1, 18, 19, 17, 2, 3, 37, 4, 22, 23, 21, 20, 24, 25, 26, 27, 28, 29, 33, 35, 34, 36, 30, 31, 32};
static const uint8_t kSetDisplay[] = {5, 8, 9, 7};
static const uint8_t kSetSeq[] = {6, 43, 38, 39, 40, 41, 42, 10, 11, 12};
static const uint8_t kSetStorage[] = {13, 14, 15, 16};

static int settingsPanelRowCount(SettingsPanel p) {
  switch (p) {
    case SettingsPanel::Midi:
      return 26;
    case SettingsPanel::Display:
      return 4;
    case SettingsPanel::SeqArp:
      return 10;
    case SettingsPanel::Storage:
      return 4;
    default:
      return 0;
  }
}

static uint8_t settingsPanelRowId(SettingsPanel p, int idx) {
  const uint8_t* t = nullptr;
  int c = 0;
  switch (p) {
    case SettingsPanel::Midi:
      t = kSetMidi;
      c = 26;
      break;
    case SettingsPanel::Display:
      t = kSetDisplay;
      c = 4;
      break;
    case SettingsPanel::SeqArp:
      t = kSetSeq;
      c = 10;
      break;
    case SettingsPanel::Storage:
      t = kSetStorage;
      c = 4;
      break;
    default:
      return 0;
  }
  if (idx < 0 || idx >= c) return t[0];
  return t[idx];
}

static void settingsSyncRowFromCursor() {
  if (g_settingsPanel == SettingsPanel::Menu) return;
  int n = settingsPanelRowCount(g_settingsPanel);
  int i = g_settingsCursorRow;
  if (i < 0) i = 0;
  if (i >= n) i = n - 1;
  g_settingsRow = settingsPanelRowId(g_settingsPanel, i);
}

static void settingsEnsureListScroll(int nRows, int rowH, int contentTop, int contentBottom) {
  int visible = (contentBottom - contentTop) / rowH;
  if (visible < 1) visible = 1;
  if (g_settingsCursorRow < g_settingsListScroll) {
    g_settingsListScroll = g_settingsCursorRow;
  }
  if (g_settingsCursorRow >= g_settingsListScroll + visible) {
    g_settingsListScroll = g_settingsCursorRow - visible + 1;
  }
  int maxScroll = max(0, nRows - visible);
  if (g_settingsListScroll > maxScroll) g_settingsListScroll = maxScroll;
  if (g_settingsListScroll < 0) g_settingsListScroll = 0;
}

static void settingsMoveCursorInSection(int delta, int h) {
  int n = settingsPanelRowCount(g_settingsPanel);
  if (n <= 0) return;
  g_settingsCursorRow = (g_settingsCursorRow + delta + n * 100) % n;
  settingsSyncRowFromCursor();
  constexpr int kRowH = 36;
  constexpr int kTop = 42;
  int fb = (g_settingsFeedback[0] != '\0') ? 20 : 4;
  int contentBottom = h - kBezelBarH - fb;
  settingsEnsureListScroll(n, kRowH, kTop, contentBottom);
}

static int settingsDropdownOptionCount(int8_t rid) {
  switch (rid) {
    case 0:
      return 16;
    case 1:
    case 18:
    case 19:
      return 17;
    case 2:
    case 3:
    case 4:
      return 4;
    case 37:
      return 8;
    case 5:
      return 10;
    case 6:
      return 9;
    case 43:
      return 3;
    case 38:
      return 11;
    case 39:
      return 11;
    case 40:
    case 41:
      return 5;
    case 42:
      return 4;
    case 8:
      return kUiThemeCount;
    case 9:
      return 11;
    case 10:
      return kArpeggiatorModeCount;
    case 11:
      return 33;
    case 17:
      return 49;
    case 13:
      return 2;
    case 21:
    case 22:
    case 23:
    case 25:
      return 2;
    case 26:
      return 3;
    case 27:
      return 2;
    case 28:
      return 4;
    case 29:
      return 4;
    case 30:
    case 31:
      return 5;
    case 33:
    case 35:
      return 2;
    default:
      return 0;
  }
}

static int settingsDropVisibleRows(int h) {
  const int rowH = 20;
  const int maxH = h - kBezelBarH - 52;
  return max(3, maxH / rowH);
}

static int settingsDropdownScrollForCurrent(int8_t rid, int h) {
  int cnt = settingsDropdownOptionCount(rid);
  if (cnt <= 0) return 0;
  int idx = 0;
  switch (rid) {
    case 0:
      idx = g_settings.midiOutChannel - 1;
      break;
    case 1:
      idx = (g_settings.midiInChannelUsb == 0) ? 0 : g_settings.midiInChannelUsb;
      break;
    case 18:
      idx = (g_settings.midiInChannelBle == 0) ? 0 : g_settings.midiInChannelBle;
      break;
    case 19:
      idx = (g_settings.midiInChannelDin == 0) ? 0 : g_settings.midiInChannelDin;
      break;
    case 2:
      idx = g_settings.midiTransportSend;
      break;
    case 3:
      idx = g_settings.midiTransportReceive;
      break;
    case 4:
      idx = g_settings.midiClockSource;
      break;
    case 37:
      idx = static_cast<int>(g_settings.midiThruMask & 0x07U);
      break;
    case 5:
      idx = static_cast<int>(g_settings.brightnessPercent) / 10 - 1;
      if (idx < 0) idx = 0;
      if (idx > 9) idx = 9;
      break;
    case 6: {
      uint8_t v = g_settings.outputVelocity;
      if (v <= 40) {
        idx = 0;
      } else if (v >= 120) {
        idx = 8;
      } else {
        idx = (static_cast<int>(v) - 40 + 5) / 10;
        if (idx > 8) idx = 8;
      }
      break;
    }
    case 43:
      idx = static_cast<int>(g_settings.velocityCurve);
      if (idx < 0) idx = 0;
      if (idx > 2) idx = 0;
      break;
    case 38:
      idx = static_cast<int>(g_settings.globalSwingPct) / 10;
      if (idx < 0) idx = 0;
      if (idx > 10) idx = 10;
      break;
    case 39:
      idx = static_cast<int>(g_settings.globalHumanizePct) / 10;
      if (idx < 0) idx = 0;
      if (idx > 10) idx = 10;
      break;
    case 40:
      idx = static_cast<int>(g_settings.settingsEntryHoldPreset);
      if (idx < 0) idx = 0;
      if (idx > 4) idx = 2;
      break;
    case 41:
      idx = static_cast<int>(g_settings.seqLongPressPreset);
      if (idx < 0) idx = 0;
      if (idx > 4) idx = 2;
      break;
    case 42:
      idx = static_cast<int>(g_settings.bpmHoldPreset);
      if (idx < 0) idx = 0;
      if (idx > 3) idx = 1;
      break;
    case 8:
      idx = g_uiTheme;
      break;
    case 9:
      idx = static_cast<int>(g_clickVolumePercent) / 10;
      if (idx > 10) idx = 10;
      break;
    case 10:
      idx = g_settings.arpeggiatorMode;
      break;
    case 11:
      idx = (static_cast<int>(g_projectBpm) - 40) / 5;
      if (idx < 0) idx = 0;
      if (idx > 32) idx = 32;
      break;
    case 17:
      idx = static_cast<int>(g_settings.transposeSemitones) + 24;
      if (idx < 0) idx = 0;
      if (idx > 48) idx = 48;
      break;
    case 21:
      idx = g_settings.midiDebugEnabled ? 1 : 0;
      break;
    case 22:
      idx = g_settings.clkFollow ? 1 : 0;
      break;
    case 23:
      idx = g_settings.clkFlashEdge ? 1 : 0;
      break;
    case 25:
      idx = g_settings.playInMonitor ? 1 : 0;
      break;
    case 26:
      idx = static_cast<int>(g_settings.playInClockHold);
      if (idx < 0) idx = 0;
      if (idx > 2) idx = 1;
      break;
    case 27:
      idx = g_settings.playInMonitorMode ? 1 : 0;
      break;
    case 28:
      idx = static_cast<int>(g_settings.bleDebugPeakDecay);
      if (idx < 0) idx = 0;
      if (idx > 3) idx = 2;
      break;
    case 29:
      idx = static_cast<int>(g_settings.panicDebugRotate);
      if (idx < 0) idx = 0;
      if (idx > 3) idx = 2;
      break;
    case 30:
      idx = static_cast<int>(g_settings.suggestStableWindow);
      if (idx < 0) idx = 0;
      if (idx > 4) idx = 2;
      break;
    case 31:
      idx = static_cast<int>(g_settings.suggestStableGap);
      if (idx < 0) idx = 0;
      if (idx > 4) idx = 2;
      break;
    case 33:
      idx = g_settings.suggestProfile ? 1 : 0;
      break;
    case 35:
      idx = g_settings.suggestProfileLock ? 1 : 0;
      break;
    default:
      return 0;
  }
  int vis = settingsDropVisibleRows(h);
  int scroll = idx - vis / 2;
  if (scroll < 0) scroll = 0;
  int maxScroll = max(0, cnt - vis);
  if (scroll > maxScroll) scroll = maxScroll;
  return scroll;
}

static void settingsFormatDropdownOption(int8_t rid, int opt, char* buf, size_t n) {
  switch (rid) {
    case 0:
      snprintf(buf, n, "Channel %u", static_cast<unsigned>(opt + 1));
      break;
    case 1:
    case 18:
    case 19:
      if (opt == 0) {
        snprintf(buf, n, "OMNI");
      } else {
        snprintf(buf, n, "Channel %u", static_cast<unsigned>(opt));
      }
      break;
    case 2:
    case 3:
      snprintf(buf, n, "%s", midiTransportRouteLabel(static_cast<uint8_t>(opt)));
      break;
    case 4:
      snprintf(buf, n, "%s", midiClockSourceLabel(static_cast<uint8_t>(opt)));
      break;
    case 37:
      switch (opt) {
        case 0:
          snprintf(buf, n, "Off");
          break;
        case 1:
          snprintf(buf, n, "USB");
          break;
        case 2:
          snprintf(buf, n, "BLE");
          break;
        case 3:
          snprintf(buf, n, "USB+BLE");
          break;
        case 4:
          snprintf(buf, n, "DIN");
          break;
        case 5:
          snprintf(buf, n, "USB+DIN");
          break;
        case 6:
          snprintf(buf, n, "BLE+DIN");
          break;
        default:
          snprintf(buf, n, "All");
          break;
      }
      break;
    case 5:
      snprintf(buf, n, "%u%%", static_cast<unsigned>((opt + 1) * 10));
      break;
    case 6:
      snprintf(buf, n, "%u", static_cast<unsigned>(40 + opt * 10));
      break;
    case 43:
      snprintf(buf, n, "%s", opt == 0 ? "Linear" : (opt == 1 ? "Soft" : "Hard"));
      break;
    case 38:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(opt * 10));
      break;
    case 39:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(opt * 10));
      break;
    case 40:
      snprintf(buf, n, "%s", opt == 0 ? "0.5s" :
                             (opt == 1 ? "0.65s" : (opt == 2 ? "0.8s" : (opt == 3 ? "1.0s" : "1.2s"))));
      break;
    case 41:
      snprintf(buf, n, "%s", opt == 0 ? "0.25s" :
                             (opt == 1 ? "0.3s" : (opt == 2 ? "0.4s" : (opt == 3 ? "0.5s" : "0.7s"))));
      break;
    case 42:
      snprintf(buf, n, "%s", opt == 0 ? "0.35s" : (opt == 1 ? "0.5s" : (opt == 2 ? "0.7s" : "0.9s")));
      break;
    case 8:
      snprintf(buf, n, "%s", uiThemeName(static_cast<uint8_t>(opt)));
      break;
    case 9:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(opt * 10));
      break;
    case 10:
      snprintf(buf, n, "%s", arpeggiatorModeLabel(static_cast<uint8_t>(opt)));
      break;
    case 11:
      snprintf(buf, n, "%u BPM", static_cast<unsigned>(40 + opt * 5));
      break;
    case 17: {
      const int v = opt - 24;
      if (v > 0) {
        snprintf(buf, n, "+%d", v);
      } else {
        snprintf(buf, n, "%d", v);
      }
      break;
    }
    case 13:
      snprintf(buf, n, "%s", opt == 0 ? "Cancel" : "Erase everything");
      break;
    case 21:
      snprintf(buf, n, "%s", opt == 0 ? "Off" : "On");
      break;
    case 22:
      snprintf(buf, n, "%s", opt == 0 ? "Off" : "On");
      break;
    case 23:
      snprintf(buf, n, "%s", opt == 0 ? "Quarter" : "Eighth");
      break;
    case 25:
      snprintf(buf, n, "%s", opt == 0 ? "Off" : "On");
      break;
    case 26:
      snprintf(buf, n, "%s", opt == 0 ? "Short (0.6s)" : (opt == 1 ? "Medium (1.2s)" : "Long (2.0s)"));
      break;
    case 27:
      snprintf(buf, n, "%s", opt == 0 ? "Compact" : "Detailed");
      break;
    case 28:
      snprintf(buf, n, "%s", opt == 0 ? "Off" :
                             (opt == 1 ? "5 seconds" : (opt == 2 ? "10 seconds" : "30 seconds")));
      break;
    case 29:
      snprintf(buf, n, "%s", opt == 0 ? "Off" : (opt == 1 ? "1 second" : (opt == 2 ? "2 seconds" : "4 seconds")));
      break;
    case 30:
      snprintf(buf, n, "%s", opt == 0
                               ? "0.6s"
                               : (opt == 1 ? "1.0s" : (opt == 2 ? "1.4s" : (opt == 3 ? "2.0s" : "3.0s"))));
      break;
    case 31:
      snprintf(buf, n, "%s", opt == 0
                               ? "8"
                               : (opt == 1 ? "12" : (opt == 2 ? "18" : (opt == 3 ? "24" : "32"))));
      break;
    case 33:
      snprintf(buf, n, "%s", opt == 0 ? "A (balanced)" : "B (sticky)");
      break;
    case 35:
      snprintf(buf, n, "%s", opt == 0 ? "Off" : "On");
      break;
    default:
      buf[0] = '\0';
      break;
  }
}

static void settingsRunFactoryReset() {
  panicForTrigger(PanicTrigger::FactoryReset);
  factoryResetAll(g_settings, g_model);
  memset(g_seqPattern, kSeqRest, sizeof(g_seqPattern));
  seqExtrasLoad(&g_seqExtras);
  seqLaneChannelsLoad(g_seqMidiCh);
  chordVoicingLoad(&g_chordVoicing);
  g_seqLane = 0;
  xyMappingLoad(&g_xyOutChannel, &g_xyCcA, &g_xyCcB, &g_xyCurveA, &g_xyCurveB);
  projectBpmLoad(&g_projectBpm);
  projectCustomNameLoad(g_projectCustomName);
  lastProjectFolderLoad(g_lastProjectFolder);
  uiThemeLoad(&g_uiTheme);
  uiThemeApply(g_uiTheme);
  transportPrefsLoad();
  transportApplyClickVolume();
  g_factoryResetConfirmArmed = false;
  applyBrightness();
}

static void settingsRunBackup() {
  char folder[48];
  resolveBackupFolder(folder);
  const uint16_t bm = g_projectBpm;
  if (sdBackupWriteAll(g_settings, g_model, g_seqPattern, g_seqMidiCh, g_xyOutChannel, g_xyCcA, g_xyCcB,
                       g_xyCurveA, g_xyCurveB, bm, g_chordVoicing, &g_seqExtras, folder)) {
    lastProjectFolderSave(folder);
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "OK %s/%s", SD_BACKUP_ROOT, folder);
  } else {
    panicForTrigger(PanicTrigger::ErrorPath);
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SD backup failed");
  }
}

static void settingsRunRestoreFlow() {
  char dirs[8][48];
  int n = 0;
  if (!sdBackupListProjects(dirs, 8, &n)) {
    panicForTrigger(PanicTrigger::ErrorPath);
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SD list failed");
    return;
  }
  if (n == 0) {
    panicForTrigger(PanicTrigger::ErrorPath);
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "No projects in " SD_BACKUP_ROOT);
    return;
  }
  if (n == 1) {
    panicForTrigger(PanicTrigger::RestoreTransition);
    applySdRestoreFromFolder(dirs[0]);
    return;
  }
  panicForTrigger(PanicTrigger::RestoreTransition);
  g_sdPickCount = n;
  for (int i = 0; i < n && i < 8; ++i) {
    strncpy(g_sdPickNames[i], dirs[i], 47);
    g_sdPickNames[i][47] = '\0';
  }
  g_screen = Screen::SdProjectPick;
  drawSdProjectPick();
}

static void settingsRunClearMidiDebugData() {
  s_midiEventHistory.clear();
  s_midiInState.reset();
  bleMidiResetRxBytes();
  s_bleDbgRatePrevMs = 0;
  s_bleDbgRatePrevRx = 0;
  s_bleDbgRateBps = 0;
  s_bleDbgRatePeakBps = 0;
  s_bleDbgLastActivityMs = 0;
  s_bleDbgPeakHeld = true;
  dinMidiResetRxBytes();
  s_dinDbgRatePrevMs = 0;
  s_dinDbgRatePrevRx = 0;
  s_dinDbgRateBps = 0;
  s_dinDbgRatePeakBps = 0;
  s_dinDbgLastActivityMs = 0;
  s_dinDbgPeakHeld = true;
  midiOutResetTxStats();
  s_outDbgRatePrevMs = 0;
  s_outDbgRatePrevTx = 0;
  s_outDbgRateBps = 0;
  s_outDbgRatePeakBps = 0;
  midiIngressResetQueueDropTotals();
  s_bleDbgPrevDrop = 0;
  s_bleDbgWarnUntilMs = 0;
  s_midiDetectedChord[0] = '\0';
  s_midiDetectedChordUntilMs = 0;
  s_midiDetectedSuggest[0] = '\0';
  s_midiDetectedSuggestUntilMs = 0;
  s_midiSuggestLastAtMs = 0;
  for (int i = 0; i < 3; ++i) {
    s_midiSuggestTop3[i][0] = '\0';
    s_midiSuggestTop3Score[i] = 0;
  }
  s_midiSuggestTop3UntilMs = 0;
  s_midiSuggestStableUntilMs = 0;
  s_playIngressInfo[0] = '\0';
  s_playIngressInfoUntilMs = 0;
  s_playIngressLastMusicalMs = 0;
  s_playIngressInfoEventMs = 0;
  for (size_t i = 0; i < static_cast<size_t>(PanicTrigger::Count); ++i) {
    s_panicTriggerCounts[i] = 0;
  }
  snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "MIDI debug data cleared");
}

static void settingsSaveAndExit() {
  panicForTrigger(PanicTrigger::SettingsSaveExit);
  g_settings.normalize();
  settingsSave(g_settings);
  chordStateSave(g_model);
  seqPatternSave(g_seqPattern);
  seqExtrasSave(&g_seqExtras);
  seqLaneChannelsSave(g_seqMidiCh);
  chordVoicingSave(g_chordVoicing);
  xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
  projectBpmSave(g_projectBpm);
  projectCustomNameSave(g_projectCustomName);
  uiThemeSave(g_uiTheme);
  transportPrefsSave();
  g_screen = Screen::Play;
  g_lastAction = "Settings saved";
  resetSettingsNav();
  drawPlaySurface();
}

static void settingsApplyDropdownPick(int8_t rid, int opt) {
  switch (rid) {
    case 0:
      g_settings.midiOutChannel = static_cast<uint8_t>(opt + 1);
      break;
    case 1:
      g_settings.midiInChannelUsb = static_cast<uint8_t>(opt == 0 ? 0 : opt);
      break;
    case 18:
      g_settings.midiInChannelBle = static_cast<uint8_t>(opt == 0 ? 0 : opt);
      break;
    case 19:
      g_settings.midiInChannelDin = static_cast<uint8_t>(opt == 0 ? 0 : opt);
      break;
    case 2:
      g_settings.midiTransportSend = static_cast<uint8_t>(opt);
      settingsSave(g_settings);
      break;
    case 3:
      g_settings.midiTransportReceive = static_cast<uint8_t>(opt);
      settingsSave(g_settings);
      break;
    case 4:
      g_settings.midiClockSource = static_cast<uint8_t>(opt);
      settingsSave(g_settings);
      break;
    case 37:
      g_settings.midiThruMask = static_cast<uint8_t>(opt & 0x07);
      settingsSave(g_settings);
      break;
    case 5:
      g_settings.brightnessPercent = static_cast<uint8_t>((opt + 1) * 10);
      applyBrightness();
      break;
    case 6:
      g_settings.outputVelocity = static_cast<uint8_t>(40 + opt * 10);
      break;
    case 43:
      g_settings.velocityCurve = static_cast<uint8_t>((opt < 0 || opt > 2) ? 0 : opt);
      settingsSave(g_settings);
      break;
    case 38:
      g_settings.globalSwingPct = static_cast<uint8_t>(opt * 10);
      settingsSave(g_settings);
      transportSetGlobalSwing(g_settings.globalSwingPct);
      break;
    case 39:
      g_settings.globalHumanizePct = static_cast<uint8_t>(opt * 10);
      settingsSave(g_settings);
      transportSetGlobalHumanize(g_settings.globalHumanizePct);
      break;
    case 40:
      g_settings.settingsEntryHoldPreset = static_cast<uint8_t>((opt < 0 || opt > 4) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 41:
      g_settings.seqLongPressPreset = static_cast<uint8_t>((opt < 0 || opt > 4) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 42:
      g_settings.bpmHoldPreset = static_cast<uint8_t>((opt < 0 || opt > 3) ? 1 : opt);
      settingsSave(g_settings);
      break;
    case 8:
      g_uiTheme = static_cast<uint8_t>(opt);
      uiThemeSave(g_uiTheme);
      uiThemeApply(g_uiTheme);
      break;
    case 9:
      g_clickVolumePercent = static_cast<uint8_t>(opt * 10);
      transportApplyClickVolume();
      transportPrefsSave();
      break;
    case 10:
      g_settings.arpeggiatorMode = static_cast<uint8_t>(opt);
      settingsSave(g_settings);
      break;
    case 11:
      g_projectBpm = static_cast<uint16_t>(40 + opt * 5);
      projectBpmSave(g_projectBpm);
      break;
    case 17:
      g_settings.transposeSemitones = static_cast<int8_t>(opt - 24);
      settingsSave(g_settings);
      break;
    case 13:
      if (opt == 1) {
        settingsRunFactoryReset();
      }
      break;
    case 21:
      g_settings.midiDebugEnabled = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      break;
    case 22:
      g_settings.clkFollow = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      break;
    case 23:
      g_settings.clkFlashEdge = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      break;
    case 25:
      g_settings.playInMonitor = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      break;
    case 26:
      g_settings.playInClockHold = static_cast<uint8_t>(opt > 2 ? 1 : opt);
      settingsSave(g_settings);
      break;
    case 27:
      g_settings.playInMonitorMode = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      break;
    case 28:
      g_settings.bleDebugPeakDecay = static_cast<uint8_t>((opt < 0 || opt > 3) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 29:
      g_settings.panicDebugRotate = static_cast<uint8_t>((opt < 0 || opt > 3) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 30:
      if (g_settings.suggestProfileLock) {
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Set SUG lock Off to edit");
        break;
      }
      g_settings.suggestStableWindow = static_cast<uint8_t>((opt < 0 || opt > 4) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 31:
      if (g_settings.suggestProfileLock) {
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Set SUG lock Off to edit");
        break;
      }
      g_settings.suggestStableGap = static_cast<uint8_t>((opt < 0 || opt > 4) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 33:
      g_settings.suggestProfile = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      if (g_settings.suggestProfile == 0) {
        g_settings.suggestStableWindow = 2;  // 1.4s
        g_settings.suggestStableGap = 2;     // 18
      } else {
        g_settings.suggestStableWindow = 3;  // 2.0s
        g_settings.suggestStableGap = 3;     // 24
      }
      g_settings.suggestProfileLock = 1;
      settingsSave(g_settings);
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG profile %c applied (lock on)",
               g_settings.suggestProfile == 0 ? 'A' : 'B');
      break;
    case 35:
      g_settings.suggestProfileLock = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG lock %s",
               g_settings.suggestProfileLock ? "on" : "off");
      break;
    default:
      break;
  }
  g_settings.normalize();
}

static void settingsApplySuggestionDefaults() {
  g_settings.suggestStableWindow = 2;  // 1.4s
  g_settings.suggestStableGap = 2;     // 18
  g_settings.suggestProfile = 0;
  g_settings.suggestProfileLock = 0;
  g_settings.normalize();
  settingsSave(g_settings);
  snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG defaults restored");
}

static void settingsFlipSuggestionProfile() {
  g_settings.suggestProfile = (g_settings.suggestProfile == 0) ? 1 : 0;
  if (g_settings.suggestProfile == 0) {
    g_settings.suggestStableWindow = 2;  // 1.4s
    g_settings.suggestStableGap = 2;     // 18
  } else {
    g_settings.suggestStableWindow = 3;  // 2.0s
    g_settings.suggestStableGap = 3;     // 24
  }
  g_settings.suggestProfileLock = 1;
  g_settings.normalize();
  settingsSave(g_settings);
  snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG flipped to %c (lock on)",
           g_settings.suggestProfile == 0 ? 'A' : 'B');
}

static void settingsOpenDropdownOrAction(uint8_t rid, int w, int h) {
  (void)w;
  switch (rid) {
    case 7:
      return;
    case 12:
      openProjectNameEditor();
      return;
    case 14:
      settingsRunBackup();
      return;
    case 15:
      g_settingsConfirmAction = SettingsConfirmAction::RestoreSd;
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Confirm restore from SD");
      return;
    case 16:
      settingsSaveAndExit();
      return;
    case 20:
#if M5CHORD_ENABLE_MIDI_DEBUG_SCREEN
      if (!g_settings.midiDebugEnabled) {
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Enable MIDI debug first");
        return;
      }
      g_screen = Screen::MidiDebug;
      drawMidiDebugSurface();
#else
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "MIDI debug disabled in build");
#endif
      return;
    case 24:
      g_settingsConfirmAction = SettingsConfirmAction::ClearMidiDebug;
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Confirm clear MIDI debug");
      return;
    case 32:
      settingsApplySuggestionDefaults();
      return;
    case 34:
      settingsFlipSuggestionProfile();
      return;
    case 36:
      if (!g_settings.suggestProfileLock) {
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG already unlocked");
        return;
      }
      g_settings.suggestProfileLock = 0;
      g_settings.normalize();
      settingsSave(g_settings);
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG lock off");
      return;
    default:
      break;
  }
  if (settingsDropdownOptionCount(static_cast<int8_t>(rid)) <= 0) return;
  s_settingsDropRowId = static_cast<int8_t>(rid);
  s_settingsDropOptScroll = settingsDropdownScrollForCurrent(s_settingsDropRowId, h);
}

static void settingsComputeDropdownLayout(int w, int h, int8_t rid, int* outX, int* outY, int* outW,
                                          int* rowH, int* firstOpt, int* visCount, int* totalOpts) {
  *totalOpts = settingsDropdownOptionCount(rid);
  *rowH = 20;
  *visCount = min(*totalOpts, settingsDropVisibleRows(h));
  int totalH = (*rowH) * (*visCount);
  *outW = min(w - 16, 220);
  *outX = (w - *outW) / 2;
  *outY = max(36, (h - kBezelBarH - totalH) / 2);
  *firstOpt = s_settingsDropOptScroll;
  if (*firstOpt + *visCount > *totalOpts) {
    *firstOpt = max(0, *totalOpts - *visCount);
    s_settingsDropOptScroll = *firstOpt;
  }
}

static int settingsHitDropdown(int px, int py, int w, int h) {
  int dx, dy, dw, rh, first, vis, tot;
  settingsComputeDropdownLayout(w, h, s_settingsDropRowId, &dx, &dy, &dw, &rh, &first, &vis, &tot);
  if (px < dx || px >= dx + dw || py < dy || py >= dy + vis * rh) return -1;
  int row = (py - dy) / rh;
  int idx = first + row;
  if (idx < 0 || idx >= tot) return -1;
  return idx;
}

static int settingsHitMenuCell(int px, int py, int w, int h) {
  constexpr int kTop = 42;
  const int gap = 8;
  int y0 = kTop;
  int availH = h - kBezelBarH - y0 - gap;
  int availW = w - 2 * gap;
  if (availH < 40 || availW < 40) return -1;
  int cw = (availW - gap) / 2;
  int ch = (availH - gap) / 2;
  if (px < gap || py < y0) return -1;
  int lx = (px - gap) / (cw + gap);
  int ly = (py - y0) / (ch + gap);
  if (lx < 0 || lx > 1 || ly < 0 || ly > 1) return -1;
  int ox = gap + lx * (cw + gap);
  int oy = y0 + ly * (ch + gap);
  if (px < ox || px >= ox + cw || py < oy || py >= oy + ch) return -1;
  return ly * 2 + lx;
}

static int settingsHitSectionRow(int px, int py, int w, int h) {
  (void)w;
  int n = settingsPanelRowCount(g_settingsPanel);
  if (n <= 0) return -1;
  constexpr int kRowH = 36;
  constexpr int kTop = 42;
  int fb = (g_settingsFeedback[0] != '\0') ? 20 : 4;
  int contentBottom = h - kBezelBarH - fb;
  int visible = (contentBottom - kTop) / kRowH;
  if (visible < 1) return -1;
  if (py < kTop || py >= kTop + visible * kRowH) return -1;
  int rel = (py - kTop) / kRowH + g_settingsListScroll;
  if (rel < 0 || rel >= n) return -1;
  return rel;
}

static const char* settingsConfirmTitle() {
  switch (g_settingsConfirmAction) {
    case SettingsConfirmAction::RestoreSd:
      return "Restore from SD?";
    case SettingsConfirmAction::ClearMidiDebug:
      return "Clear MIDI debug data?";
    default:
      return "";
  }
}

static const char* settingsRowTitle(uint8_t rid) {
  switch (rid) {
    case 0:
      return "MIDI out";
    case 1:
      return "MIDI in USB";
    case 18:
      return "MIDI in BLE";
    case 19:
      return "MIDI in DIN";
    case 2:
      return "Transport send";
    case 3:
      return "Transport recv";
    case 4:
      return "MIDI clock";
    case 37:
      return "MIDI thru src";
    case 17:
      return "Transpose";
    case 5:
      return "Brightness";
    case 6:
      return "Velocity";
    case 43:
      return "Velocity curve";
    case 38:
      return "Global swing";
    case 39:
      return "Global humanize";
    case 40:
      return "Settings hold";
    case 41:
      return "Seq clear hold";
    case 42:
      return "BPM hold";
    case 7:
      return "Build";
    case 8:
      return "Theme";
    case 9:
      return "Click volume";
    case 10:
      return "Arpeggiator";
    case 11:
      return "BPM";
    case 12:
      return "Project folder";
    case 13:
      return "Factory reset";
    case 14:
      return "Backup SD";
    case 15:
      return "Restore SD";
    case 16:
      return "Save & exit";
    case 20:
      return "MIDI debug";
    case 21:
      return "MIDI debug enabled";
    case 22:
      return "Follow MIDI clock";
    case 23:
      return "Clock flash edge";
    case 24:
      return "Clear MIDI debug";
    case 25:
      return "Play IN monitor";
    case 26:
      return "IN clock hold";
    case 27:
      return "IN monitor mode";
    case 28:
      return "BLE peak decay";
    case 29:
      return "Panic rotate";
    case 30:
      return "SUG lane window";
    case 31:
      return "SUG score gap";
    case 33:
      return "SUG profile";
    case 35:
      return "SUG profile lock";
    case 36:
      return "SUG unlock now";
    case 34:
      return "SUG flip A/B";
    case 32:
      return "SUG defaults";
    default:
      return "";
  }
}

static void settingsRowValueString(uint8_t rid, char* buf, size_t n) {
  switch (rid) {
    case 0:
      snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.midiOutChannel));
      break;
    case 1:
      if (g_settings.midiInChannelUsb == 0) {
        snprintf(buf, n, "OMNI");
      } else {
        snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.midiInChannelUsb));
      }
      break;
    case 18:
      if (g_settings.midiInChannelBle == 0) {
        snprintf(buf, n, "OMNI");
      } else {
        snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.midiInChannelBle));
      }
      break;
    case 19:
      if (g_settings.midiInChannelDin == 0) {
        snprintf(buf, n, "OMNI");
      } else {
        snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.midiInChannelDin));
      }
      break;
    case 2:
      snprintf(buf, n, "%s", midiTransportRouteLabel(g_settings.midiTransportSend));
      break;
    case 3:
      snprintf(buf, n, "%s", midiTransportRouteLabel(g_settings.midiTransportReceive));
      break;
    case 4:
      snprintf(buf, n, "%s", midiClockSourceLabel(g_settings.midiClockSource));
      break;
    case 37:
      switch (g_settings.midiThruMask & 0x07U) {
        case 0:
          snprintf(buf, n, "Off");
          break;
        case 1:
          snprintf(buf, n, "USB");
          break;
        case 2:
          snprintf(buf, n, "BLE");
          break;
        case 3:
          snprintf(buf, n, "USB+BLE");
          break;
        case 4:
          snprintf(buf, n, "DIN");
          break;
        case 5:
          snprintf(buf, n, "USB+DIN");
          break;
        case 6:
          snprintf(buf, n, "BLE+DIN");
          break;
        default:
          snprintf(buf, n, "All");
          break;
      }
      break;
    case 17: {
      const int v = static_cast<int>(g_settings.transposeSemitones);
      if (v > 0) {
        snprintf(buf, n, "+%d", v);
      } else {
        snprintf(buf, n, "%d", v);
      }
      break;
    }
    case 5:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(g_settings.brightnessPercent));
      break;
    case 6:
      snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.outputVelocity));
      break;
    case 43:
      snprintf(buf, n, "%s", g_settings.velocityCurve == 0 ? "Linear" :
                             (g_settings.velocityCurve == 1 ? "Soft" : "Hard"));
      break;
    case 38:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(g_settings.globalSwingPct));
      break;
    case 39:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(g_settings.globalHumanizePct));
      break;
    case 40:
      snprintf(buf, n, "%s", g_settings.settingsEntryHoldPreset == 0
                               ? "0.5s"
                               : (g_settings.settingsEntryHoldPreset == 1
                                      ? "0.65s"
                                      : (g_settings.settingsEntryHoldPreset == 2
                                             ? "0.8s"
                                             : (g_settings.settingsEntryHoldPreset == 3 ? "1.0s" : "1.2s"))));
      break;
    case 41:
      snprintf(buf, n, "%s", g_settings.seqLongPressPreset == 0
                               ? "0.25s"
                               : (g_settings.seqLongPressPreset == 1
                                      ? "0.3s"
                                      : (g_settings.seqLongPressPreset == 2
                                             ? "0.4s"
                                             : (g_settings.seqLongPressPreset == 3 ? "0.5s" : "0.7s"))));
      break;
    case 42:
      snprintf(buf, n, "%s", g_settings.bpmHoldPreset == 0
                               ? "0.35s"
                               : (g_settings.bpmHoldPreset == 1
                                      ? "0.5s"
                                      : (g_settings.bpmHoldPreset == 2 ? "0.7s" : "0.9s")));
      break;
    case 7:
      snprintf(buf, n, "%s", M5CHORD_BUILD_STAMP);
      break;
    case 8:
      snprintf(buf, n, "%s", uiThemeName(g_uiTheme));
      break;
    case 9:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(g_clickVolumePercent));
      break;
    case 10:
      snprintf(buf, n, "%s", arpeggiatorModeLabel(g_settings.arpeggiatorMode));
      break;
    case 20:
      if (!M5CHORD_ENABLE_MIDI_DEBUG_SCREEN) {
        snprintf(buf, n, "Build off");
      } else if (!g_settings.midiDebugEnabled) {
        snprintf(buf, n, "Disabled");
      } else {
        snprintf(buf, n, "Open");
      }
      break;
    case 21:
      snprintf(buf, n, "%s", g_settings.midiDebugEnabled ? "On" : "Off");
      break;
    case 22:
      snprintf(buf, n, "%s", g_settings.clkFollow ? "On" : "Off");
      break;
    case 23:
      snprintf(buf, n, "%s", g_settings.clkFlashEdge ? "1/8" : "1/4");
      break;
    case 24:
      snprintf(buf, n, "Run");
      break;
    case 32:
      snprintf(buf, n, "Apply");
      break;
    case 34:
      snprintf(buf, n, "Flip");
      break;
    case 36:
      snprintf(buf, n, "Run");
      break;
    case 33:
      snprintf(buf, n, "%s", g_settings.suggestProfile == 0 ? "A" : "B");
      break;
    case 35:
      snprintf(buf, n, "%s", g_settings.suggestProfileLock ? "On" : "Off");
      break;
    case 25:
      snprintf(buf, n, "%s", g_settings.playInMonitor ? "On" : "Off");
      break;
    case 26:
      snprintf(buf, n, "%s", g_settings.playInClockHold == 0 ? "Short" :
                             (g_settings.playInClockHold == 1 ? "Medium" : "Long"));
      break;
    case 27:
      snprintf(buf, n, "%s", g_settings.playInMonitorMode ? "Detailed" : "Compact");
      break;
    case 28:
      snprintf(buf, n, "%s", g_settings.bleDebugPeakDecay == 0 ? "Off" :
                             (g_settings.bleDebugPeakDecay == 1
                                  ? "5s"
                                  : (g_settings.bleDebugPeakDecay == 2 ? "10s" : "30s")));
      break;
    case 29:
      snprintf(buf, n, "%s", g_settings.panicDebugRotate == 0
                               ? "Off"
                               : (g_settings.panicDebugRotate == 1
                                      ? "1s"
                                      : (g_settings.panicDebugRotate == 2 ? "2s" : "4s")));
      break;
    case 30:
      snprintf(buf, n, "%s%s", g_settings.suggestStableWindow == 0
                                  ? "0.6s"
                                  : (g_settings.suggestStableWindow == 1
                                         ? "1.0s"
                                         : (g_settings.suggestStableWindow == 2
                                                ? "1.4s"
                                                : (g_settings.suggestStableWindow == 3 ? "2.0s" : "3.0s"))),
               g_settings.suggestProfileLock ? " [L]" : "");
      break;
    case 31:
      snprintf(buf, n, "%s%s", g_settings.suggestStableGap == 0
                                  ? "8"
                                  : (g_settings.suggestStableGap == 1
                                         ? "12"
                                         : (g_settings.suggestStableGap == 2
                                                ? "18"
                                                : (g_settings.suggestStableGap == 3 ? "24" : "32"))),
               g_settings.suggestProfileLock ? " [L]" : "");
      break;
    case 11:
      snprintf(buf, n, "%u", static_cast<unsigned>(g_projectBpm));
      break;
    case 12:
      if (g_projectCustomName[0] != '\0') {
        snprintf(buf, n, "%.12s", g_projectCustomName);
      } else {
        snprintf(buf, n, "(auto)");
      }
      break;
    case 13:
    case 14:
    case 15:
    case 16:
      buf[0] = '\0';
      break;
    default:
      buf[0] = '\0';
      break;
  }
}

void drawSettingsUi() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutBottomBezels(w, h);
  M5.Display.startWrite();
  M5.Display.fillScreen(g_uiPalette.bg);

  M5.Display.setFont(nullptr);
  M5.Display.setTextColor(g_uiPalette.settingsHeader, g_uiPalette.bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Settings", w / 2, 4);

  if (g_settingsPanel == SettingsPanel::Menu) {
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
    M5.Display.drawString("Choose a category", w / 2, 26);
    constexpr int kTop = 42;
    const int gap = 8;
    int y0 = kTop;
    int availH = h - kBezelBarH - y0 - gap;
    int availW = w - 2 * gap;
    int cw = (availW - gap) / 2;
    int ch = (availH - gap) / 2;
    const char* labs[4] = {"MIDI", "Display", "Seq/Arp", "SD/Backup"};
    for (int i = 0; i < 4; ++i) {
      int lx = i % 2;
      int ly = i / 2;
      Rect r = {gap + lx * (cw + gap), y0 + ly * (ch + gap), cw, ch};
      const uint8_t tsize = (ly == 1) ? static_cast<uint8_t>(1) : static_cast<uint8_t>(2);
      drawRoundedButton(r, g_uiPalette.panelMuted, labs[i], tsize);
    }
  } else {
    const char* sec = "";
    switch (g_settingsPanel) {
      case SettingsPanel::Midi:
        sec = "MIDI";
        break;
      case SettingsPanel::Display:
        sec = "Display";
        break;
      case SettingsPanel::SeqArp:
        sec = "Seq/Arp";
        break;
      case SettingsPanel::Storage:
        sec = "SD/Backup";
        break;
      default:
        break;
    }
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
    M5.Display.drawString(sec, w / 2, 26);

    constexpr int kRowH = 36;
    constexpr int kTop = 42;
    int fb = (g_settingsFeedback[0] != '\0') ? 20 : 4;
    int contentBottom = h - kBezelBarH - fb;
    int n = settingsPanelRowCount(g_settingsPanel);
    settingsEnsureListScroll(n, kRowH, kTop, contentBottom);

    for (int vi = 0; vi < n; ++vi) {
      int si = vi - g_settingsListScroll;
      if (si < 0) continue;
      int y = kTop + si * kRowH;
      if (y + kRowH > contentBottom) break;
      uint8_t rid = settingsPanelRowId(g_settingsPanel, vi);
      bool sel = (vi == g_settingsCursorRow);
      M5.Display.setTextSize(3);
      M5.Display.setTextDatum(middle_left);
      M5.Display.setTextColor(sel ? g_uiPalette.rowSelect : g_uiPalette.rowNormal, g_uiPalette.bg);
      char val[40];
      settingsRowValueString(rid, val, sizeof(val));
      char line[48];
      snprintf(line, sizeof(line), "%s", settingsRowTitle(rid));
      M5.Display.drawString(line, 6, y + kRowH / 2);
      M5.Display.setTextDatum(middle_right);
      M5.Display.drawString(val, w - 6, y + kRowH / 2);
    }

    if (g_settingsPanel == SettingsPanel::Midi &&
        (g_settingsRow == 25 || g_settingsRow == 26 || g_settingsRow == 27)) {
      const int yHint = h - kBezelBarH - ((g_settingsFeedback[0] != '\0') ? 34 : 22);
      const bool detailedMode = (g_settings.playInMonitorMode != 0);
      M5.Display.setTextSize(1);
      M5.Display.setTextDatum(top_center);
      M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
      M5.Display.drawString("Monitor age color:", w / 2, yHint);

      const int ySwatch = yHint + 10;
      M5.Display.setTextDatum(top_left);
      M5.Display.setTextColor(detailedMode ? g_uiPalette.accentPress : g_uiPalette.subtle, g_uiPalette.bg);
      M5.Display.drawString("fresh<0.3s", 16, ySwatch);
      M5.Display.setTextColor(detailedMode ? g_uiPalette.rowNormal : g_uiPalette.subtle, g_uiPalette.bg);
      M5.Display.drawString("recent<1.2s", 112, ySwatch);
      M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
      M5.Display.drawString("stale>=1.2s", 220, ySwatch);
      M5.Display.setTextDatum(top_right);
      M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
      M5.Display.drawString(detailedMode ? "[DETAILED]" : "[COMPACT]", w - 8, ySwatch);
    }

    if (g_settingsFeedback[0] != '\0') {
      M5.Display.setTextDatum(top_center);
      M5.Display.setTextSize(1);
      M5.Display.setTextColor(g_uiPalette.feedback, g_uiPalette.bg);
      M5.Display.drawString(g_settingsFeedback, w / 2, h - kBezelBarH - 18);
    }
  }

  if (s_settingsDropRowId >= 0) {
    int dx, dy, dww, rh, first, vis, tot;
    settingsComputeDropdownLayout(w, h, s_settingsDropRowId, &dx, &dy, &dww, &rh, &first, &vis,
                                  &tot);
    M5.Display.fillRect(dx - 2, dy - 2, dww + 4, vis * rh + 4, g_uiPalette.bg);
    M5.Display.drawRect(dx - 2, dy - 2, dww + 4, vis * rh + 4, g_uiPalette.settingsBtnBorder);
    for (int i = 0; i < vis; ++i) {
      int opt = first + i;
      if (opt >= tot) break;
      Rect dr = {dx, dy + i * rh, dww, rh};
      char lab[40];
      settingsFormatDropdownOption(s_settingsDropRowId, opt, lab, sizeof(lab));
      drawRoundedButton(dr, g_uiPalette.settingsBtnIdle, lab, 1);
    }
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_center);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
    if (tot > vis) {
      char hint[24];
      snprintf(hint, sizeof(hint), "FWD: more  BACK: up/close");
      M5.Display.drawString(hint, w / 2, max(4, dy - 14));
    }
  }

  if (g_settingsConfirmAction != SettingsConfirmAction::None) {
    const int boxW = min(w - 24, 250);
    const int boxH = 66;
    const int boxX = (w - boxW) / 2;
    const int boxY = (h - kBezelBarH - boxH) / 2;
    M5.Display.fillRoundRect(boxX, boxY, boxW, boxH, 8, g_uiPalette.panelMuted);
    M5.Display.drawRoundRect(boxX, boxY, boxW, boxH, 8, g_uiPalette.settingsBtnActive);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_center);
    M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.panelMuted);
    M5.Display.drawString(settingsConfirmTitle(), w / 2, boxY + 10);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.panelMuted);
    M5.Display.drawString("BACK cancel  SELECT/FWD confirm", w / 2, boxY + 34);
  }

  drawBezelBarStrip();
  M5.Display.endWrite();
}

void processSettingsTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);

  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
    wasTouchActive = true;
    return;
  }

  if (!wasTouchActive) return;
  wasTouchActive = false;

  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;

  if (g_settingsConfirmAction != SettingsConfirmAction::None) {
    if (pointInRect(hx, hy, g_bezelBack)) {
      g_settingsConfirmAction = SettingsConfirmAction::None;
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Cancelled");
      drawSettingsUi();
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect) || pointInRect(hx, hy, g_bezelFwd)) {
      const SettingsConfirmAction action = g_settingsConfirmAction;
      g_settingsConfirmAction = SettingsConfirmAction::None;
      if (action == SettingsConfirmAction::RestoreSd) {
        settingsRunRestoreFlow();
      } else if (action == SettingsConfirmAction::ClearMidiDebug) {
        settingsRunClearMidiDebugData();
      }
      if (g_screen == Screen::Settings) {
        drawSettingsUi();
      }
      return;
    }
    drawSettingsUi();
    return;
  }

  if (pointInRect(hx, hy, g_bezelBack)) {
    if (s_settingsDropRowId >= 0) {
      if (s_settingsDropOptScroll > 0) {
        --s_settingsDropOptScroll;
        drawSettingsUi();
        return;
      }
      s_settingsDropRowId = -1;
      drawSettingsUi();
      return;
    }
    if (g_settingsPanel != SettingsPanel::Menu) {
      g_factoryResetConfirmArmed = false;
      g_settingsPanel = SettingsPanel::Menu;
      drawSettingsUi();
      return;
    }
    settingsSaveAndExit();
    return;
  }

  if (pointInRect(hx, hy, g_bezelFwd)) {
    if (s_settingsDropRowId >= 0) {
      int cnt = settingsDropdownOptionCount(s_settingsDropRowId);
      int vis = settingsDropVisibleRows(h);
      if (s_settingsDropOptScroll + vis < cnt) {
        ++s_settingsDropOptScroll;
        drawSettingsUi();
      }
      return;
    }
    if (g_settingsPanel != SettingsPanel::Menu) {
      settingsMoveCursorInSection(1, h);
      drawSettingsUi();
    }
    return;
  }

  if (pointInRect(hx, hy, g_bezelSelect)) {
    if (s_settingsDropRowId >= 0) {
      drawSettingsUi();
      return;
    }
    if (g_settingsPanel != SettingsPanel::Menu) {
      settingsSyncRowFromCursor();
      settingsOpenDropdownOrAction(static_cast<uint8_t>(g_settingsRow), w, h);
      if (g_screen == Screen::Settings) {
        drawSettingsUi();
      }
    }
    return;
  }

  if (s_settingsDropRowId >= 0) {
    int pick = settingsHitDropdown(hx, hy, w, h);
    if (pick >= 0) {
      settingsApplyDropdownPick(s_settingsDropRowId, pick);
      s_settingsDropRowId = -1;
    } else {
      s_settingsDropRowId = -1;
    }
    drawSettingsUi();
    return;
  }

  if (g_settingsPanel == SettingsPanel::Menu) {
    int cell = settingsHitMenuCell(hx, hy, w, h);
    if (cell == 0) {
      g_settingsPanel = SettingsPanel::Midi;
    } else if (cell == 1) {
      g_settingsPanel = SettingsPanel::Display;
    } else if (cell == 2) {
      g_settingsPanel = SettingsPanel::SeqArp;
    } else if (cell == 3) {
      g_settingsPanel = SettingsPanel::Storage;
    } else {
      drawSettingsUi();
      return;
    }
    g_settingsCursorRow = 0;
    g_settingsListScroll = 0;
    g_factoryResetConfirmArmed = false;
    settingsSyncRowFromCursor();
    drawSettingsUi();
    return;
  }

  int rowHit = settingsHitSectionRow(hx, hy, w, h);
  if (rowHit >= 0) {
    g_settingsCursorRow = rowHit;
    settingsSyncRowFromCursor();
    settingsOpenDropdownOrAction(static_cast<uint8_t>(g_settingsRow), w, h);
    if (g_screen == Screen::Settings) {
      drawSettingsUi();
    }
    return;
  }

  drawSettingsUi();
}

static uint8_t midiInputChannelForSource(MidiSource src) {
  switch (src) {
    case MidiSource::Usb:
      return g_settings.midiInChannelUsb;
    case MidiSource::Ble:
      return g_settings.midiInChannelBle;
    case MidiSource::Din:
      return g_settings.midiInChannelDin;
    default:
      return 0;
  }
}

static bool midiEventPassesChannelFilter(const MidiEvent& ev) {
  if (!midiEventIsChannelVoice(ev)) return true;
  const uint8_t filter = midiInputChannelForSource(ev.source);
  if (filter == 0) return true;  // OMNI
  return ev.channel == static_cast<uint8_t>(filter - 1U);
}

static const char* midiSourceCompactLabel(MidiSource src) {
  switch (src) {
    case MidiSource::Usb:
      return "USB";
    case MidiSource::Ble:
      return "BLE";
    case MidiSource::Din:
      return "DIN";
    default:
      return "?";
  }
}

static const char* midiEventCompactLabel(const MidiEvent& ev) {
  switch (ev.type) {
    case MidiEventType::NoteOn:
      return "On";
    case MidiEventType::NoteOff:
      return "Off";
    case MidiEventType::ControlChange:
      return "CC";
    case MidiEventType::Clock:
      return "Clk";
    case MidiEventType::Start:
      return "Start";
    case MidiEventType::Stop:
      return "Stop";
    case MidiEventType::Continue:
      return "Cont";
    case MidiEventType::SongPosition:
      return "SPP";
    default:
      return "Evt";
  }
}

static bool midiEventIsPlayMonitorMusical(const MidiEvent& ev) {
  switch (ev.type) {
    case MidiEventType::NoteOn:
    case MidiEventType::NoteOff:
    case MidiEventType::ControlChange:
    case MidiEventType::Start:
    case MidiEventType::Stop:
    case MidiEventType::Continue:
    case MidiEventType::SongPosition:
      return true;
    default:
      return false;
  }
}

static uint32_t playMonitorClockSuppressMs() {
  switch (g_settings.playInClockHold) {
    case 0:
      return 600U;
    case 2:
      return 2000U;
    default:
      return 1200U;
  }
}

static void playMonitorFormatAge(uint32_t ageMs, char* out, size_t outSize) {
  if (ageMs < 1000U) {
    snprintf(out, outSize, "+%ums", static_cast<unsigned>(ageMs));
    return;
  }
  const unsigned tenths = static_cast<unsigned>((ageMs + 50U) / 100U);
  const unsigned whole = tenths / 10U;
  const unsigned frac = tenths % 10U;
  snprintf(out, outSize, "+%u.%us", whole, frac);
}

static void playMonitorFormatLine(const MidiEvent& ev, char* out, size_t outSize, uint32_t nowMs) {
  const bool detailed = g_settings.playInMonitorMode != 0;
  const char* src = midiSourceCompactLabel(ev.source);
  const char* typ = midiEventCompactLabel(ev);
  const uint32_t ageMs = (nowMs >= s_playIngressInfoEventMs) ? (nowMs - s_playIngressInfoEventMs) : 0U;
  char ageBuf[12];
  playMonitorFormatAge(ageMs, ageBuf, sizeof(ageBuf));
  if (!detailed) {
    if (midiEventIsChannelVoice(ev)) {
      snprintf(out, outSize, "IN:%s ch%u %s", src, static_cast<unsigned>(ev.channel + 1U), typ);
    } else {
      snprintf(out, outSize, "IN:%s -- %s", src, typ);
    }
    return;
  }
  switch (ev.type) {
    case MidiEventType::NoteOn:
    case MidiEventType::NoteOff:
      snprintf(out, outSize, "IN:%s ch%u %s n%u v%u %s", src, static_cast<unsigned>(ev.channel + 1U), typ,
               static_cast<unsigned>(ev.data1), static_cast<unsigned>(ev.data2), ageBuf);
      return;
    case MidiEventType::ControlChange:
      snprintf(out, outSize, "IN:%s ch%u CC %u=%u %s", src, static_cast<unsigned>(ev.channel + 1U),
               static_cast<unsigned>(ev.data1), static_cast<unsigned>(ev.data2), ageBuf);
      return;
    case MidiEventType::SongPosition:
      snprintf(out, outSize, "IN:%s -- SPP %u %s", src, static_cast<unsigned>(ev.value14), ageBuf);
      return;
    default:
      break;
  }
  if (midiEventIsChannelVoice(ev)) {
    snprintf(out, outSize, "IN:%s ch%u %s %s", src, static_cast<unsigned>(ev.channel + 1U), typ, ageBuf);
  } else {
    snprintf(out, outSize, "IN:%s -- %s %s", src, typ, ageBuf);
  }
}

static void processIncomingMidiEvent(const MidiEvent& ev, uint32_t nowMs) {
  s_midiEventHistory.push(nowMs, ev);
  const bool isClock = ev.type == MidiEventType::Clock;
  const bool isMusical = midiEventIsPlayMonitorMusical(ev);
  if (isMusical) {
    s_playIngressLastMusicalMs = nowMs;
  }
  // Avoid monitor spam from dense MIDI clock traffic while musical events are active.
  const bool allowClockUpdate = !isClock || (nowMs - s_playIngressLastMusicalMs >= playMonitorClockSuppressMs());
  if (allowClockUpdate) {
    s_playIngressInfoEventMs = nowMs;
    playMonitorFormatLine(ev, s_playIngressInfo, sizeof(s_playIngressInfo), nowMs);
    s_playIngressInfoUntilMs = nowMs + 2200;
    s_playIngressInfoDirty = true;
  }
  if (!midiEventIsRealtime(ev)) {
    if (!midiEventPassesChannelFilter(ev)) return;
    const uint8_t srcBit =
        (ev.source == MidiSource::Usb) ? 0x01U : ((ev.source == MidiSource::Ble) ? 0x02U : 0x04U);
    if ((g_settings.midiThruMask & srcBit) != 0U) {
      switch (ev.type) {
        case MidiEventType::NoteOn:
          if (ev.data2 == 0) {
            midiSendNoteOff(ev.channel, ev.data1);
          } else {
            midiSendNoteOn(ev.channel, ev.data1, ev.data2);
          }
          break;
        case MidiEventType::NoteOff:
          midiSendNoteOff(ev.channel, ev.data1);
          break;
        case MidiEventType::ControlChange:
          midiSendControlChange(ev.channel, ev.data1, ev.data2);
          break;
        default:
          break;
      }
    }
    s_midiInState.onEvent(ev);
    if (ev.type == MidiEventType::NoteOn || ev.type == MidiEventType::NoteOff) {
      uint8_t active[16] = {};
      size_t n = 0;
      s_midiInState.collectActiveNotes(ev.source, ev.channel, active, sizeof(active), &n);
      char chord[12];
      if (midiDetectChordFromNotes(active, n, chord, sizeof(chord))) {
        snprintf(s_midiDetectedChord, sizeof(s_midiDetectedChord), "%s", chord);
        s_midiDetectedChordUntilMs = nowMs + 2000;
        MidiSuggestionItem ranked[ChordModel::kSurroundCount]{};
        const size_t rankedN = midiRankSuggestions(g_model, chord, ranked, ChordModel::kSurroundCount);
        if (rankedN > 0) {
          size_t chosenIdx = 0;
          bool usedStability = false;
          uint32_t stableWindowMs = 1400U;
          switch (g_settings.suggestStableWindow) {
            case 0:
              stableWindowMs = 600U;
              break;
            case 1:
              stableWindowMs = 1000U;
              break;
            case 2:
              stableWindowMs = 1400U;
              break;
            case 3:
              stableWindowMs = 2000U;
              break;
            default:
              stableWindowMs = 3000U;
              break;
          }
          int stableScoreGap = 18;
          switch (g_settings.suggestStableGap) {
            case 0:
              stableScoreGap = 8;
              break;
            case 1:
              stableScoreGap = 12;
              break;
            case 2:
              stableScoreGap = 18;
              break;
            case 3:
              stableScoreGap = 24;
              break;
            default:
              stableScoreGap = 32;
              break;
          }
          // Source/channel stability: when events stay on the same lane, keep prior best
          // unless a new candidate is materially stronger.
          const bool stableLane =
              (s_midiSuggestLastSource == ev.source) && (s_midiSuggestLastChannel == ev.channel) &&
              (s_midiSuggestLastAtMs > 0) && ((nowMs - s_midiSuggestLastAtMs) <= stableWindowMs);
          if (stableLane && s_midiDetectedSuggest[0] != '\0') {
            int prevIdx = -1;
            for (size_t i = 0; i < rankedN; ++i) {
              if (strcmp(ranked[i].name, s_midiDetectedSuggest) == 0) {
                prevIdx = static_cast<int>(i);
                break;
              }
            }
            if (prevIdx >= 0) {
              const int topScore = static_cast<int>(ranked[0].score);
              const int prevScore = static_cast<int>(ranked[prevIdx].score);
              if (topScore - prevScore < stableScoreGap) {
                chosenIdx = static_cast<size_t>(prevIdx);
                usedStability = true;
              }
            }
          }
          snprintf(s_midiDetectedSuggest, sizeof(s_midiDetectedSuggest), "%s", ranked[chosenIdx].name);
          s_midiDetectedSuggestUntilMs = nowMs + 2000;
          s_midiSuggestStableUntilMs = usedStability ? (nowMs + 1800U) : 0U;
          s_midiSuggestLastSource = ev.source;
          s_midiSuggestLastChannel = ev.channel;
          s_midiSuggestLastAtMs = nowMs;
          for (int i = 0; i < 3; ++i) {
            if (static_cast<size_t>(i) < rankedN) {
              snprintf(s_midiSuggestTop3[i], sizeof(s_midiSuggestTop3[i]), "%s", ranked[i].name);
              s_midiSuggestTop3Score[i] = ranked[i].score;
            } else {
              s_midiSuggestTop3[i][0] = '\0';
              s_midiSuggestTop3Score[i] = 0;
            }
          }
          s_midiSuggestTop3UntilMs = nowMs + 4000;
        } else {
          s_midiDetectedSuggest[0] = '\0';
          s_midiDetectedSuggestUntilMs = 0;
          for (int i = 0; i < 3; ++i) {
            s_midiSuggestTop3[i][0] = '\0';
            s_midiSuggestTop3Score[i] = 0;
          }
          s_midiSuggestTop3UntilMs = 0;
          s_midiSuggestStableUntilMs = 0;
        }
      } else if (n == 0) {
        s_midiDetectedChord[0] = '\0';
        s_midiDetectedChordUntilMs = 0;
        s_midiDetectedSuggest[0] = '\0';
        s_midiDetectedSuggestUntilMs = 0;
        s_midiSuggestLastAtMs = 0;
        for (int i = 0; i < 3; ++i) {
          s_midiSuggestTop3[i][0] = '\0';
          s_midiSuggestTop3Score[i] = 0;
        }
        s_midiSuggestTop3UntilMs = 0;
        s_midiSuggestStableUntilMs = 0;
      }
    }
    return;
  }

  const uint8_t sourceRoute = midiSourceToRoute(ev.source);
  const bool followClock = g_settings.clkFollow != 0;
  const bool receiveFromThisSource = g_settings.midiTransportReceive == sourceRoute;
  const bool clockFromThisSource = g_settings.midiClockSource == sourceRoute;
  switch (ev.type) {
    case MidiEventType::Start:
      if (followClock && receiveFromThisSource) transportOnExternalStart(nowMs);
      break;
    case MidiEventType::Continue:
      if (followClock && receiveFromThisSource) transportOnExternalContinue(nowMs);
      break;
    case MidiEventType::Stop:
      if (followClock && receiveFromThisSource) {
        panicForTrigger(PanicTrigger::ExternalTransportStop);
        transportOnExternalStop();
      }
      break;
    case MidiEventType::SongPosition:
      if (followClock && receiveFromThisSource) transportOnExternalSongPosition(ev.value14);
      break;
    case MidiEventType::Clock:
      if (followClock && receiveFromThisSource && clockFromThisSource) transportOnExternalClockTick(nowMs);
      break;
    default:
      break;
  }
}

}  // namespace

// =====================================================================
//  SETUP / LOOP
// =====================================================================

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(1);
  M5.Display.setTextSize(2);
  prefsMigrateOnBoot();
  settingsLoad(g_settings);
  g_settings.normalize();
  applyBrightness();
  transportSetGlobalSwing(g_settings.globalSwingPct);
  transportSetGlobalHumanize(g_settings.globalHumanizePct);

  chordStateLoad(g_model);
  seqPatternLoad(g_seqPattern);
  seqExtrasLoad(&g_seqExtras);
  seqLaneChannelsLoad(g_seqMidiCh);
  chordVoicingLoad(&g_chordVoicing);
  bleMidiInit();
  dinMidiInit();
  xyMappingLoad(&g_xyOutChannel, &g_xyCcA, &g_xyCcB, &g_xyCurveA, &g_xyCurveB);
  projectBpmLoad(&g_projectBpm);
  projectCustomNameLoad(g_projectCustomName);
  lastProjectFolderLoad(g_lastProjectFolder);
  uiThemeLoad(&g_uiTheme);
  uiThemeApply(g_uiTheme);
  transportInit();
  srand(static_cast<unsigned>(esp_random()));
  (void)sdBackupInit();
  g_model.rebuildChords();
  drawPlaySurface();
}

void loop() {
  M5.update();
  bleMidiPoll();
  dinMidiPoll();

  const uint32_t now = millis();
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  const uint8_t touchCount = M5.Touch.getCount();

  MidiEvent midiEv{};
  while (midiIngressPollUsb(s_midiIngressParser, &midiEv)) {
    processIncomingMidiEvent(midiEv, now);
  }
  while (midiIngressPollBle(s_midiIngressParser, &midiEv)) {
    processIncomingMidiEvent(midiEv, now);
  }
  while (midiIngressPollDin(s_midiIngressParser, &midiEv)) {
    processIncomingMidiEvent(midiEv, now);
  }
  static uint32_t s_playMonitorAgeRefreshMs = 0;
  const bool playMonitorDetailedVisible =
      (g_screen == Screen::Play) && g_settings.playInMonitor && (g_settings.playInMonitorMode != 0) &&
      (s_playIngressInfo[0] != '\0') && (now < s_playIngressInfoUntilMs);
  if (playMonitorDetailedVisible && (now - s_playMonitorAgeRefreshMs >= 200U)) {
    s_playMonitorAgeRefreshMs = now;
    if (s_midiEventHistory.size() > 0) {
      MidiEventHistoryItem it{};
      if (s_midiEventHistory.getNewestFirst(0, &it)) {
        MidiEvent ev{};
        ev.source = it.source;
        ev.type = it.type;
        ev.channel = it.channel;
        ev.data1 = it.data1;
        ev.data2 = it.data2;
        ev.value14 = it.value14;
        playMonitorFormatLine(ev, s_playIngressInfo, sizeof(s_playIngressInfo), now);
        s_playIngressInfoDirty = true;
      }
    }
  }
  if (s_playIngressInfoDirty && g_screen == Screen::Play && g_settings.playInMonitor) {
    drawPlaySurface();
  }
  s_playIngressInfoDirty = false;

  xyFlushPending(now);
  seqArpProcessDue(now);
  transportTick(now);
  if (transportExternalClockActive() && g_settings.midiClockSource != 0 && g_settings.clkFollow != 0) {
    const uint16_t extBpm = transportExternalClockBpm();
    if (extBpm >= 40 && extBpm <= 300 && extBpm != g_projectBpm) {
      g_projectBpm = extBpm;
    }
  }

  static uint8_t s_prevAudible = 255;
  static uint8_t s_prevCntIn = 255;
  if (transportIsPlaying()) {
    const uint8_t au = transportAudibleStep();
    if (au != s_prevAudible) {
      s_prevAudible = au;
      transportEmitSequencerStepMidi(au);
      seqArpProcessDue(now);
      const uint8_t flashDiv = g_settings.clkFlashEdge ? 2U : 4U;
      if (transportExternalClockActive() && (au % flashDiv) == 0U) {
        s_playClockFlashUntilMs = now + 90;
      }
      if (g_screen == Screen::Sequencer) {
        drawSequencerSurface();
      }
      if (g_screen == Screen::Transport) {
        drawTransportSurface();
      }
      if (g_screen == Screen::Play && g_settings.midiClockSource != 0 && g_settings.clkFollow != 0) {
        drawPlaySurface();
      }
    }
  } else {
    s_prevAudible = 255;
    s_playClockFlashUntilMs = 0;
    seqArpStopAll(false);
  }
  if (transportIsCountIn()) {
    const uint8_t cn = transportCountInNumber();
    if (cn != s_prevCntIn) {
      s_prevCntIn = cn;
      if (g_screen == Screen::Play) {
        drawPlaySurface();
      }
      if (g_screen == Screen::Transport) {
        drawTransportSurface();
      }
    }
  } else {
    s_prevCntIn = 255;
  }

  if (g_screen == Screen::Play || g_screen == Screen::PlayCategory || g_screen == Screen::Sequencer ||
      g_screen == Screen::XyPad || g_screen == Screen::Transport || g_screen == Screen::MidiDebug ||
      g_screen == Screen::TransportBpmEdit || g_screen == Screen::TransportTapTempo) {
    layoutBottomBezels(w, h);
    updateShiftHoldState(touchCount);
    if (checkBezelLongPressSettings(touchCount)) {
      wasTouchActive = false;
      drawSettingsUi();
      delay(10);
      return;
    }
  } else {
    s_shiftActive = false;
    s_seqSelectHeld = false;
    s_shiftSelectDown = false;
    s_shiftSelectDownMs = 0;
  }

  switch (g_screen) {
    case Screen::Play:
      processPlayTouch(touchCount, w, h);
      break;
    case Screen::PlayCategory:
      processPlayCategoryTouch(touchCount, w, h);
      break;
    case Screen::Sequencer:
      processSequencerTouch(touchCount, w, h);
      break;
    case Screen::XyPad:
      processXyTouch(touchCount, w, h);
      break;
    case Screen::MidiDebug:
      processMidiDebugTouch(touchCount, w, h);
      break;
    case Screen::Transport:
      processTransportTouch(touchCount, w, h);
      break;
    case Screen::TransportBpmEdit:
      processTransportBpmEditTouch(touchCount, w, h);
      break;
    case Screen::TransportTapTempo:
      processTransportTapTempoTouch(touchCount, w, h);
      break;
    case Screen::KeyPicker:
      processKeyPickerTouch(touchCount, w, h);
      break;
    case Screen::Settings:
      processSettingsTouch(touchCount, w, h);
      break;
    case Screen::ProjectNameEdit:
      processProjectNameEditTouch(touchCount, w, h);
      break;
    case Screen::SdProjectPick:
      processSdProjectPickTouch(touchCount, w, h);
      break;
  }

  delay(10);
}
