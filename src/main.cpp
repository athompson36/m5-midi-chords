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
#include "m5chords_app/M5ChordsAppShared.h"

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

namespace m5chords_app {

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
int g_pickTonic = 0;
KeyMode g_pickMode = KeyMode::Major;

Rect g_keyPickCells[ChordModel::kKeyCount];
Rect g_modePickCells[static_cast<int>(KeyMode::kCount)];
Rect g_keyPickCancel;
Rect g_keyPickDone;

ChordModel g_model;
Screen g_screen = Screen::Play;

String g_lastAction = "";

uint32_t touchStartMs = 0;

SettingsEntryGestureState g_settingsEntryGesture;
bool suppressNextPlayTap = false;

int g_settingsRow = 0;
bool g_factoryResetConfirmArmed = false;
char g_settingsFeedback[48] = "";
SettingsConfirmAction g_settingsConfirmAction = SettingsConfirmAction::None;

SettingsPanel g_settingsPanel = SettingsPanel::Menu;
int g_settingsCursorRow = 0;
int g_settingsListScroll = 0;
int g_userGuideScroll = 0;

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

SeqTool g_seqTool = SeqTool::None;
int8_t g_seqProbFocusStep = -1;
Rect g_seqSliderPanel{};
bool g_seqSliderActive = false;
static bool g_seqDraggingSlider = false;
/// While Shift is active, top row of seq cells (0-3) shows tool picks.
bool s_seqSelectHeld = false;
int s_seqChordDropStep = -1;
int s_seqChordDropTouchStartX = 0;
int s_seqChordDropTouchStartY = 0;
int s_seqChordDropScroll = 0;
int s_seqChordDropDragLastY = -1;
int s_seqChordDropFingerScrollCount = 0;
int s_seqStepEditStep = -1;
bool s_seqStepEditJustOpened = false;
Rect g_seqStepEditPanel{};
Rect g_seqStepEditVoMinus{}, g_seqStepEditVoPlus{};
Rect g_seqStepEditPatMinus{}, g_seqStepEditPatPlus{};
Rect g_seqStepEditRateMinus{}, g_seqStepEditRatePlus{};
Rect g_seqStepEditOctMinus{}, g_seqStepEditOctPlus{};
Rect g_seqStepEditGateMinus{}, g_seqStepEditGatePlus{};
Rect g_seqStepEditArpToggle{};
Rect g_seqStepEditDone{};
Rect g_seqStepEditDelete{};
int8_t s_shiftSeqFocusStep = -1;
int8_t s_shiftSeqFocusStepByLane[3] = {-1, -1, -1};
uint8_t s_shiftSeqFocusField = 0;  // 0 Div, 1 Arp, 2 Pat, 3 ADiv
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
bool s_shiftActive = false;
static bool s_shiftSelectDown = false;
bool s_shiftTriggeredThisHold = false;
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
uint32_t s_playCategoryTouchStartMs = 0;
/// Play category: last painted finger cell (-9999 = force full paint on next change).
static int s_playCategoryFingerPainted = -9999;
static bool s_playChordTriggeredThisTouch = false;
static uint8_t s_playChordActiveCh0 = 0;

constexpr uint32_t kPlayBezelCycleHoldMs = 450U;
constexpr uint32_t kPlayBezelCycleRepeatMs = 140U;
constexpr int kSeqHitPadPx = 4;
constexpr int kPlayCategoryHitPadPx = 5;
const char* kSeqStepDivLabs[4] = {"1x", "2x", "4x", "8x"};
const char* kSeqArpPatLabs[4] = {"Up", "Dn", "UD", "Rnd"};

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
uint8_t s_xyMidiSentX = 255;
uint8_t s_xyMidiSentY = 255;
uint32_t s_xyLastSendMsX = 0;
uint32_t s_xyLastSendMsY = 0;
XyAxisPending s_xyPendingX{};
XyAxisPending s_xyPendingY{};

int g_lastPlayedOutline = -1;

char g_projectCustomName[48] = "";
char g_lastProjectFolder[48] = "";

uint8_t g_uiTheme = 0;

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
static int s_settingsTouchStartX = 0;
static int s_settingsTouchStartY = 0;
int s_keyPickTouchStartX = 0;
int s_keyPickTouchStartY = 0;
int s_sdPickTouchStartX = 0;
int s_sdPickTouchStartY = 0;

/// Transport drawer: swipe up from select to open, stores screen to return to.
static Screen s_screenBeforeTransport = Screen::Play;
static bool s_selectSwipeTracking = false;
static int s_selectSwipeStartY = 0;
static constexpr int kSwipeUpThresholdPx = 40;
// True right after swipe-opening Transport; ignore carried touch until finger-up.
bool s_transportIgnoreUntilTouchUp = false;

/// Movement past this distance from touch-down suppresses row pick (scroll vs tap).
static constexpr int kUiScrollSuppressPickPx = 14;

bool touchMovedPastSuppressThreshold(int sx, int sy, int ex, int ey) {
  const int dx = ex - sx;
  const int dy = ey - sy;
  const int t = kUiScrollSuppressPickPx;
  return (static_cast<int32_t>(dx) * dx + static_cast<int32_t>(dy) * dy) > (t * t);
}
int8_t s_xyTouchZone = -1;
bool s_xyTwoFingerSurfaceDrawn = false;
static MidiIngressParser s_midiIngressParser;
MidiInState s_midiInState;
MidiEventHistory s_midiEventHistory;
static uint32_t s_playClockFlashUntilMs = 0;
char s_midiDetectedChord[12] = "";
uint32_t s_midiDetectedChordUntilMs = 0;
static char s_surpriseChordLabel[12] = "";
static bool s_surpriseChordActive = false;
char s_midiDetectedSuggest[12] = "";
uint32_t s_midiDetectedSuggestUntilMs = 0;
char s_midiSuggestTop3[3][12] = {};
int16_t s_midiSuggestTop3Score[3] = {0, 0, 0};
uint32_t s_midiSuggestTop3UntilMs = 0;
uint32_t s_midiSuggestStableUntilMs = 0;
MidiSource s_midiSuggestLastSource = MidiSource::Usb;
uint8_t s_midiSuggestLastChannel = 0;
uint32_t s_midiSuggestLastAtMs = 0;
char s_playIngressInfo[44] = "";
uint32_t s_playIngressInfoUntilMs = 0;
bool s_playIngressInfoDirty = false;
uint32_t s_playIngressLastMusicalMs = 0;
uint32_t s_playIngressInfoEventMs = 0;
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
static Rect g_midiDbgSourceChip{};
static Rect g_midiDbgTypeChip{};
static Rect g_midiDbgResetChip{};
uint32_t s_panicTriggerCounts[static_cast<size_t>(PanicTrigger::Count)] = {};

void schedulePlayAutoSilence(uint8_t channel0) {
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

uint16_t g_bpmEditValue = 120;
Rect g_bpmEditM5;
Rect g_bpmEditP5;
Rect g_bpmEditM1;
Rect g_bpmEditP1;
Rect g_bpmEditOk;
Rect g_bpmEditCancel;

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

uint32_t transportBpmLongMs() {
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

uint32_t s_transportTouchStartMs = 0;
int s_transportTouchStartX = 0;
int s_transportTouchStartY = 0;
bool s_transportTouchDown = false;

uint32_t s_tapTempoPrevTapMs = 0;
uint16_t s_tapTempoPreviewBpm = 120;
Rect g_trTapTempoDone;

int8_t s_transportDropKind = -1;
int s_transportDropScroll = 0;
int s_transportDropDragLastY = -1;
/// Finger-drag steps that changed list scroll this touch (suppress row pick on lift).
int s_transportDropFingerScrollCount = 0;

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

Rect playVoicingTrackRect() {
  const Rect& p = g_playVoicingPanel;
  const int trackTop = p.y + 14;
  const int trackBottom = p.y + p.h - 4;
  const int trackH = max(10, trackBottom - trackTop);
  return {p.x + 8, trackTop, max(8, p.w - 50), trackH};
}

void playSetVoicingFromX(int px) {
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

void layoutPlayVoicingPanel(int w, int h) {
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
void playRedrawGridBand() {
  playRedrawAllCellsInPlace(-100, false);
}

void playRedrawAfterOutlineChange(int previousOutline, int fingerChord, bool fingerOnKey) {
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

void playRedrawInPlace(int fingerChord, bool fingerOnKey);
void playRedrawFingerHighlightOnly(int fingerChord, bool fingerOnKey);

void playRedrawClearFingerHighlight() {
  playRedrawFingerHighlightOnly(-100, false);
}

// Updates finger highlight on chord pads / key without full-panel fillRect (see §2 UI backlog).
void playRedrawFingerHighlightOnly(int fingerChord, bool fingerOnKey) {
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
void playRedrawInPlace(int fingerChord, bool fingerOnKey) {
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

bool playHandleBezelFastCycle(int direction) {
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

void playResetBezelFastCycle() {
  s_playBezelHoldStartMs = 0;
  s_playBezelRepeatMs = 0;
  s_playBezelHoldDir = 0;
}

bool playTriggerSurroundByIdx(int hit, bool autoSilence) {
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

void beforeLeaveSequencer() {
  if (g_screen == Screen::Sequencer) {
    seqPatternSave(g_seqPattern);
    seqExtrasSave(&g_seqExtras);
  }
}

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
      drawXyPadSurface(true);
      break;
    default:
      break;
  }
}

void openTransportDrawer() {
  if (g_screen == Screen::Transport || g_screen == Screen::TransportBpmEdit ||
      g_screen == Screen::TransportTapTempo)
    return;
  ::ui::topDrawerClose(g_topDrawerUi);
  panicForTrigger(PanicTrigger::RingNavigation);
  beforeLeaveSequencer();
  if (g_screen == Screen::Play) s_playVoicingPanelOpen = false;
  s_screenBeforeTransport = g_screen;
  g_screen = Screen::Transport;
  drawTransportSurface(true);
}

void closeTransportDrawer() {
  transportDropDismiss();
  g_screen = s_screenBeforeTransport;
  switch (g_screen) {
    case Screen::Play:          drawPlaySurface(); break;
    case Screen::Sequencer:     drawSequencerSurface(); break;
    case Screen::XyPad:         drawXyPadSurface(true); break;
    case Screen::PlayCategory:  drawPlayCategorySurface(); break;
    case Screen::MidiDebug:     drawMidiDebugSurface(); break;
    default:                    drawPlaySurface(); g_screen = Screen::Play; break;
  }
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

}  // namespace m5chords_app

using namespace m5chords_app;

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
        drawTransportSurface(false);
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
            drawXyPadSurface(false);
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
