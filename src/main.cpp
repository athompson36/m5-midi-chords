#include <M5Unified.h>
#include <WiFi.h>
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
#include "MidiMmc.h"
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
#include "UsbMidiTransport.h"
#include "ui/UiTypes.h"
#include "ui/UiLayout.h"
#include "ui/UiGloss.h"
#include "ui/UiDrawers.h"

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

// File-scope state shared with screen modules / top-drawer processors (external linkage).
AppSettings g_settings;
uint8_t g_chordVoicing = 4;
bool wasTouchActive = false;
int g_lastTouchX = 0;
int g_lastTouchY = 0;
::ui::TopDrawerState g_topDrawerUi{};

namespace {

using Rect = ::ui::Rect;

uint16_t colorForRole(ChordRole role) {
  switch (role) {
    case ChordRole::Principal: return g_uiPalette.principal;
    case ChordRole::Standard: return g_uiPalette.standard;
    case ChordRole::Tension: return g_uiPalette.tension;
    case ChordRole::Surprise: return g_uiPalette.surprise;
  }
  return g_uiPalette.subtle;
}

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
void drawMidiDebugSurface(bool fullClear = true);
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
Screen g_screen = Screen::Play;

String g_lastAction = "";

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

enum class SettingsPanel : uint8_t { Menu, Midi, Display, SeqArp, Storage, UserGuide };

SettingsPanel g_settingsPanel = SettingsPanel::Menu;
int g_settingsCursorRow = 0;
int g_settingsListScroll = 0;
static int g_userGuideScroll = 0;


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
static int s_seqChordDropTouchStartX = 0;
static int s_seqChordDropTouchStartY = 0;
static int s_seqChordDropScroll = 0;
static int s_seqChordDropDragLastY = -1;
static int s_seqChordDropFingerScrollCount = 0;
static int s_seqStepEditStep = -1;
static bool s_seqStepEditJustOpened = false;
static Rect g_seqStepEditPanel{};
static Rect g_seqStepEditVoMinus{}, g_seqStepEditVoPlus{};
static Rect g_seqStepEditPatMinus{}, g_seqStepEditPatPlus{};
static Rect g_seqStepEditRateMinus{}, g_seqStepEditRatePlus{};
static Rect g_seqStepEditOctMinus{}, g_seqStepEditOctPlus{};
static Rect g_seqStepEditGateMinus{}, g_seqStepEditGatePlus{};
static Rect g_seqStepEditArpToggle{};
static Rect g_seqStepEditDone{};
static Rect g_seqStepEditDelete{};
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
/// Play category: last painted finger cell (-9999 = force full paint on next change).
static int s_playCategoryFingerPainted = -9999;
static bool s_playChordTriggeredThisTouch = false;
static uint8_t s_playChordActiveCh0 = 0;

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

static bool seqSelectToolsActive() {
  return s_seqSelectHeld || s_shiftActive;
}
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
// Locked hit target for current touch gesture (prevents noisy cross-tile bouncing).
static int s_playTouchLockedHit = -9999;
/// Last painted finger highlight (synced from partial redraws and full `drawPlaySurface`).
static int s_playFingerVisualChord = -100;
static bool s_playFingerVisualOnKey = false;
static int s_lastSeqPreviewCell = -9999;
static bool s_wasSeqMultiFinger = false;
static int s_lastDrawnSeqComboTab = -99999;
static int8_t s_settingsDropRowId = -1;
static int s_settingsDropOptScroll = 0;
static int s_settingsDropDragLastY = -1;
static int s_settingsDropFingerScrollCount = 0;
static int s_settingsTouchStartX = 0;
static int s_settingsTouchStartY = 0;
static int s_keyPickTouchStartX = 0;
static int s_keyPickTouchStartY = 0;
static int s_sdPickTouchStartX = 0;
static int s_sdPickTouchStartY = 0;

/// Transport drawer: swipe up from select to open, stores screen to return to.
static Screen s_screenBeforeTransport = Screen::Play;
static bool s_selectSwipeTracking = false;
static int s_selectSwipeStartY = 0;
static constexpr int kSwipeUpThresholdPx = 40;
// True right after swipe-opening Transport; ignore carried touch until finger-up.
static bool s_transportIgnoreUntilTouchUp = false;

/// Movement past this distance from touch-down suppresses row pick (scroll vs tap).
static constexpr int kUiScrollSuppressPickPx = 14;

static bool touchMovedPastSuppressThreshold(int sx, int sy, int ex, int ey) {
  const int dx = ex - sx;
  const int dy = ey - sy;
  const int t = kUiScrollSuppressPickPx;
  return (static_cast<int32_t>(dx) * dx + static_cast<int32_t>(dy) * dy) > (t * t);
}
static int8_t s_xyTouchZone = -1;
static bool s_xyTwoFingerSurfaceDrawn = false;
static MidiIngressParser s_midiIngressParser;
static MidiInState s_midiInState;
static MidiEventHistory s_midiEventHistory;
static uint32_t s_playClockFlashUntilMs = 0;
static char s_midiDetectedChord[12] = "";
static uint32_t s_midiDetectedChordUntilMs = 0;
static char s_surpriseChordLabel[12] = "";
static bool s_surpriseChordActive = false;
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
static uint32_t s_playAutoSilenceAtMs = 0;
static uint8_t s_playAutoSilenceCh0 = 0;
static bool s_transportTxPrevPlaying = false;
static bool s_transportTxPrevRunning = false;
static uint32_t s_transportTxNextClockMs = 0;
static Rect g_midiDbgSourceChip{};
static Rect g_midiDbgTypeChip{};
static Rect g_midiDbgResetChip{};
static uint32_t s_panicTriggerCounts[static_cast<size_t>(PanicTrigger::Count)] = {};

static void schedulePlayAutoSilence(uint8_t channel0) {
  s_playAutoSilenceCh0 = static_cast<uint8_t>(channel0 & 0x0F);
  s_playAutoSilenceAtMs = millis() + 220U;
}

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
  uint8_t gatePct = 80;
  bool noteActive = false;
  uint32_t noteOffMs = 0;
};

static SeqArpLaneRuntime s_seqArpRt[3];

static void seqArpStopLane(uint8_t lane, bool silence) {
  if (lane > 2) return;
  SeqArpLaneRuntime& rt = s_seqArpRt[lane];
  if (silence) {
    midiSilenceLastChord(rt.channel0 & 0x0F);
  }
  rt.active = false;
  rt.noteActive = false;
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
    if (rt.noteActive && static_cast<int32_t>(nowMs - rt.noteOffMs) >= 0) {
      midiSilenceLastChord(rt.channel0 & 0x0F);
      rt.noteActive = false;
    }
    while (rt.active && static_cast<int32_t>(nowMs - rt.nextMs) >= 0) {
      if (rt.emitPos >= rt.orderCount || static_cast<int32_t>(rt.nextMs - rt.endMs) >= 0) {
        if (rt.noteActive) {
          midiSilenceLastChord(rt.channel0 & 0x0F);
          rt.noteActive = false;
        }
        rt.active = false;  // Truncate policy: never spill into next step.
        break;
      }
      const uint8_t idx = rt.orderIdx[rt.emitPos];
      if (idx >= rt.noteCount) {
        if (rt.noteActive) {
          midiSilenceLastChord(rt.channel0 & 0x0F);
          rt.noteActive = false;
        }
        rt.active = false;
        break;
      }
      midiSilenceLastChord(rt.channel0 & 0x0F);
      midiSendNoteOnTracked(rt.channel0 & 0x0F, rt.notes[idx], applyOutputVelocityCurve(g_settings.outputVelocity));
      rt.noteActive = true;
      uint32_t gateMs = (rt.intervalMs * static_cast<uint32_t>(rt.gatePct)) / 100U;
      if (gateMs == 0) gateMs = 1;
      rt.noteOffMs = rt.nextMs + gateMs;
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

    uint8_t vcap = g_seqExtras.stepVoicing[lane][step & 0x0F];
    if (vcap < 1 || vcap > 4) {
      vcap = g_chordVoicing;
    }
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
        const int octSpread = static_cast<int>(g_seqExtras.arpOctRange[lane][step & 0x0F] > 2
                                                   ? 2
                                                   : g_seqExtras.arpOctRange[lane][step & 0x0F]);
        int octAdd = 0;
        if (octSpread > 0) {
          const int span = octSpread * 2 + 1;
          octAdd = (rand() % span) - octSpread;
        }
        int note = rootMidi + static_cast<int>(ordered[i]) + static_cast<int>(g_settings.transposeSemitones) +
                   (12 * octAdd);
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
      rt.gatePct = g_seqExtras.arpGatePct[lane][step & 0x0F];
      if (rt.gatePct < 10) rt.gatePct = 10;
      if (rt.gatePct > 100) rt.gatePct = 100;
      rt.noteActive = false;
      rt.noteOffMs = 0;
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
void drawSequencerSurface(int fingerCell = -1, bool fullClear = true);
void drawXyPadSurface(bool fullClear = true);
void paintAppTopDrawer();
void drawXyTopDrawer(::ui::DisplayAdapter& d);
void drawPlayTopDrawer(::ui::DisplayAdapter& d);
void drawSequencerTopDrawer(::ui::DisplayAdapter& d);
bool playProcessTopDrawerTouch(uint8_t touchCount, int w, int h);
bool sequencerProcessTopDrawerTouch(uint8_t touchCount, int w, int h);

void drawTransportSurface(bool fullClear = true);
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
  s_settingsDropDragLastY = -1;
  s_settingsDropFingerScrollCount = 0;
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
  ::ui::DisplayAdapter d = ::ui::displayAdapterForM5();
  ::ui::GlossButtonSpec spec{};
  spec.bounds = r;
  spec.label = label;
  spec.textSize = textSize;
  spec.customBase565 = bg;
  spec.role = ::ui::GlossRole::Neutral;
  spec.state = ::ui::GlossState::Idle;
  spec.radius = static_cast<uint8_t>(max(4, min(r.w, r.h) / 8));
  ::ui::drawGlossButton(d, spec);
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
  const bool onTransport = (g_screen == Screen::Transport || g_screen == Screen::TransportBpmEdit ||
                            g_screen == Screen::TransportTapTempo);
  if (onTransport) {
    M5.Display.drawString("CLOSE", g_bezelBack.x + g_bezelBack.w / 2, h - 2);
    M5.Display.drawString("CLOSE", g_bezelSelect.x + g_bezelSelect.w / 2, h - 2);
    M5.Display.drawString("CLOSE", g_bezelFwd.x + g_bezelFwd.w / 2, h - 2);
  } else {
    M5.Display.drawString("BACK", g_bezelBack.x + g_bezelBack.w / 2, h - 2);
    M5.Display.drawString(s_shiftActive ? "SHIFT" : "SELECT", g_bezelSelect.x + g_bezelSelect.w / 2, h - 2);
    M5.Display.drawString("FWD", g_bezelFwd.x + g_bezelFwd.w / 2, h - 2);
  }
}

// Top-right: battery + optional external BPM, then connection glyphs (USB, BLE, DIN, WiFi) when active.
// DIN: wide hold so very slow / sparse MIDI clock (long gaps between 0xF8 ticks) still reads as active.
static constexpr uint32_t kStatusDinTrafficHoldMs = 5000;

static void drawStatusGlyphUsb(int x0, int y0, uint16_t c) {
  M5.Display.drawRect(x0 + 2, y0 + 3, 4, 4, c);
  M5.Display.drawFastVLine(x0 + 4, y0 + 1, 2, c);
  M5.Display.drawFastHLine(x0 + 1, y0 + 7, 7, c);
}

static void drawStatusGlyphBle(int x0, int y0, uint16_t c) {
  M5.Display.drawFastVLine(x0 + 4, y0 + 1, 6, c);
  M5.Display.drawFastHLine(x0 + 1, y0 + 2, 4, c);
  M5.Display.drawFastHLine(x0 + 1, y0 + 5, 4, c);
  M5.Display.drawFastHLine(x0 + 5, y0 + 3, 3, c);
}

static void drawStatusGlyphDin(int x0, int y0, uint16_t c) {
  M5.Display.drawCircle(x0 + 4, y0 + 4, 3, c);
  M5.Display.drawFastVLine(x0 + 4, y0 + 7, 2, c);
}

static void drawStatusGlyphWifi(int x0, int y0, uint16_t c) {
  M5.Display.fillRect(x0 + 4, y0 + 7, 1, 1, c);
  M5.Display.drawPixel(x0 + 3, y0 + 6, c);
  M5.Display.drawPixel(x0 + 5, y0 + 6, c);
  M5.Display.drawPixel(x0 + 2, y0 + 5, c);
  M5.Display.drawPixel(x0 + 6, y0 + 5, c);
  M5.Display.drawPixel(x0 + 1, y0 + 4, c);
  M5.Display.drawPixel(x0 + 7, y0 + 4, c);
}

static void drawTopSystemStatus(int w, int yTop, const char* extBpmText, uint16_t extBpmColor) {
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_right);
  M5.Display.setTextSize(1);

  int x = w - 4;
  const uint16_t ink = g_uiPalette.rowNormal;

  int32_t bat = M5.Power.getBatteryLevel();
  if (bat < 0) {
    bat = -1;
  } else if (bat > 100) {
    bat = 100;
  }
  uint16_t bcol = ink;
  if (bat >= 0 && bat < 15) {
    bcol = g_uiPalette.danger;
  } else if (bat >= 0 && bat < 35) {
    bcol = g_uiPalette.accentPress;
  }

  constexpr int kBw = 17;
  constexpr int kBh = 7;
  const int bx = x - kBw;
  const int by = yTop + 1;
  M5.Display.drawRect(bx, by, kBw, kBh, bcol);
  M5.Display.fillRect(bx + kBw - 1, by + 2, 2, kBh - 4, bcol);
  if (bat >= 0) {
    const int innerW = kBw - 4;
    const int fillW = max(0, innerW * static_cast<int>(bat) / 100);
    if (fillW > 0) {
      M5.Display.fillRect(bx + 2, by + 2, fillW, kBh - 4, bcol);
    }
  }
  x = bx - 3;

  char pct[8];
  if (bat >= 0) {
    snprintf(pct, sizeof(pct), "%d%%", static_cast<int>(bat));
  } else {
    snprintf(pct, sizeof(pct), "--");
  }
  M5.Display.setTextColor(bcol, g_uiPalette.bg);
  const int pctW = M5.Display.textWidth(pct);
  M5.Display.drawString(pct, x, yTop);
  x -= pctW + 5;

  if (extBpmText && extBpmText[0] != '\0') {
    M5.Display.setTextColor(extBpmColor, g_uiPalette.bg);
    const int bpmW = M5.Display.textWidth(extBpmText);
    M5.Display.drawString(extBpmText, x, yTop);
    x -= bpmW + 6;
  }

  const bool wifiOn = (WiFi.status() == WL_CONNECTED);
  const bool dinOn = dinMidiRecentTraffic(kStatusDinTrafficHoldMs);
  const bool bleOn = bleMidiConnected();
  const bool usbOn = usbMidiHostConnected();

  auto placeIcon = [&](void (*drawFn)(int, int, uint16_t)) {
    drawFn(x - 9, yTop, ink);
    x -= 10;
  };
  if (wifiOn) {
    placeIcon(drawStatusGlyphWifi);
  }
  if (dinOn) {
    placeIcon(drawStatusGlyphDin);
  }
  if (bleOn) {
    placeIcon(drawStatusGlyphBle);
  }
  if (usbOn) {
    placeIcon(drawStatusGlyphUsb);
  }
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
    }
  } else {
    if (s_shiftActive) {
      s_shiftActive = false;
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
  constexpr int hintH = 0;
  constexpr int padAfterHint = 4;
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
  constexpr int hintH = 0;
  constexpr int tabH = 20;
  constexpr int tabGap = 4;
  const int tabY = hintH + 4;
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
    M5.Display.setFont(nullptr);
    M5.Display.setTextColor(TFT_WHITE, g_uiPalette.heart);
    const int cx = g_keyRect.x + g_keyRect.w / 2;
    const int cy = g_keyRect.y + g_keyRect.h / 2;
    const uint8_t mainSz = (cell >= 52) ? 2 : 1;
    M5.Display.setTextSize(mainSz);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("\x03 Tap!", cx, cy);
  } else if (s_surpriseChordActive && s_surpriseChordLabel[0] != '\0') {
    const int rad = max(4, min(g_keyRect.w, g_keyRect.h) / 8);
    M5.Display.fillRoundRect(g_keyRect.x, g_keyRect.y, g_keyRect.w, g_keyRect.h, rad,
                             g_uiPalette.surprise);
    M5.Display.drawRoundRect(g_keyRect.x, g_keyRect.y, g_keyRect.w, g_keyRect.h, rad, TFT_WHITE);
    M5.Display.setFont(nullptr);
    M5.Display.setTextColor(TFT_WHITE, g_uiPalette.surprise);
    const int cx = g_keyRect.x + g_keyRect.w / 2;
    const int cy = g_keyRect.y + g_keyRect.h / 2;
    const uint8_t mainSz = (cell >= 52) ? 2 : 1;
    M5.Display.setTextSize(mainSz);
    M5.Display.setTextDatum(bottom_center);
    M5.Display.drawString(s_surpriseChordLabel, cx, cy - 1);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_center);
    M5.Display.drawString("surprise!", cx, cy + 1);
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

// Clear the padded area around a single cell (outline ring spill) and nothing more.
static void playClearCellPadded(const Rect& r, int maxW, int maxY) {
  constexpr int kPad = 4;
  const int x1 = max(0, r.x - kPad);
  const int y1 = max(0, r.y - kPad);
  const int x2 = min(maxW, r.x + r.w + kPad);
  const int y2 = min(maxY, r.y + r.h + kPad);
  if (x2 > x1 && y2 > y1) {
    M5.Display.fillRect(x1, y1, x2 - x1, y2 - y1, g_uiPalette.bg);
  }
}

// Redraw all play grid cells with per-cell clearing (no band-wide flash).
static void playRedrawAllCellsInPlace(int fingerChord = -100, bool fingerOnKey = false) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  computePlaySurfaceLayout(w, h);
  const int maxY = h - kBezelBarH;
  const bool showLp = fingerChord < -50;

  // Clear the whole pad grid in one pass to avoid cell-by-cell visual hopping.
  Rect band = playSurfaceGridBoundsPadded(4);
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

  drawPlayKeyCell(fingerOnKey, showLp);
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    drawPlayChordCell(i, fingerChord, showLp);
  }
  s_playFingerVisualChord = fingerChord;
  s_playFingerVisualOnKey = fingerOnKey;
  if (s_playVoicingPanelOpen) {
    layoutPlayVoicingPanel(w, h);
    drawPlayVoicingPanelOverlay();
  }
}

// Legacy wrapper: same as playRedrawAllCellsInPlace with defaults.
static void playRedrawGridBand() {
  playRedrawAllCellsInPlace(-100, false);
}

static void playRedrawAfterOutlineChange(int previousOutline, int fingerChord = -100, bool fingerOnKey = false) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  computePlaySurfaceLayout(w, h);
  const int maxY = h - kBezelBarH;
  const bool showLp = fingerChord < -50;

  bool redrawKey = (previousOutline == -2) || (g_lastPlayedOutline == -2) || fingerOnKey;
  bool redrawChord[ChordModel::kSurroundCount] = {};
  if (previousOutline >= 0 && previousOutline < ChordModel::kSurroundCount) {
    redrawChord[previousOutline] = true;
  }
  if (g_lastPlayedOutline >= 0 && g_lastPlayedOutline < ChordModel::kSurroundCount) {
    redrawChord[g_lastPlayedOutline] = true;
  }
  if (fingerChord >= 0 && fingerChord < ChordModel::kSurroundCount) {
    redrawChord[fingerChord] = true;
  }

  M5.Display.startWrite();
  if (redrawKey) {
    playClearCellPadded(g_keyRect, w, maxY);
    drawPlayKeyCell(fingerOnKey, showLp);
  }
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    if (!redrawChord[i]) continue;
    playClearCellPadded(g_chordRects[i], w, maxY);
    drawPlayChordCell(i, fingerChord, showLp);
  }
  s_playFingerVisualChord = fingerChord;
  s_playFingerVisualOnKey = fingerOnKey;
  drawBezelBarStrip();
  M5.Display.endWrite();
}

static void playRedrawInPlace(int fingerChord, bool fingerOnKey);
static void playRedrawFingerHighlightOnly(int fingerChord, bool fingerOnKey);

static void playRedrawClearFingerHighlight() {
  playRedrawFingerHighlightOnly(-100, false);
}

// Updates finger highlight on chord pads / key without full-panel fillRect (see §2 UI backlog).
static void playRedrawFingerHighlightOnly(int fingerChord, bool fingerOnKey) {
  if (transportIsCountIn() || transportIsRecordingLive() || s_playVoicingPanelOpen) {
    playRedrawInPlace(fingerChord, fingerOnKey);
    return;
  }
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  computePlaySurfaceLayout(w, h);

  const bool showLpNew = fingerChord < -50;
  const bool showLpOld = s_playFingerVisualChord < -50;

  const int oldFc = s_playFingerVisualChord;
  const bool oldOk = s_playFingerVisualOnKey;
  constexpr int kPad = 4;
  const int maxY = h - kBezelBarH;

  M5.Display.startWrite();
  auto clearFill = [&](const Rect& r) {
    const int x1 = max(0, r.x - kPad);
    const int y1 = max(0, r.y - kPad);
    const int x2 = min(w, r.x + r.w + kPad);
    const int y2 = min(maxY, r.y + r.h + kPad);
    if (x2 > x1 && y2 > y1) {
      M5.Display.fillRect(x1, y1, x2 - x1, y2 - y1, g_uiPalette.bg);
    }
  };

  if (oldOk != fingerOnKey) {
    clearFill(g_keyRect);
    drawPlayKeyCell(fingerOnKey, showLpNew);
  }
  if (showLpOld != showLpNew && g_lastPlayedOutline == -2) {
    clearFill(g_keyRect);
    drawPlayKeyCell(fingerOnKey, showLpNew);
  }

  bool chordMark[ChordModel::kSurroundCount] = {};
  if (oldFc >= 0 && oldFc < ChordModel::kSurroundCount) {
    chordMark[oldFc] = true;
  }
  if (fingerChord >= 0 && fingerChord < ChordModel::kSurroundCount) {
    chordMark[fingerChord] = true;
  }
  if (showLpOld != showLpNew && g_lastPlayedOutline >= 0 &&
      g_lastPlayedOutline < ChordModel::kSurroundCount) {
    chordMark[g_lastPlayedOutline] = true;
  }
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    if (!chordMark[i]) continue;
    clearFill(g_chordRects[i]);
    drawPlayChordCell(i, fingerChord, showLpNew);
  }

  s_playFingerVisualChord = fingerChord;
  s_playFingerVisualOnKey = fingerOnKey;
  drawBezelBarStrip();
  M5.Display.endWrite();
}

/// IN / SUG / ingress line + REC tag + top-right status (same painting as end of `drawPlaySurface`).
static void playPaintTopOverlays(int w) {
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
    M5.Display.drawString(lab, 4, 4);
  }
  if (s_midiDetectedSuggest[0] != '\0' && millis() < s_midiDetectedSuggestUntilMs) {
    char sug[20];
    snprintf(sug, sizeof(sug), "SUG %s", s_midiDetectedSuggest);
    M5.Display.setFont(nullptr);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
    M5.Display.drawString(sug, 4, 14);
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
    M5.Display.drawString(s_playIngressInfo, 4, 24);
  }

  const bool extClockSelected = (g_settings.midiClockSource != 0) && (g_settings.clkFollow != 0);
  const bool extClockActive = transportExternalClockActive();
  char bpmCorner[8];
  const char* extBpmPtr = nullptr;
  uint16_t extBpmCol = g_uiPalette.subtle;
  if (extClockSelected) {
    const uint16_t extBpm = transportExternalClockBpm();
    if (extClockActive && extBpm >= 40 && extBpm <= 300) {
      snprintf(bpmCorner, sizeof(bpmCorner), "%u", static_cast<unsigned>(extBpm));
    } else {
      snprintf(bpmCorner, sizeof(bpmCorner), "--");
    }
    extBpmPtr = bpmCorner;
    const uint32_t nowMs = millis();
    const bool flashOn = nowMs < s_playClockFlashUntilMs;
    extBpmCol = flashOn ? g_uiPalette.rowNormal : g_uiPalette.subtle;
  }
  drawTopSystemStatus(w, 2, extBpmPtr, extBpmCol);
}

/// Clears only the margin above the chord grid and repaints top overlays (cheap vs full play redraw).
static void playRedrawTopMarginOverlays() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  computePlaySurfaceLayout(w, h);
  int minGridY = g_keyRect.y;
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    minGridY = min(minGridY, g_chordRects[i].y);
  }
  const int stripH = max(0, minGridY - 1);
  if (stripH < 10) {
    return;
  }
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, stripH, g_uiPalette.bg);
  playPaintTopOverlays(w);
  M5.Display.endWrite();
}

// Full content repaint of the Play screen without a full-area clear.
// Each cell clears only its own padded area; top strip is cleared for overlays.
// Use this for in-screen updates (chord taps, mode toggles, etc.) to avoid flash.
static void playRedrawInPlace(int fingerChord = -100, bool fingerOnKey = false) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  computePlaySurfaceLayout(w, h);
  M5.Display.startWrite();

  playRedrawAllCellsInPlace(fingerChord, fingerOnKey);

  if (transportIsCountIn()) {
    char cn[8];
    snprintf(cn, sizeof(cn), "%u", (unsigned)transportCountInNumber());
    M5.Display.setFont(nullptr);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(4);
    M5.Display.setTextColor(g_uiPalette.accentPress, g_uiPalette.bg);
    M5.Display.drawString(cn, w / 2, h / 2 - 24);
  }

  int minGridY = g_keyRect.y;
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    minGridY = min(minGridY, g_chordRects[i].y);
  }
  const int stripH = max(0, minGridY - 1);
  if (stripH >= 10) {
    M5.Display.fillRect(0, 0, w, stripH, g_uiPalette.bg);
  }
  playPaintTopOverlays(w);

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
  playPaintTopOverlays(w);

  drawBezelBarStrip();
  M5.Display.endWrite();
  s_playFingerVisualChord = fingerChord;
  s_playFingerVisualOnKey = fingerOnKey;
  paintAppTopDrawer();
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

static bool playTriggerSurroundByIdx(int hit, bool autoSilence = true) {
  if (hit < 0 || hit >= ChordModel::kSurroundCount) return false;
  s_surpriseChordActive = false;
  const bool heartBefore = g_model.heartAvailable;
  const uint8_t mch = static_cast<uint8_t>(g_settings.midiOutChannel - 1);
  const uint8_t vel = applyOutputVelocityCurve(g_settings.outputVelocity);
  uint8_t vcap = g_chordVoicing;
  if (vcap < 1) vcap = 1;
  if (vcap > 4) vcap = 4;
  midiPlaySurroundPad(g_model, hit, mch, vel, vcap, g_settings.arpeggiatorMode, g_settings.transposeSemitones);
  if (autoSilence) {
    schedulePlayAutoSilence(mch);
  }
  g_lastPlayedOutline = hit;
  g_model.registerPlay();
  playHistoryRecord(static_cast<uint8_t>(hit));
  transportSetLiveChord(static_cast<int8_t>(hit));
  return !heartBefore && g_model.heartAvailable;
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
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString(playCategoryTitle(s_playCategoryPage), 4, 4);

  constexpr int pad = 6;
  const int gridTop = 22;
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
  drawTopSystemStatus(w, 2, nullptr, 0);
  drawBezelBarStrip();
  M5.Display.endWrite();
}

int hitTestPlayCategoryCell(int px, int py) {
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    if (pointInRectPad(px, py, g_playCategoryCells[i], kPlayCategoryHitPadPx)) return i;
  }
  return -1;
}

// Redraw PlayCategory cells in-place (per-cell clear, no full-area flash).
static void playCategoryRedrawInPlace(int fingerCell = -1) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutBottomBezels(w, h);
  playCategoryBuildItems();
  const int maxY = h - kBezelBarH;
  M5.Display.startWrite();

  constexpr int kPad = 4;
  constexpr int pad = 6;
  const int gridTop = 22;
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
    playClearCellPadded(g_playCategoryCells[i], w, maxY);
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

  M5.Display.fillRect(0, 0, w, gridTop, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString(playCategoryTitle(s_playCategoryPage), 4, 4);
  drawTopSystemStatus(w, 2, nullptr, 0);
  drawBezelBarStrip();
  M5.Display.endWrite();
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

static int seqChordDropVisibleRows(int h) {
  const int maxH = h - kBezelBarH - 12;
  constexpr int rowH = 28;
  int vis = max(3, maxH / rowH);
  const int items = seqChordDropItemCount();
  if (vis > items) vis = items;
  return vis;
}

static void seqChordDropComputeLayout(int w, int h, int* outX, int* outY, int* outW, int* outRowH, int* outVis) {
  constexpr int rowH = 28;
  const int vis = seqChordDropVisibleRows(h);
  const int totalH = rowH * vis;
  const int dropW = min(172, max(136, w - 20));
  const Rect& anchor = g_seqCellRects[s_seqChordDropStep];
  int dx = anchor.x + anchor.w / 2 - dropW / 2;
  if (dx < 8) dx = 8;
  if (dx + dropW > w - 8) dx = w - 8 - dropW;
  int dy = (h - kBezelBarH - totalH) / 2;
  if (dy < 4) dy = 4;
  *outX = dx;
  *outY = dy;
  *outW = dropW;
  *outRowH = rowH;
  *outVis = vis;
}

static void seqStepEditComputeLayout(int w, int h) {
  const int pw = min(w - 12, 228);
  const int ph = 188;
  const int px = (w - pw) / 2;
  const int py = max(4, (h - kBezelBarH - ph) / 2);
  g_seqStepEditPanel = {px, py, pw, ph};
  const int rowVo = py + 22;
  const int rowArp = py + 44;
  const int rowPat = py + 66;
  const int rowRate = py + 88;
  const int rowOct = py + 110;
  const int rowGate = py + 132;
  g_seqStepEditVoMinus = {px + 8, rowVo, 28, 20};
  g_seqStepEditVoPlus = {px + pw - 36, rowVo, 28, 20};
  g_seqStepEditArpToggle = {px + pw / 2 - 34, rowArp, 68, 20};
  g_seqStepEditPatMinus = {px + 8, rowPat, 28, 20};
  g_seqStepEditPatPlus = {px + pw - 36, rowPat, 28, 20};
  g_seqStepEditRateMinus = {px + 8, rowRate, 28, 20};
  g_seqStepEditRatePlus = {px + pw - 36, rowRate, 28, 20};
  g_seqStepEditOctMinus = {px + 8, rowOct, 28, 20};
  g_seqStepEditOctPlus = {px + pw - 36, rowOct, 28, 20};
  g_seqStepEditGateMinus = {px + 8, rowGate, 28, 20};
  g_seqStepEditGatePlus = {px + pw - 36, rowGate, 28, 20};
  g_seqStepEditDelete = {px + 14, py + ph - 24, 76, 18};
  g_seqStepEditDone = {px + pw - 90, py + ph - 24, 76, 18};
}

static void drawSeqStepEditPopup() {
  if (s_seqStepEditStep < 0 || s_seqStepEditStep >= 16) return;
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  seqStepEditComputeLayout(w, h);
  const uint8_t lane = seqLaneClamped();
  const int step = s_seqStepEditStep;
  uint8_t vo = g_seqExtras.stepVoicing[lane][step];
  if (vo < 1) vo = 1;
  if (vo > 4) vo = 4;
  const bool arpOn = g_seqExtras.arpEnabled[lane][step] != 0U;
  const uint8_t pat = g_seqExtras.arpPattern[lane][step] & 0x03U;
  const uint8_t rate = g_seqExtras.arpClockDiv[lane][step] & 0x03U;
  const uint8_t oct = g_seqExtras.arpOctRange[lane][step] > 2 ? 2 : g_seqExtras.arpOctRange[lane][step];
  uint8_t gate = g_seqExtras.arpGatePct[lane][step];
  if (gate < 10) gate = 10;
  if (gate > 100) gate = 100;

  M5.Display.fillRoundRect(g_seqStepEditPanel.x, g_seqStepEditPanel.y, g_seqStepEditPanel.w, g_seqStepEditPanel.h, 8,
                           g_uiPalette.panelMuted);
  M5.Display.drawRoundRect(g_seqStepEditPanel.x, g_seqStepEditPanel.y, g_seqStepEditPanel.w, g_seqStepEditPanel.h, 8,
                           g_uiPalette.settingsBtnBorder);
  M5.Display.setFont(nullptr);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.panelMuted);
  char hdr[24];
  snprintf(hdr, sizeof(hdr), "Step %u", (unsigned)(step + 1));
  M5.Display.drawString(hdr, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2, g_seqStepEditPanel.y + 4);

  drawRoundedButton(g_seqStepEditVoMinus, g_uiPalette.accentPress, "-", 2);
  drawRoundedButton(g_seqStepEditVoPlus, g_uiPalette.accentPress, "+", 2);
  drawRoundedButton(g_seqStepEditArpToggle, arpOn ? g_uiPalette.seqLaneTab : g_uiPalette.panelMuted,
                    arpOn ? "ARP On" : "ARP Off", 1);
  drawRoundedButton(g_seqStepEditPatMinus, g_uiPalette.accentPress, "-", 2);
  drawRoundedButton(g_seqStepEditPatPlus, g_uiPalette.accentPress, "+", 2);
  drawRoundedButton(g_seqStepEditRateMinus, g_uiPalette.accentPress, "-", 2);
  drawRoundedButton(g_seqStepEditRatePlus, g_uiPalette.accentPress, "+", 2);
  drawRoundedButton(g_seqStepEditOctMinus, g_uiPalette.accentPress, "-", 2);
  drawRoundedButton(g_seqStepEditOctPlus, g_uiPalette.accentPress, "+", 2);
  drawRoundedButton(g_seqStepEditGateMinus, g_uiPalette.accentPress, "-", 2);
  drawRoundedButton(g_seqStepEditGatePlus, g_uiPalette.accentPress, "+", 2);
  drawRoundedButton(g_seqStepEditDelete, g_uiPalette.danger, "Delete", 1);
  drawRoundedButton(g_seqStepEditDone, g_uiPalette.seqLaneTab, "Done", 1);

  char line[40];
  M5.Display.setTextDatum(middle_center);
  snprintf(line, sizeof(line), "Voicing: V%u", (unsigned)vo);
  M5.Display.drawString(line, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2, g_seqStepEditVoMinus.y + 10);
  snprintf(line, sizeof(line), "Pattern: %s", kArpPatLabs[pat]);
  M5.Display.drawString(line, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2, g_seqStepEditPatMinus.y + 10);
  snprintf(line, sizeof(line), "Rate: %s", kStepDivLabs[rate]);
  M5.Display.drawString(line, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2, g_seqStepEditRateMinus.y + 10);
  snprintf(line, sizeof(line), "Octave range: %u", (unsigned)oct);
  M5.Display.drawString(line, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2, g_seqStepEditOctMinus.y + 10);
  snprintf(line, sizeof(line), "Gate: %u%%", (unsigned)gate);
  M5.Display.drawString(line, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2, g_seqStepEditGateMinus.y + 10);
}

/// Multi-ring glow for the audible sequencer step while transport is playing (theme-tuned colors).
static void drawSequencerActiveStepGlow(const Rect& r) {
  const int rad = max(4, r.h / 8);
  M5.Display.drawRoundRect(r.x - 7, r.y - 7, r.w + 14, r.h + 14, rad + 7, g_uiPalette.seqPlayGlowOuter);
  M5.Display.drawRoundRect(r.x - 4, r.y - 4, r.w + 8, r.h + 8, rad + 4, g_uiPalette.highlightRing);
  M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, rad + 2, g_uiPalette.seqPlayGlowInner);
}

static void drawSequencerPaintCell(int i, int fingerCell) {
  const Rect& r = g_seqCellRects[i];
  static const char* kToolLabs[4] = {"Qnt", "Swg", "Prb", "Rnd"};
  static const SeqTool kToolMap[4] = {SeqTool::Quantize, SeqTool::Swing, SeqTool::StepProb,
                                      SeqTool::ChordRand};

  if (seqSelectToolsActive() && i < 4) {
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
    return;
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
  if (transportIsPlaying() && transportAudibleStep() == static_cast<uint8_t>(i)) {
    drawSequencerActiveStepGlow(r);
  }
  if (g_seqTool == SeqTool::StepProb && g_seqProbFocusStep == i) {
    M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                             g_uiPalette.highlightRing);
  }
  if (s_shiftActive && s_shiftSeqFocusStep == i) {
    M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                             g_uiPalette.settingsBtnActive);
  }
}

/// Repaint only cells affected by playhead motion (avoids full-screen flash on each tick).
static void sequencerRedrawPlayheadDelta(uint8_t oldAudible, uint8_t newAudible) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  computeSequencerLayout(w, h);
  M5.Display.startWrite();
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  if (oldAudible < 16) {
    drawSequencerPaintCell(static_cast<int>(oldAudible), -1);
  }
  if (newAudible < 16) {
    drawSequencerPaintCell(static_cast<int>(newAudible), -1);
  }
  M5.Display.endWrite();
}

void drawSequencerSurface(int fingerCell, bool fullClear) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  if (fullClear) M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  computeSequencerLayout(w, h);

  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);

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

  for (int i = 0; i < 16; ++i) {
    drawSequencerPaintCell(i, fingerCell);
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
    int dx, dy, dropW, rowH, vis;
    seqChordDropComputeLayout(w, h, &dx, &dy, &dropW, &rowH, &vis);
    if (s_seqChordDropScroll < 0) s_seqChordDropScroll = 0;
    if (s_seqChordDropScroll + vis > items) s_seqChordDropScroll = max(0, items - vis);
    const int totalH = rowH * vis;
    M5.Display.fillRect(dx - 2, dy - 2, dropW + 4, totalH + 4, g_uiPalette.bg);
    for (int r = 0; r < vis; ++r) {
      const int itemIdx = s_seqChordDropScroll + r;
      const Rect dr = {dx, dy + r * rowH, dropW, rowH};
      const char* lab;
      if (itemIdx < 8) {
        lab = g_model.surround[itemIdx].name;
      } else if (itemIdx == 8) {
        lab = "~ tie";
      } else if (itemIdx == 9) {
        lab = "- rest";
      } else {
        lab = "S? surprise";
      }
      uint16_t bg;
      if (itemIdx < 8) {
        bg = colorForRole(g_model.surround[itemIdx].role);
      } else if (itemIdx == 8) {
        bg = g_uiPalette.seqTie;
      } else if (itemIdx == 9) {
        bg = g_uiPalette.seqRest;
      } else {
        bg = g_uiPalette.surprise;
      }
      drawRoundedButton(dr, bg, lab, 1);
    }
    if (items > vis) {
      const int railX = dx + dropW - 3;
      M5.Display.drawFastVLine(railX, dy, totalH, g_uiPalette.subtle);
      const int thumbH = max(14, (totalH * vis) / items);
      const int thumbY = dy + ((totalH - thumbH) * s_seqChordDropScroll) / max(1, items - vis);
      M5.Display.fillRect(railX - 1, thumbY, 3, thumbH, g_uiPalette.rowNormal);
    }
  }

  if (s_seqStepEditStep >= 0 && s_seqStepEditStep < 16) {
    drawSeqStepEditPopup();
  }

  // No top-right battery / link glyphs here — sequencer grid needs the horizontal space.
  drawBezelBarStrip();
  M5.Display.endWrite();
  paintAppTopDrawer();
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
    case MidiEventType::SysEx:
      return "SX";
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

void drawMidiDebugSurface(bool fullClear) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  if (fullClear) M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString("MIDI Debug", w / 2, 2);
  M5.Display.setTextSize(1);

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
  drawTopSystemStatus(w, 2, nullptr, 0);
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
    drawMidiDebugSurface(false);
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
    drawMidiDebugSurface(false);
    return;
  }
  if (pointInRect(hx, hy, g_midiDbgResetChip)) {
    s_midiDbgSourceFilter = -1;
    s_midiDbgTypeFilter = -1;
    drawMidiDebugSurface(false);
    return;
  }
  drawMidiDebugSurface(false);
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
    midiSilenceLastChord(ch);
  }
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
  ::ui::topDrawerClose(g_topDrawerUi);
  static const Screen kRing[] = {Screen::Play, Screen::Sequencer, Screen::XyPad};
  constexpr int kRingLen = 3;
  beforeLeaveSequencer();
  if (g_screen == Screen::Play) {
    s_playVoicingPanelOpen = false;
  }
  if (g_screen == Screen::Transport) {
    transportDropDismiss();
  }
  int idx = 0;
  bool found = false;
  for (int i = 0; i < kRingLen; ++i) {
    if (kRing[i] == g_screen) {
      idx = i;
      found = true;
      break;
    }
  }
  if (!found) {
    idx = 0;
  }
  idx = (idx + direction + 300) % kRingLen;
  g_screen = kRing[idx];
  s_xyMidiSentX = 255;
  s_xyMidiSentY = 255;
  s_xyLastSendMsX = 0;
  s_xyLastSendMsY = 0;
  s_xyPendingX.dirty = false;
  s_xyPendingY.dirty = false;
  switch (g_screen) {
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

static void openTransportDrawer() {
  if (g_screen == Screen::Transport || g_screen == Screen::TransportBpmEdit ||
      g_screen == Screen::TransportTapTempo)
    return;
  ::ui::topDrawerClose(g_topDrawerUi);
  panicForTrigger(PanicTrigger::RingNavigation);
  beforeLeaveSequencer();
  if (g_screen == Screen::Play) s_playVoicingPanelOpen = false;
  s_screenBeforeTransport = g_screen;
  g_screen = Screen::Transport;
  drawTransportSurface();
}

static void closeTransportDrawer() {
  transportDropDismiss();
  g_screen = s_screenBeforeTransport;
  switch (g_screen) {
    case Screen::Play:          drawPlaySurface(); break;
    case Screen::Sequencer:     drawSequencerSurface(); break;
    case Screen::XyPad:         drawXyPadSurface(); break;
    case Screen::PlayCategory:  drawPlayCategorySurface(); break;
    case Screen::MidiDebug:     drawMidiDebugSurface(); break;
    default:                    drawPlaySurface(); g_screen = Screen::Play; break;
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
  const int items = seqChordDropItemCount();
  int dx, dy, dropW, rowH, vis;
  seqChordDropComputeLayout(M5.Display.width(), M5.Display.height(), &dx, &dy, &dropW, &rowH, &vis);
  if (s_seqChordDropScroll < 0) s_seqChordDropScroll = 0;
  if (s_seqChordDropScroll + vis > items) s_seqChordDropScroll = max(0, items - vis);
  for (int r = 0; r < vis; ++r) {
    if (px >= dx && px < dx + dropW && py >= dy + r * rowH && py < dy + (r + 1) * rowH) {
      return s_seqChordDropScroll + r;
    }
  }
  return -1;
}

void processSequencerTouch(uint8_t touchCount, int w, int h) {
  computeSequencerLayout(w, h);

  if (::ui::topDrawerIsOpen(g_topDrawerUi) && g_topDrawerUi.ignoreUntilTouchUp) {
    if (touchCount > 0) {
      wasTouchActive = true;
      return;
    }
    g_topDrawerUi.ignoreUntilTouchUp = false;
    wasTouchActive = false;
    return;
  }

  if (sequencerProcessTopDrawerTouch(touchCount, w, h)) return;

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
    if (s_seqChordDropStep >= 0 && d.isPressed() && !wasTouchActive &&
        hitTestSeqChordDrop(d.x, d.y) >= 0) {
      s_seqChordDropTouchStartX = d.x;
      s_seqChordDropTouchStartY = d.y;
    }
    if (s_seqChordDropStep >= 0 && d.isPressed()) {
      if (s_seqChordDropDragLastY >= 0) {
        const int delta = d.y - s_seqChordDropDragLastY;
        if (delta > 18) {
          if (s_seqChordDropScroll > 0) {
            --s_seqChordDropScroll;
            ++s_seqChordDropFingerScrollCount;
            drawSequencerSurface(-1, false);
          }
          s_seqChordDropDragLastY = d.y;
        } else if (delta < -18) {
          const int vis = seqChordDropVisibleRows(h);
          const int cnt = seqChordDropItemCount();
          if (s_seqChordDropScroll + vis < cnt) {
            ++s_seqChordDropScroll;
            ++s_seqChordDropFingerScrollCount;
            drawSequencerSurface(-1, false);
          }
          s_seqChordDropDragLastY = d.y;
        }
      } else {
        s_seqChordDropDragLastY = d.y;
      }
    } else {
      s_seqChordDropDragLastY = -1;
    }
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
        drawSequencerSurface(-1, false);
      }
      wasTouchActive = true;
      return;
    }

    const uint32_t now = millis();
    if (s_seqChordDropStep < 0) {
      const int ht = hitTestSeq(d.x, d.y);
      if (ht >= 0 && !(seqSelectToolsActive() && ht < 4)) {
        if (s_seqStepDownIdx != ht) {
          s_seqStepDownIdx = ht;
          s_seqStepDownMs = now;
          s_seqLongPressHandled = false;
        }
        if (!s_seqLongPressHandled && now - s_seqStepDownMs >= seqLongPressMs()) {
          s_seqStepEditStep = ht;
          s_seqStepEditJustOpened = true;
          s_seqLongPressHandled = true;
          drawSequencerSurface(-1, false);
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
      drawSequencerSurface(-1, false);
      return;
    }

    if (g_seqDraggingSlider) {
      g_seqDraggingSlider = false;
      seqExtrasSave(&g_seqExtras);
      drawSequencerSurface(-1, false);
      return;
    }

    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;

    if (s_seqStepEditStep >= 0) {
      seqStepEditComputeLayout(w, h);
      if (s_seqStepEditJustOpened) {
        // Ignore the release that ends the long-press gesture used to open the popup.
        s_seqStepEditJustOpened = false;
        drawSequencerSurface(-1, false);
        return;
      }
      if (pointInRect(hx, hy, g_seqStepEditDone) || pointInRect(hx, hy, g_bezelSelect) ||
          pointInRect(hx, hy, g_bezelBack) || pointInRect(hx, hy, g_bezelFwd)) {
        s_seqStepEditStep = -1;
        s_seqStepEditJustOpened = false;
        seqExtrasSave(&g_seqExtras);
        drawSequencerSurface(-1, false);
        return;
      }
      const uint8_t lane = seqLaneClamped();
      const int step = s_seqStepEditStep;
      bool changed = false;
      if (pointInRect(hx, hy, g_seqStepEditDelete)) {
        g_seqPattern[lane][step] = kSeqRest;
        s_seqStepEditStep = -1;
        s_seqStepEditJustOpened = false;
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditVoMinus)) {
        uint8_t v = g_seqExtras.stepVoicing[lane][step];
        if (v > 1) {
          --v;
          g_seqExtras.stepVoicing[lane][step] = v;
          changed = true;
        }
      } else if (pointInRect(hx, hy, g_seqStepEditVoPlus)) {
        uint8_t v = g_seqExtras.stepVoicing[lane][step];
        if (v < 4) {
          ++v;
          g_seqExtras.stepVoicing[lane][step] = v;
          changed = true;
        }
      } else if (pointInRect(hx, hy, g_seqStepEditArpToggle)) {
        g_seqExtras.arpEnabled[lane][step] = g_seqExtras.arpEnabled[lane][step] ? 0U : 1U;
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditPatMinus)) {
        int v = static_cast<int>(g_seqExtras.arpPattern[lane][step] & 0x03U) - 1;
        if (v < 0) v = 3;
        g_seqExtras.arpPattern[lane][step] = static_cast<uint8_t>(v);
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditPatPlus)) {
        int v = static_cast<int>(g_seqExtras.arpPattern[lane][step] & 0x03U) + 1;
        if (v > 3) v = 0;
        g_seqExtras.arpPattern[lane][step] = static_cast<uint8_t>(v);
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditRateMinus)) {
        int v = static_cast<int>(g_seqExtras.arpClockDiv[lane][step] & 0x03U) - 1;
        if (v < 0) v = 3;
        g_seqExtras.arpClockDiv[lane][step] = static_cast<uint8_t>(v);
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditRatePlus)) {
        int v = static_cast<int>(g_seqExtras.arpClockDiv[lane][step] & 0x03U) + 1;
        if (v > 3) v = 0;
        g_seqExtras.arpClockDiv[lane][step] = static_cast<uint8_t>(v);
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditOctMinus)) {
        uint8_t v = g_seqExtras.arpOctRange[lane][step];
        if (v > 0) {
          --v;
          g_seqExtras.arpOctRange[lane][step] = v;
          changed = true;
        }
      } else if (pointInRect(hx, hy, g_seqStepEditOctPlus)) {
        uint8_t v = g_seqExtras.arpOctRange[lane][step];
        if (v < 2) {
          ++v;
          g_seqExtras.arpOctRange[lane][step] = v;
          changed = true;
        }
      } else if (pointInRect(hx, hy, g_seqStepEditGateMinus)) {
        int v = static_cast<int>(g_seqExtras.arpGatePct[lane][step]) - 5;
        if (v < 10) v = 10;
        if (v != g_seqExtras.arpGatePct[lane][step]) {
          g_seqExtras.arpGatePct[lane][step] = static_cast<uint8_t>(v);
          changed = true;
        }
      } else if (pointInRect(hx, hy, g_seqStepEditGatePlus)) {
        int v = static_cast<int>(g_seqExtras.arpGatePct[lane][step]) + 5;
        if (v > 100) v = 100;
        if (v != g_seqExtras.arpGatePct[lane][step]) {
          g_seqExtras.arpGatePct[lane][step] = static_cast<uint8_t>(v);
          changed = true;
        }
      }
      if (changed) {
        seqExtrasSave(&g_seqExtras);
      }
      drawSequencerSurface(-1, false);
      return;
    }

    if (s_seqGestureMaxTouches >= 2 && s_seqComboTab >= 0) {
      uint8_t ch = g_seqMidiCh[s_seqComboTab];
      ch = static_cast<uint8_t>((ch % 16) + 1);
      g_seqMidiCh[s_seqComboTab] = ch;
      seqLaneChannelsSave(g_seqMidiCh);
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      drawSequencerSurface(-1, false);
      return;
    }

    if (pointInRect(hx, hy, g_bezelBack)) {
      if (s_shiftActive && s_shiftSeqFocusStep >= 0) {
        shiftSeqAdjustFocusedParam(-1);
        drawSequencerSurface(-1, false);
        return;
      }
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      s_seqChordDropStep = -1;
      s_seqChordDropDragLastY = -1;
      s_seqChordDropFingerScrollCount = 0;
      s_seqSelectHeld = false;
      navigateMainRing(-1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      if (s_shiftActive && s_shiftSeqFocusStep >= 0) {
        shiftSeqAdjustFocusedParam(1);
        drawSequencerSurface(-1, false);
        return;
      }
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      s_seqChordDropStep = -1;
      s_seqChordDropDragLastY = -1;
      s_seqChordDropFingerScrollCount = 0;
      s_seqSelectHeld = false;
      navigateMainRing(1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      if (s_shiftTriggeredThisHold) {
        s_shiftTriggeredThisHold = false;
        drawSequencerSurface(-1, false);
        return;
      }
      const uint8_t maxT = s_seqGestureMaxTouches;
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      const bool hadChordDrop = (s_seqChordDropStep >= 0);
      s_seqChordDropStep = -1;
      s_seqChordDropDragLastY = -1;
      s_seqChordDropFingerScrollCount = 0;
      if (maxT <= 1) s_seqSelectHeld = !s_seqSelectHeld;
      drawSequencerSurface(-1, hadChordDrop);
      return;
    }

    s_seqGestureMaxTouches = 0;
    s_seqComboTab = -1;

    if (s_seqChordDropStep >= 0) {
      const int fingerScrollsThisGesture = s_seqChordDropFingerScrollCount;
      s_seqChordDropFingerScrollCount = 0;
      s_seqChordDropDragLastY = -1;
      const int pick = hitTestSeqChordDrop(hx, hy);
      if (pick >= 0) {
        const bool haveAnchor =
            hitTestSeqChordDrop(s_seqChordDropTouchStartX, s_seqChordDropTouchStartY) >= 0;
        const bool movedPast = haveAnchor && touchMovedPastSuppressThreshold(
                                                s_seqChordDropTouchStartX, s_seqChordDropTouchStartY, hx, hy);
        if (!movedPast && fingerScrollsThisGesture == 0) {
          uint8_t v;
          if (pick < 8) v = static_cast<uint8_t>(pick);
          else if (pick == 8) v = kSeqTie;
          else if (pick == 9) v = kSeqRest;
          else v = kSeqSurprise;
          g_seqPattern[g_seqLane][s_seqChordDropStep] = v;
        }
      }
      s_seqChordDropStep = -1;
      // fullClear: dropdown extends past step cells; partial redraw leaves the list painted
      // "behind" the grid until the next full paint.
      drawSequencerSurface(-1, true);
      return;
    }

    if (seqSelectToolsActive()) {
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
        drawSequencerSurface(-1, false);
        return;
      }
    }

    const int tabHit = hitTestSeqTab(hx, hy);
    if (tabHit >= 0) {
      g_seqLane = static_cast<uint8_t>(tabHit);
      shiftSeqFocusSyncFromCurrentLane();
      drawSequencerSurface(-1, false);
      return;
    }

    if (s_seqLongPressHandled) {
      s_seqStepDownIdx = -1;
      s_seqLongPressHandled = false;
      drawSequencerSurface(-1, false);
      return;
    }

    const int cell = hitTestSeq(hx, hy);
    if (cell >= 0) {
      if (s_shiftActive) {
        shiftSeqFocusSetForCurrentLane(static_cast<int8_t>(cell));
        drawSequencerSurface(-1, false);
        return;
      }
      if (g_seqTool == SeqTool::StepProb) {
        g_seqProbFocusStep = static_cast<int8_t>(cell);
        seqExtrasSave(&g_seqExtras);
        drawSequencerSurface(-1, false);
        return;
      }
      s_seqChordDropStep = cell;
      s_seqChordDropDragLastY = -1;
      s_seqChordDropFingerScrollCount = 0;
      const int items = seqChordDropItemCount();
      const int vis = seqChordDropVisibleRows(h);
      uint8_t cur = g_seqPattern[g_seqLane][cell];
      int curIdx = 0;
      if (cur <= 7) curIdx = static_cast<int>(cur);
      else if (cur == kSeqTie) curIdx = 8;
      else if (cur == kSeqRest) curIdx = 9;
      else curIdx = 10;
      s_seqChordDropScroll = curIdx - vis / 2;
      if (s_seqChordDropScroll < 0) s_seqChordDropScroll = 0;
      if (s_seqChordDropScroll + vis > items) s_seqChordDropScroll = max(0, items - vis);
      s_seqChordDropTouchStartX = -9999;
      s_seqChordDropTouchStartY = -9999;
      drawSequencerSurface(-1, false);
      return;
    }
    s_seqStepDownIdx = -1;
    drawSequencerSurface(-1, false);
  }
}

void processPlayTouch(uint8_t touchCount, int w, int h) {
  computePlaySurfaceLayout(w, h);

  if (::ui::topDrawerIsOpen(g_topDrawerUi) && g_topDrawerUi.ignoreUntilTouchUp) {
    if (touchCount > 0) {
      wasTouchActive = true;
      return;
    }
    g_topDrawerUi.ignoreUntilTouchUp = false;
    wasTouchActive = false;
    return;
  }

  if (playProcessTopDrawerTouch(touchCount, w, h)) return;

  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    if (!wasTouchActive) {
      s_playTouchStartMs = millis();
      s_playChordTriggeredThisTouch = false;
      s_playTouchLockedHit = -9999;
      for (uint8_t i = 0; i < touchCount; ++i) {
        const auto& d = M5.Touch.getDetail(i);
        if (!d.isPressed()) continue;
        if (pointInRect(d.x, d.y, g_bezelBack) || pointInRect(d.x, d.y, g_bezelSelect) ||
            pointInRect(d.x, d.y, g_bezelFwd)) {
          s_playTouchLockedHit = -100;
        } else {
          s_playTouchLockedHit = hitTestPlay(d.x, d.y);
        }
        break;
      }
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
      const int htLocked = (s_playTouchLockedHit != -9999) ? s_playTouchLockedHit : ht;
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
    const int htLocked = (s_playTouchLockedHit != -9999) ? s_playTouchLockedHit : ht;
    int dc = -100;
    bool dok = false;
    if (pointInRect(d.x, d.y, g_bezelBack) || pointInRect(d.x, d.y, g_bezelSelect) ||
        pointInRect(d.x, d.y, g_bezelFwd)) {
      dc = -100;
      dok = false;
    } else if (htLocked == -2) {
      dc = -100;
      dok = true;
    } else if (htLocked >= 0) {
      dc = htLocked;
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
      if (!onBezel && htLocked != -2 && htLocked < 0) {
        playRedrawInPlace();
      } else {
        playRedrawFingerHighlightOnly(dc, dok);
      }
    }
    if (d.isPressed() && !s_playChordTriggeredThisTouch && !s_playSelectLatched && !s_shiftActive) {
      if (htLocked == -2) {
        const uint8_t mch = static_cast<uint8_t>(g_settings.midiOutChannel - 1);
        const uint8_t vel = applyOutputVelocityCurve(g_settings.outputVelocity);
        uint8_t vcap = g_chordVoicing;
        if (vcap < 1) vcap = 1;
        if (vcap > 4) vcap = 4;
        if (g_model.heartAvailable) {
          const int poolIdx = g_model.surprisePeekIndex();
          const ChordInfo& si = g_model.surprisePool[poolIdx % ChordModel::kSurprisePoolSize];
          strncpy(s_surpriseChordLabel, si.name, sizeof(s_surpriseChordLabel) - 1);
          s_surpriseChordLabel[sizeof(s_surpriseChordLabel) - 1] = '\0';
          s_surpriseChordActive = true;
          g_model.nextSurprise();
          g_model.consumeHeart();
          midiPlaySurprisePad(g_model, poolIdx, mch, vel, vcap, g_settings.arpeggiatorMode,
                              g_settings.transposeSemitones);
        } else {
          midiPlayTonicChord(g_model, mch, vel, vcap, g_settings.arpeggiatorMode, g_settings.transposeSemitones);
        }
        const int prevOutline = g_lastPlayedOutline;
        g_lastPlayedOutline = -2;
        transportSetLiveChord(-1);
        s_playChordTriggeredThisTouch = true;
        s_playChordActiveCh0 = static_cast<uint8_t>(mch & 0x0F);
        playRedrawAfterOutlineChange(prevOutline, -100, true);
        wasTouchActive = true;
        return;
      }
      if (htLocked >= 0 && htLocked < ChordModel::kSurroundCount) {
        const uint8_t mch = static_cast<uint8_t>((g_settings.midiOutChannel - 1U) & 0x0F);
        const int prevOutline = g_lastPlayedOutline;
        const bool heartTransitioned = playTriggerSurroundByIdx(htLocked, false);
        s_playChordTriggeredThisTouch = true;
        s_playChordActiveCh0 = mch;
        if (heartTransitioned) {
          playRedrawInPlace();
        } else {
          playRedrawAfterOutlineChange(prevOutline, htLocked, false);
        }
        wasTouchActive = true;
        return;
      }
    }
    wasTouchActive = true;
    return;
  }

  if (touchCount == 0) {
    s_playTouchLockedHit = -9999;
    if (!wasTouchActive) return;
    if (millis() - s_playTouchStartMs < kTapDebounceMs) {
      s_playTouchDrawChord = -100;
      s_playTouchDrawOnKey = false;
      s_playTouchDrawVoicing = false;
      wasTouchActive = false;
      s_playGestureMaxTouches = 0;
      s_playVoicingCombo = false;
      s_playKeyHoldArmed = false;
      s_playChordTriggeredThisTouch = false;
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
      playRedrawInPlace();
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
      playRedrawInPlace();
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
        playRedrawInPlace();
        return;
      }
      if (gestureMax <= 1) {
        s_playSelectLatched = !s_playSelectLatched;
        if (!s_playSelectLatched) {
          s_playVoicingPanelOpen = false;
        }
      }
      playRedrawInPlace();
      return;
    }

    const int hit = hitTestPlay(hx, hy);
    if (hit == -2) {
      if (s_playChordTriggeredThisTouch) {
        midiSilenceLastChord(s_playChordActiveCh0 & 0x0F);
        s_playChordTriggeredThisTouch = false;
        if (hadHighlight) {
          playRedrawClearFingerHighlight();
        }
        return;
      }
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
          const ChordInfo& si = g_model.surprisePool[poolIdx % ChordModel::kSurprisePoolSize];
          strncpy(s_surpriseChordLabel, si.name, sizeof(s_surpriseChordLabel) - 1);
          s_surpriseChordLabel[sizeof(s_surpriseChordLabel) - 1] = '\0';
          s_surpriseChordActive = true;
          g_model.nextSurprise();
          g_model.consumeHeart();
          midiPlaySurprisePad(g_model, poolIdx, mch, vel, vcap, g_settings.arpeggiatorMode,
                              g_settings.transposeSemitones);
          schedulePlayAutoSilence(mch);
        } else {
          midiPlayTonicChord(g_model, mch, vel, vcap, g_settings.arpeggiatorMode, g_settings.transposeSemitones);
          schedulePlayAutoSilence(mch);
        }
      }
      const int prevOutline = g_lastPlayedOutline;
      g_lastPlayedOutline = -2;
      transportSetLiveChord(-1);
      playRedrawAfterOutlineChange(prevOutline);
      return;
    }
    if (hit >= 0 && hit < ChordModel::kSurroundCount) {
      if (s_playChordTriggeredThisTouch) {
        midiSilenceLastChord(s_playChordActiveCh0 & 0x0F);
        s_playChordTriggeredThisTouch = false;
        if (hadHighlight) {
          playRedrawClearFingerHighlight();
        }
        return;
      }
      if (s_shiftActive) {
        const uint8_t ch0 = static_cast<uint8_t>((g_settings.midiOutChannel - 1U) & 0x0F);
        if (hit < 4) {
          midiSendControlChange(ch0, kShiftPadCcMap[hit], 127);
        } else {
          midiSendProgramChange(ch0, kShiftPadProgramMap[hit - 4]);
        }
        playRedrawInPlace();
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
          playRedrawInPlace();
          return;
        }
        if (hit == 2) {
          int v = static_cast<int>(g_settings.transposeSemitones) + 1;
          if (v > 24) v = 24;
          g_settings.transposeSemitones = static_cast<int8_t>(v);
          settingsSave(g_settings);
          playRedrawInPlace();
          return;
        }
        if (hit == 6) {
          int v = static_cast<int>(g_settings.transposeSemitones) - 1;
          if (v < -24) v = -24;
          g_settings.transposeSemitones = static_cast<int8_t>(v);
          settingsSave(g_settings);
          playRedrawInPlace();
          return;
        }
      }
      const int prevOutline = g_lastPlayedOutline;
      const bool heartTransitioned = playTriggerSurroundByIdx(hit);
      if (heartTransitioned) {
        playRedrawInPlace();
      } else {
        playRedrawAfterOutlineChange(prevOutline);
      }
      return;
    }
    s_playChordTriggeredThisTouch = false;
    if (hadHighlight) {
      playRedrawClearFingerHighlight();
    }
  }
}

void processPlayCategoryTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);
  if (touchCount > 0) {
    if (!wasTouchActive) {
      s_playCategoryFingerPainted = -9999;
    }
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
    if (cell != s_playCategoryFingerPainted) {
      s_playCategoryFingerPainted = cell;
      playCategoryRedrawInPlace(cell);
    }
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
      playCategoryRedrawInPlace();
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      playCategoryStep(1);
      playCategoryRedrawInPlace();
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
      playCategoryRedrawInPlace();
      return;
    }
    playCategoryRedrawInPlace();
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
  drawTopSystemStatus(w, 2, nullptr, 0);
  M5.Display.endWrite();
}

void processKeyPickerTouch(uint8_t touchCount, int w, int h) {
  (void)w;
  (void)h;
  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
    if (!wasTouchActive) {
      s_keyPickTouchStartX = g_lastTouchX;
      s_keyPickTouchStartY = g_lastTouchY;
    }
    wasTouchActive = true;
    return;
  }

  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int px = g_lastTouchX;
  const int py = g_lastTouchY;
  const bool movedPast = touchMovedPastSuppressThreshold(s_keyPickTouchStartX, s_keyPickTouchStartY, px, py);

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
  if (!movedPast) {
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
  }
  drawKeyPicker();
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
  drawTopSystemStatus(w, 2, nullptr, 0);
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
  drawTopSystemStatus(w, 2, nullptr, 0);
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
    if (!wasTouchActive) {
      s_sdPickTouchStartX = g_lastTouchX;
      s_sdPickTouchStartY = g_lastTouchY;
    }
    wasTouchActive = true;
    return;
  }
  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;
  const bool movedPast = touchMovedPastSuppressThreshold(s_sdPickTouchStartX, s_sdPickTouchStartY, hx, hy);
  if (pointInRect(hx, hy, g_sdPickCancel)) {
    panicForTrigger(PanicTrigger::RestoreTransition);
    g_screen = Screen::Settings;
    drawSettingsUi();
    return;
  }
  if (!movedPast) {
    for (int i = 0; i < g_sdPickCount && i < 8; ++i) {
      if (pointInRect(hx, hy, g_sdPickRows[i])) {
        panicForTrigger(PanicTrigger::RestoreTransition);
        applySdRestoreFromFolder(g_sdPickNames[i]);
        g_screen = Screen::Settings;
        drawSettingsUi();
        return;
      }
    }
  }
  drawSdProjectPick();
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
    case MidiEventType::SysEx:
      return "SX";
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

static void midiMaybeReplyUniversalDeviceInquiry(const MidiEvent& ev) {
  if (ev.type != MidiEventType::SysEx) return;
  const uint8_t* p = midiIngressLastSysexPayload();
  const uint16_t n = ev.value14;
  if (n != 6) return;
  if (p[0] != 0xF0 || p[1] != 0x7E || p[3] != 0x06 || p[4] != 0x01 || p[5] != 0xF7) return;
  const uint8_t devicePath = p[2];
  // Standard Identity Reply: F0 7E <dev> 06 02 <mfr> <fam_lo> <fam_hi> <mdl_lo> <mdl_hi> <v1> <v2> <v3> <v4> F7
  uint8_t reply[15];
  reply[0]  = 0xF0;
  reply[1]  = 0x7E;
  reply[2]  = devicePath;
  reply[3]  = 0x06;
  reply[4]  = 0x02;
  reply[5]  = 0x7D;  // educational / non-commercial manufacturer ID
  reply[6]  = 0x01;  // family LSB
  reply[7]  = 0x00;  // family MSB
  reply[8]  = 0x01;  // model LSB
  reply[9]  = 0x00;  // model MSB
  reply[10] = 0x01;  // version byte 1
  reply[11] = 0x00;  // version byte 2
  reply[12] = 0x00;  // version byte 3
  reply[13] = 0x00;  // version byte 4
  reply[14] = 0xF7;
  midiSendSysEx(reply, sizeof(reply));
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
    case MidiEventType::SysEx: {
      const uint8_t* sx = midiIngressLastSysexPayload();
      const unsigned n = static_cast<unsigned>(ev.value14);
      if (n >= 3U) {
        snprintf(out, outSize, "IN:%s -- SX len=%u %02X%02X%02X… %s", src, n,
                 static_cast<unsigned>(sx[0]), static_cast<unsigned>(sx[1]),
                 static_cast<unsigned>(sx[2]), ageBuf);
      } else if (n >= 1U) {
        snprintf(out, outSize, "IN:%s -- SX len=%u %02X… %s", src, n,
                 static_cast<unsigned>(sx[0]), ageBuf);
      } else {
        snprintf(out, outSize, "IN:%s -- SX len=%u %s", src, n, ageBuf);
      }
      return;
    }
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
  // Avoid monitor spam: never drive a full play-surface redraw from MIDI Clock (24 ticks/qn).
  // Also only mark dirty when the formatted line actually changes (dense CC/notes).
  const bool allowClockUpdate = !isClock || (nowMs - s_playIngressLastMusicalMs >= playMonitorClockSuppressMs());
  if (allowClockUpdate && !isClock) {
    char line[sizeof(s_playIngressInfo)];
    playMonitorFormatLine(ev, line, sizeof(line), nowMs);
    if (strcmp(line, s_playIngressInfo) != 0) {
      snprintf(s_playIngressInfo, sizeof(s_playIngressInfo), "%s", line);
      s_playIngressInfoEventMs = nowMs;
      s_playIngressInfoUntilMs = nowMs + 2200;
      s_playIngressInfoDirty = true;
    }
  }
  if (!midiEventIsRealtime(ev)) {
    if (!midiEventPassesChannelFilter(ev)) return;
    if (ev.type == MidiEventType::SysEx) {
      midiMaybeReplyUniversalDeviceInquiry(ev);
      uint8_t mmcCmd = 0;
      if (midiMmcParseRealtime(midiIngressLastSysexPayload(), ev.value14, &mmcCmd)) {
        const uint8_t sourceRoute = midiSourceToRoute(ev.source);
        const bool followClock = g_settings.clkFollow != 0;
        const bool receiveFromThisSource = g_settings.midiTransportReceive == sourceRoute;
        if (followClock && receiveFromThisSource) {
          switch (mmcCmd) {
            case 0x01:  // MMC Stop
              panicForTrigger(PanicTrigger::ExternalTransportStop);
              transportOnExternalStop();
              break;
            case 0x02:  // MMC Play
              transportOnExternalStart(nowMs);
              break;
            case 0x03:  // MMC Deferred Play (resume from current position)
              transportOnExternalContinue(nowMs);
              break;
            case 0x09:  // MMC Pause — treat like transport stop (notes off + idle)
              panicForTrigger(PanicTrigger::ExternalTransportStop);
              transportOnExternalStop();
              break;
            default:
              break;
          }
        }
      }
    }
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
        case MidiEventType::SysEx:
          midiSendSysEx(midiIngressLastSysexPayload(), ev.value14);
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
      // Arm from clock alone when the host never sends 0xFA/0xFB (see Transport). After MIDI Stop
      // or local Stop, do not re-arm from 0xF8 alone — hosts often keep sending clock while stopped.
      if (followClock && receiveFromThisSource) {
        if (!transportExternalClockActive()) {
          if (transportExternalMayArmFromClockStream()) {
            transportOnExternalContinue(nowMs);
          }
        }
        if (transportExternalClockActive()) {
          transportOnExternalClockTick(nowMs);
        }
      }
      break;
    default:
      break;
  }
}

static void midiRouteWriteBytes(uint8_t route, const uint8_t* bytes, size_t len) {
  if (!bytes || len == 0) return;
  // route: 1=USB, 2=BLE, 3=DIN (matches Transport dropdown; 0 = off).
  switch (route) {
    case 1:
      (void)usbMidiWrite(bytes, len);
      break;
    case 2:
      (void)bleMidiWrite(bytes, len);
      break;
    case 3:
      (void)dinMidiWrite(bytes, len);
      break;
    default:
      break;
  }
}

static void midiRouteWriteRealtime(uint8_t route, uint8_t status) {
  const uint8_t b = status;
  midiRouteWriteBytes(route, &b, 1);
}

static void transportSendTickIfNeeded(uint32_t nowMs) {
  const uint8_t route = g_settings.midiTransportSend;
  if (route == 0 || route > 3) return;
  // While following external clock, do not echo generated clock back out.
  if (transportExternalClockActive()) return;

  const bool playing = transportIsPlaying();
  const bool running = transportIsRunning();
  if (running && !s_transportTxPrevRunning) {
    s_transportTxNextClockMs = nowMs;
  }
  if (playing && !s_transportTxPrevPlaying) {
    seqArpStopAll(true);
    // Start from step-zero, Continue otherwise.
    const uint8_t st = (transportPlayhead() == 0) ? 0xFAU : 0xFBU;
    midiRouteWriteRealtime(route, st);
    s_transportTxNextClockMs = nowMs;
  } else if (!playing && s_transportTxPrevPlaying) {
    midiRouteWriteRealtime(route, 0xFCU);
  }
  s_transportTxPrevPlaying = playing;
  s_transportTxPrevRunning = running;

  if (!playing) return;
  uint16_t bpm = g_projectBpm;
  if (bpm < 40U) bpm = 40U;
  if (bpm > 300U) bpm = 300U;
  const uint32_t tickMs = 60000UL / (static_cast<uint32_t>(bpm) * 24UL);
  if (s_transportTxNextClockMs == 0) s_transportTxNextClockMs = nowMs;
  while (static_cast<int32_t>(nowMs - s_transportTxNextClockMs) >= 0) {
    midiRouteWriteRealtime(route, 0xF8U);
    s_transportTxNextClockMs += (tickMs == 0 ? 1U : tickMs);
  }
}

void paintAppTopDrawer() {
  if (!::ui::topDrawerIsOpen(g_topDrawerUi)) return;
  ::ui::DisplayAdapter d = ::ui::displayAdapterForM5();
  switch (g_topDrawerUi.kind) {
    case ::ui::TopDrawerKind::Xy:
      drawXyTopDrawer(d);
      break;
    case ::ui::TopDrawerKind::Play:
      drawPlayTopDrawer(d);
      break;
    case ::ui::TopDrawerKind::Sequencer:
      drawSequencerTopDrawer(d);
      break;
    default:
      break;
  }
}

#include "screens/XyScreen.inc"
#include "screens/PlayScreen.inc"
#include "screens/SequencerScreen.inc"
#include "screens/TransportScreen.inc"
#include "screens/SettingsScreen.inc"

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
  usbMidiInit();
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
  usbMidiPoll();
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
  // Hosts often stop sending 0xF8 when transport stops but omit 0xFC to USB device ports — infer stop.
  if (transportExternalClockStalled(now)) {
    panicForTrigger(PanicTrigger::ExternalTransportStop);
    transportOnExternalStop();
  }
  if (s_playAutoSilenceAtMs != 0 && static_cast<int32_t>(now - s_playAutoSilenceAtMs) >= 0) {
    midiSilenceLastChord(s_playAutoSilenceCh0);
    s_playAutoSilenceAtMs = 0;
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
        char line[sizeof(s_playIngressInfo)];
        playMonitorFormatLine(ev, line, sizeof(line), now);
        if (strcmp(line, s_playIngressInfo) != 0) {
          snprintf(s_playIngressInfo, sizeof(s_playIngressInfo), "%s", line);
          s_playIngressInfoDirty = true;
        }
      }
    }
  }
  if (s_playIngressInfoDirty && g_screen == Screen::Play && g_settings.playInMonitor) {
    playRedrawTopMarginOverlays();
  }
  s_playIngressInfoDirty = false;

  xyFlushPending(now);
  seqArpProcessDue(now);
  transportTick(now);
  transportSendTickIfNeeded(now);
  bleMidiFlush();
  if (transportExternalClockActive() && g_settings.midiClockSource != 0 && g_settings.clkFollow != 0) {
    const uint16_t extBpm = transportExternalClockBpm();
    if (extBpm >= 40 && extBpm <= 300 && extBpm != g_projectBpm) {
      g_projectBpm = extBpm;
    }
  }

  // Play screen: refresh top margin when external BPM / clock-flash / slaved state changes (not every seq step).
  static uint16_t s_playTopOverlayBpmKey = 0xFFFF;
  static bool s_playTopOverlayFlash = false;
  static bool s_playTopOverlayExtOn = false;
  if (g_screen == Screen::Play && g_settings.midiClockSource != 0 && g_settings.clkFollow != 0) {
    const bool extOn = transportExternalClockActive();
    const uint16_t eb = transportExternalClockBpm();
    const bool flashOn = (now < s_playClockFlashUntilMs);
    const uint16_t bpmKey = (extOn && eb >= 40 && eb <= 300) ? eb : 0;
    if (bpmKey != s_playTopOverlayBpmKey || flashOn != s_playTopOverlayFlash || extOn != s_playTopOverlayExtOn) {
      s_playTopOverlayBpmKey = bpmKey;
      s_playTopOverlayFlash = flashOn;
      s_playTopOverlayExtOn = extOn;
      playRedrawTopMarginOverlays();
    }
  }

  static uint8_t s_prevAudible = 255;
  static uint8_t s_prevCntIn = 255;
  if (transportIsPlaying()) {
    const uint8_t au = transportAudibleStep();
    if (au != s_prevAudible) {
      const uint8_t prevAud = s_prevAudible;
      s_prevAudible = au;
      transportEmitSequencerStepMidi(au);
      seqArpProcessDue(now);
      bleMidiFlush();
      const uint8_t flashDiv = g_settings.clkFlashEdge ? 2U : 4U;
      if (transportExternalClockActive() && (au % flashDiv) == 0U) {
        s_playClockFlashUntilMs = now + 90;
      }
      if (g_screen == Screen::Sequencer) {
        if (g_seqSliderActive || (s_seqChordDropStep >= 0 && s_seqChordDropStep < 16)) {
          drawSequencerSurface(-1, false);
        } else {
          sequencerRedrawPlayheadDelta(prevAud, au);
        }
      }
      if (g_screen == Screen::Transport) {
        if (s_transportDropKind >= 0) {
          drawTransportSurface(false);
        } else {
          transportRedrawStepDisplayOnly();
        }
      }
    }
  } else {
    if (s_prevAudible != 255) {
      seqArpStopAll(true);
      for (uint8_t ch = 0; ch < 16; ++ch) {
        midiSilenceLastChord(ch);
      }
      bleMidiFlush();
    }
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

  static bool s_topHadTouch = false;
  static int16_t s_topEndY = 0;
  const bool modalBlocking = g_screen == Screen::Settings || g_screen == Screen::KeyPicker ||
                             g_screen == Screen::ProjectNameEdit || g_screen == Screen::SdProjectPick;
  const bool transportOpen = (g_screen == Screen::Transport || g_screen == Screen::TransportBpmEdit ||
                              g_screen == Screen::TransportTapTempo);
  ::ui::HostScreen topHs = ::ui::HostScreen::Other;
  switch (g_screen) {
    case Screen::Play:
      topHs = ::ui::HostScreen::Play;
      break;
    case Screen::Sequencer:
      topHs = ::ui::HostScreen::Sequencer;
      break;
    case Screen::XyPad:
      topHs = ::ui::HostScreen::XyPad;
      break;
    default:
      break;
  }
  if (touchCount > 0) {
    const auto& dTop = M5.Touch.getDetail(0);
    if (dTop.isPressed()) {
      s_topEndY = dTop.y;
      ::ui::topDrawerOnTouchDown(g_topDrawerUi, topHs, transportOpen, modalBlocking, dTop.y);
    }
    s_topHadTouch = true;
  } else {
    if (s_topHadTouch) {
      if (::ui::topDrawerOnTouchUpAfterSwipe(g_topDrawerUi, topHs, transportOpen, modalBlocking, s_topEndY)) {
        switch (g_screen) {
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
        delay(10);
        return;
      }
    }
    s_topHadTouch = false;
  }

  // Swipe-up from select button → open Transport drawer (only on non-Transport screens).
  if (g_screen != Screen::Transport && g_screen != Screen::TransportBpmEdit &&
      g_screen != Screen::TransportTapTempo && g_screen != Screen::Settings &&
      g_screen != Screen::KeyPicker && g_screen != Screen::ProjectNameEdit &&
      g_screen != Screen::SdProjectPick) {
    if (touchCount >= 1) {
      const auto& d = M5.Touch.getDetail(0);
      if (d.isPressed()) {
        if (!s_selectSwipeTracking && pointInRect(d.x, d.y, g_bezelSelect)) {
          s_selectSwipeTracking = true;
          s_selectSwipeStartY = d.y;
        }
        if (s_selectSwipeTracking && (s_selectSwipeStartY - d.y) >= kSwipeUpThresholdPx) {
          s_selectSwipeTracking = false;
          s_shiftSelectDown = false;
          s_shiftSelectDownMs = 0;
          wasTouchActive = true;
          suppressNextPlayTap = true;
          s_transportIgnoreUntilTouchUp = true;
          openTransportDrawer();
          delay(10);
          return;
        }
      }
    }
    if (touchCount == 0) {
      s_selectSwipeTracking = false;
    }
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
