#pragma once

#include <stddef.h>
#include <stdint.h>

#include <WString.h>

#include "AppSettings.h"
#include "ChordModel.h"
#include "MidiIngress.h"
#include "MidiEventHistory.h"
#include "MidiInState.h"
#include "SettingsEntryGesture.h"
#include "ui/UiTypes.h"

namespace ui {
struct DisplayAdapter;
}

struct XyAxisPending {
  bool dirty = false;
  uint8_t channel = 0;
  uint8_t cc = 0;
  uint8_t value = 0;
};

namespace m5chords_app {

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

extern uint32_t s_panicTriggerCounts[static_cast<size_t>(PanicTrigger::Count)];

enum class SettingsConfirmAction : uint8_t { None, RestoreSd, ClearMidiDebug };

enum class SettingsPanel : uint8_t { Menu, Midi, Display, SeqArp, Storage, UserGuide };

enum class HoldProgressOwner : uint8_t { None, SettingsSingle, SettingsDual, TransportBpm, TransportPsr };

enum class SeqTool : uint8_t { None, Quantize, Swing, StepProb, ChordRand };

static constexpr int kBezelBarH = 20;
static constexpr uint8_t kSeqRest = 0xFE;
static constexpr uint8_t kSeqTie = 0xFD;
static constexpr uint8_t kSeqSurprise = 0xFC;

static constexpr int8_t kTransportDropMidiOut = 0;
static constexpr int8_t kTransportDropMidiIn = 1;
static constexpr int8_t kTransportDropClock = 2;
static constexpr int8_t kTransportDropStrum = 3;

static constexpr int kProjEditLen = 24;
static constexpr uint32_t kXyCcMinIntervalMs = 10U;

// --- Globals (defined in main.cpp unless noted) ---
extern int g_pickTonic;
extern KeyMode g_pickMode;
extern ::ui::Rect g_keyPickCells[ChordModel::kKeyCount];
extern ::ui::Rect g_modePickCells[static_cast<int>(KeyMode::kCount)];
extern ::ui::Rect g_keyPickCancel;
extern ::ui::Rect g_keyPickDone;

extern ChordModel g_model;
extern Screen g_screen;

extern String g_lastAction;
extern uint32_t touchStartMs;

extern SettingsEntryGestureState g_settingsEntryGesture;
extern bool suppressNextPlayTap;

extern int g_settingsRow;
extern bool g_factoryResetConfirmArmed;
extern char g_settingsFeedback[48];
extern SettingsConfirmAction g_settingsConfirmAction;

extern SettingsPanel g_settingsPanel;
extern int g_settingsCursorRow;
extern int g_settingsListScroll;
extern int g_userGuideScroll;

extern ::ui::Rect g_keyRect;
extern ::ui::Rect g_chordRects[ChordModel::kSurroundCount];
extern ::ui::Rect g_bezelBack;
extern ::ui::Rect g_bezelSelect;
extern ::ui::Rect g_bezelFwd;

extern ::ui::Rect g_seqCellRects[16];
extern ::ui::Rect g_seqTabRects[3];
extern uint8_t g_seqMidiCh[3];

extern SeqTool g_seqTool;
extern int8_t g_seqProbFocusStep;
extern ::ui::Rect g_seqSliderPanel;
extern bool g_seqSliderActive;

extern const char* kSeqStepDivLabs[4];
extern const char* kSeqArpPatLabs[4];

extern bool s_seqSelectHeld;
extern int s_seqChordDropStep;
extern int s_seqChordDropTouchStartX;
extern int s_seqChordDropTouchStartY;
extern int s_seqChordDropScroll;
extern int s_seqChordDropDragLastY;
extern int s_seqChordDropFingerScrollCount;
extern int s_seqStepEditStep;
extern bool s_seqStepEditJustOpened;
extern ::ui::Rect g_seqStepEditPanel;
extern ::ui::Rect g_seqStepEditVoMinus;
extern ::ui::Rect g_seqStepEditVoPlus;
extern ::ui::Rect g_seqStepEditPatMinus;
extern ::ui::Rect g_seqStepEditPatPlus;
extern ::ui::Rect g_seqStepEditRateMinus;
extern ::ui::Rect g_seqStepEditRatePlus;
extern ::ui::Rect g_seqStepEditOctMinus;
extern ::ui::Rect g_seqStepEditOctPlus;
extern ::ui::Rect g_seqStepEditGateMinus;
extern ::ui::Rect g_seqStepEditGatePlus;
extern ::ui::Rect g_seqStepEditArpToggle;
extern ::ui::Rect g_seqStepEditDone;
extern ::ui::Rect g_seqStepEditDelete;
extern int8_t s_shiftSeqFocusStep;
extern int8_t s_shiftSeqFocusStepByLane[3];
extern uint8_t s_shiftSeqFocusField;
extern bool s_shiftTriggeredThisHold;

extern ::ui::Rect g_xyPadRect;
extern ::ui::Rect g_xyCfgChRect;
extern ::ui::Rect g_xyCfgCcARect;
extern ::ui::Rect g_xyCfgCcBRect;
extern ::ui::Rect g_xyCfgCvARect;
extern ::ui::Rect g_xyCfgCvBRect;
extern uint8_t g_xyOutChannel;
extern uint8_t g_xyCcA;
extern uint8_t g_xyCcB;
extern uint8_t g_xyCurveA;
extern uint8_t g_xyCurveB;

extern int g_lastPlayedOutline;

extern char g_projectCustomName[48];
extern char g_lastProjectFolder[48];

extern uint8_t g_uiTheme;

extern char g_projEditBuf[kProjEditLen + 1];
extern int g_projEditCursor;
extern ::ui::Rect g_peSave;
extern ::ui::Rect g_peClear;
extern ::ui::Rect g_peCancel;

extern char g_sdPickNames[8][48];
extern int g_sdPickCount;
extern ::ui::Rect g_sdPickRows[8];
extern ::ui::Rect g_sdPickCancel;

extern MidiEventHistory s_midiEventHistory;
extern MidiInState s_midiInState;

extern bool pointInRect(int px, int py, const ::ui::Rect& r);
bool pointInRectPad(int px, int py, const ::ui::Rect& r, int pad);
constexpr int kPlayHitPadPx = 5;
constexpr uint32_t kTapDebounceMs = 35U;
constexpr uint8_t kShiftPadCcMap[4] = {20, 21, 22, 23};
constexpr uint8_t kShiftPadProgramMap[4] = {0, 1, 2, 3};

// Transport surface (state shared with TransportScreen.cpp)
extern uint32_t s_transportTouchStartMs;
extern int s_transportTouchStartX;
extern int s_transportTouchStartY;
extern bool s_transportTouchDown;

extern uint32_t s_tapTempoPrevTapMs;
extern uint16_t s_tapTempoPreviewBpm;
extern ::ui::Rect g_trTapTempoDone;

extern int8_t s_transportDropKind;
extern int s_transportDropScroll;
extern int s_transportDropDragLastY;
extern int s_transportDropFingerScrollCount;

extern bool s_transportIgnoreUntilTouchUp;

extern ::ui::Rect g_trPlay;
extern ::ui::Rect g_trStop;
extern ::ui::Rect g_trRec;
extern ::ui::Rect g_trMetro;
extern ::ui::Rect g_trCntIn;
extern ::ui::Rect g_trMidiOutBtn;
extern ::ui::Rect g_trMidiInBtn;
extern ::ui::Rect g_trSyncBtn;
extern ::ui::Rect g_trStrumBtn;
extern ::ui::Rect g_trBpmTap;
extern ::ui::Rect g_trStepDisplay;

extern uint16_t g_bpmEditValue;
extern ::ui::Rect g_bpmEditM5;
extern ::ui::Rect g_bpmEditP5;
extern ::ui::Rect g_bpmEditM1;
extern ::ui::Rect g_bpmEditP1;
extern ::ui::Rect g_bpmEditOk;
extern ::ui::Rect g_bpmEditCancel;

// --- Functions (main.cpp unless noted) ---
void panicForTrigger(PanicTrigger trigger);
void clearMidiDebugDiagnosticsState();
void midiDebugResetUiDiagnosticsState();
void resetSettingsNav();

void processIncomingMidiEvent(const MidiEvent& ev, uint32_t nowMs);
void playMonitorFormatLine(const MidiEvent& ev, char* out, size_t outSize, uint32_t nowMs);

uint8_t applyOutputVelocityCurve(uint8_t raw);
void seqArpStopAll(bool silence);
void seqArpProcessDue(uint32_t nowMs);
void transportEmitSequencerStepMidi(uint8_t step);
void transportSendTickIfNeeded(uint32_t nowMs);

bool touchMovedPastSuppressThreshold(int sx, int sy, int ex, int ey);
uint32_t transportBpmLongMs();
uint32_t settingsEntryHoldMs();
void drawHoldProgressStrip(HoldProgressOwner owner, uint32_t elapsedMs, uint32_t targetMs, const char* label);
void clearHoldProgressStrip(HoldProgressOwner owner);
bool checkBezelLongPressSettings(uint8_t touchCount);

void transportDropDismiss();
void transportDropOpen(int8_t kind, int w, int h);

void drawRoundedButton(const ::ui::Rect& r, uint16_t bg, const char* label, uint8_t textSize = 2);
void drawKeyCenterTwoLine(const ::ui::Rect& r, const char* line1, const char* line2);
void drawBezelBarStrip();
void applyBrightness();
void wakeDisplayBacklightFromIdleDim();
void updateBacklightAutoDim(uint32_t now, uint8_t touchCount);

void drawPlayCategorySurface(int fingerCell = -1);

void drawPlaySurface(int fingerChord = -100, bool fingerOnKey = false);

::ui::Rect playVoicingTrackRect();
void playSetVoicingFromX(int px);
void layoutPlayVoicingPanel(int w, int h);
void playRedrawGridBand();
void playRedrawAfterOutlineChange(int previousOutline, int fingerChord = -100, bool fingerOnKey = false);
void playRedrawInPlace(int fingerChord = -100, bool fingerOnKey = false);
void playRedrawFingerHighlightOnly(int fingerChord, bool fingerOnKey);
void playRedrawClearFingerHighlight();
bool playHandleBezelFastCycle(int direction);
bool playTriggerSurroundByIdx(int hit, bool autoSilence = true);

void processPlayTouch(uint8_t touchCount, int w, int h);
int hitTestPlay(int px, int py);
void schedulePlayAutoSilence(uint8_t channel0);
void drawSequencerSurface(int fingerCell = -1, bool fullClear = true);
void drawXyPadSurface(bool fullClear = true);

void drawMidiDebugSurface(bool fullClear = true);
void openProjectNameEditor();

void drawKeyPicker();
void processKeyPickerTouch(uint8_t touchCount, int w, int h);
void drawProjectNameEdit();
void processProjectNameEditTouch(uint8_t touchCount, int w, int h);
void drawSdProjectPick();
void processSdProjectPickTouch(uint8_t touchCount, int w, int h);
void drawSettingsUi();
void processMidiDebugTouch(uint8_t touchCount, int w, int h);
void processPlayCategoryTouch(uint8_t touchCount, int w, int h);
void processSequencerTouch(uint8_t touchCount, int w, int h);
void midiPanicAllChannels();
const char* panicTriggerShortLabel(PanicTrigger trigger);

void layoutBottomBezels(int w, int h);
void computePlaySurfaceLayout(int w, int h);

void computeSequencerLayout(int w, int h);
int hitTestSeqTab(int px, int py);
uint8_t seqLaneClamped();
::ui::Rect seqSliderTrackRect();
void seqSetSliderValueFromX(int px);
uint8_t seqSliderDisplayedValue();
int hitTestSeq(int px, int py);
void beforeLeaveSequencer();

uint16_t colorForRole(ChordRole role);
bool seqSelectToolsActive();
int seqChordDropItemCount();
int seqChordDropVisibleRows(int h);
void seqChordDropComputeLayout(int w, int h, int* outX, int* outY, int* outW, int* outRowH, int* outVis);
void seqStepEditComputeLayout(int w, int h);
void sequencerRedrawPlayheadDelta(uint8_t oldAudible, uint8_t newAudible);

void paintAppTopDrawer();
void playResetBezelFastCycle();

void drawXyTopDrawer(::ui::DisplayAdapter& d);
void drawPlayTopDrawer(::ui::DisplayAdapter& d);
void drawSequencerTopDrawer(::ui::DisplayAdapter& d);
bool playProcessTopDrawerTouch(uint8_t touchCount, int w, int h);
bool sequencerProcessTopDrawerTouch(uint8_t touchCount, int w, int h);
bool xyProcessTopDrawerTouch(uint8_t touchCount, int w, int h);

void navigateMainRing(int direction);

void resolveBackupFolder(char out[48]);
void applySdRestoreFromFolder(const char* folder);

extern uint8_t s_xyMidiSentX;
extern uint8_t s_xyMidiSentY;
extern uint32_t s_xyLastSendMsX;
extern uint32_t s_xyLastSendMsY;
extern XyAxisPending s_xyPendingX;
extern XyAxisPending s_xyPendingY;

void openTransportDrawer();
void closeTransportDrawer();

bool tryEnterSettingsTwoFingerLong(uint8_t touchCount, int w, int h);

extern int8_t s_xyTouchZone;
extern bool s_xyTwoFingerSurfaceDrawn;

void drawTransportSurface(bool fullClear = true);

void processTransportTouch(uint8_t touchCount, int w, int h);
void drawTransportBpmEdit();
void processTransportBpmEditTouch(uint8_t touchCount, int w, int h);
void drawTransportTapTempo();
void processTransportTapTempoTouch(uint8_t touchCount, int w, int h);

void drawTopSystemStatus(int w, int yTop, const char* extBpmText, uint16_t extBpmColor);

void xyFlushPending(uint32_t nowMs);
void transportRedrawStepDisplayOnly();

void processXyTouch(uint8_t touchCount, int w, int h);
void processSettingsTouch(uint8_t touchCount, int w, int h);

}  // namespace m5chords_app
