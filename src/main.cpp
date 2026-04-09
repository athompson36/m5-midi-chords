#include <M5Unified.h>
#include <esp_random.h>
#include <stdlib.h>
#include <string.h>

#include "AppSettings.h"
#include "BuildInfo.h"
#include "ChordModel.h"
#include "MidiOut.h"
#include "SdBackup.h"
#include "SettingsEntryGesture.h"
#include "Arpeggio.h"
#include "SettingsStore.h"
#include "SeqExtras.h"
#include "Transport.h"
#include "UiTheme.h"

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
  Sequencer,
  XyPad,
  Transport,
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
void drawProjectNameEdit();
void drawSdProjectPick();
void drawSettingsUi();

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

int g_settingsRow = 0;
bool g_factoryResetConfirmArmed = false;
char g_settingsFeedback[48] = "";

enum class SettingsPanel : uint8_t { Menu, Midi, Display, SeqArp, Storage };

SettingsPanel g_settingsPanel = SettingsPanel::Menu;
int g_settingsCursorRow = 0;
int g_settingsListScroll = 0;

constexpr uint32_t kDualCornerHoldMs = 800;

constexpr int kBezelBarH = 20;

constexpr uint8_t kSeqRest = 0xFE;
constexpr uint8_t kSeqTie = 0xFD;

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
/// When true, top row of seq cells (0–3) shows tool picks; toggled by a single-finger tap on SELECT.
static bool s_seqSelectHeld = false;
static int s_seqChordDropStep = -1;
static uint32_t s_seqStepDownMs = 0;
static int s_seqStepDownIdx = -1;
static bool s_seqLongPressHandled = false;
static constexpr uint32_t kSeqLongPressMs = 400;
static uint8_t s_playGestureMaxTouches = 0;
static bool s_playVoicingCombo = false;
/// Tap SELECT (single-finger) toggles key-edit mode: center key highlighted; tap key to open picker.
static bool s_playSelectLatched = false;

Rect g_xyPadRect;
uint8_t g_xyCcA = 74;
uint8_t g_xyCcB = 71;
static uint8_t s_xyMidiSentX = 255;
static uint8_t s_xyMidiSentY = 255;

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

Rect g_trPlay;
Rect g_trStop;
Rect g_trRec;
Rect g_trMetro;
Rect g_trCntIn;

void layoutBottomBezels(int w, int h);
void beforeLeaveSequencer();
void drawPlaySurface(int fingerChord = -100, bool fingerOnKey = false);
void drawSequencerSurface(int fingerCell = -1);
void drawXyPadSurface();

void drawTransportSurface();
void processTransportTouch(uint8_t touchCount, int w, int h);

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
  M5.Display.drawString("SELECT", g_bezelSelect.x + g_bezelSelect.w / 2, h - 2);
  M5.Display.drawString("FWD", g_bezelFwd.x + g_bezelFwd.w / 2, h - 2);
}

bool checkBezelLongPressSettings(uint8_t touchCount) {
  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    if (d.isPressed() && pointInRect(d.x, d.y, g_bezelFwd)) {
      if (s_bezelLongPressMs == 0) s_bezelLongPressMs = millis();
      if (!s_bezelLongPressFired && millis() - s_bezelLongPressMs >= kDualCornerHoldMs) {
        s_bezelLongPressFired = true;
        suppressNextPlayTap = true;
        g_screen = Screen::Settings;
        resetSettingsNav();
        return true;
      }
      return false;
    }
  }
  s_bezelLongPressMs = 0;
  s_bezelLongPressFired = false;
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
  M5.Display.drawString("BACK/FWD: Transport / Pad / Seq / XY   SELECT: key edit", 4, 2);

  const bool showLp = fingerChord < -50;
  drawPlayKeyCell(fingerOnKey, showLp);
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    drawPlayChordCell(i, fingerChord, showLp);
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

  drawBezelBarStrip();
  M5.Display.endWrite();
}

int hitTestPlay(int px, int py) {
  if (pointInRect(px, py, g_keyRect)) return -2;  // key center
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    if (pointInRect(px, py, g_chordRects[i])) return i;
  }
  return -1;  // nothing
}

int hitTestSeq(int px, int py) {
  for (int i = 0; i < 16; ++i) {
    if (pointInRect(px, py, g_seqCellRects[i])) return i;
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
  if (v <= 7) return g_model.surround[v].name;
  return "?";
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
  M5.Display.drawString("tap=chord  hold=clear  SELECT=toggle tools", 4, 2);

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
      const bool on = (g_seqTool == kToolMap[i]);
      uint16_t bg = on ? g_uiPalette.seqLaneTab : g_uiPalette.accentPress;
      drawRoundedButton(r, bg, kToolLabs[i], 1);
      if (on) {
        M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                               g_uiPalette.highlightRing);
      }
      continue;
    }

    const uint8_t v = g_seqPattern[g_seqLane][i];
    uint16_t bg = g_uiPalette.panelMuted;
    if (v == kSeqRest) {
      bg = g_uiPalette.seqRest;
    } else if (v == kSeqTie) {
      bg = g_uiPalette.seqTie;
    } else {
      bg = colorForRole(g_model.surround[v].role);
    }
    if (i == fingerCell) {
      bg = g_uiPalette.accentPress;
    }
    drawRoundedButton(r, bg, seqStepLabel(v), 1);
    const uint8_t lane = seqLaneClamped();
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
    constexpr int items = 10;
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
      } else {
        lab = "- rest";
      }
      uint16_t bg;
      if (r < 8) {
        bg = colorForRole(g_model.surround[r].role);
      } else if (r == 8) {
        bg = g_uiPalette.seqTie;
      } else {
        bg = g_uiPalette.seqRest;
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
  constexpr int topBlock = 58;
  constexpr int coordH = 12;
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
  snprintf(sub, sizeof(sub), "drag pad  CC%u / CC%u", (unsigned)g_xyCcA, (unsigned)g_xyCcB);
  M5.Display.drawString(sub, w / 2, 22);
  M5.Display.drawString("BACK/FWD = Transport / Pad / Seq / XY", w / 2, 34);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString(g_xyRecordToSeq ? "Mode: REC->SEQ (SELECT+pad=toggle)" : "Mode: LIVE CC",
                        w / 2, 46);

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

void beforeLeaveSequencer() {
  if (g_screen == Screen::Sequencer) {
    seqPatternSave(g_seqPattern);
    seqExtrasSave(&g_seqExtras);
  }
}

bool tryEnterSettingsTwoFingerLong(uint8_t touchCount, int w, int h);

void navigateMainRing(int direction) {
  static const Screen kRing[] = {Screen::Transport, Screen::Play, Screen::Sequencer, Screen::XyPad};
  beforeLeaveSequencer();
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

  const uint8_t midiCh = static_cast<uint8_t>(g_settings.midiOutChannel - 1);

  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    const bool bezel = pointInRect(d.x, d.y, g_bezelBack) || pointInRect(d.x, d.y, g_bezelSelect) ||
                       pointInRect(d.x, d.y, g_bezelFwd);
    if (bezel) {
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
        if (vx != s_xyMidiSentX) {
          midiSendControlChange(midiCh, g_xyCcA, vx);
          s_xyMidiSentX = vx;
        }
        if (vy != s_xyMidiSentY) {
          midiSendControlChange(midiCh, g_xyCcB, vy);
          s_xyMidiSentY = vy;
        }
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
    if (pointInRect(hx, hy, g_bezelSelect)) {
      drawXyPadSurface();
      return;
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
  const SettingsEntryGestureResult r = settingsEntryGesture_update(
      g_settingsEntryGesture, millis(), n, xs, ys, pr, back, fwd, kDualCornerHoldMs);
  if (r.enteredSettings) {
    suppressNextPlayTap = r.suppressNextPlayTap;
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
  constexpr int items = 10;
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
        if (!s_seqLongPressHandled && now - s_seqStepDownMs >= kSeqLongPressMs) {
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
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      s_seqChordDropStep = -1;
      s_seqSelectHeld = false;
      navigateMainRing(-1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      s_seqChordDropStep = -1;
      s_seqSelectHeld = false;
      navigateMainRing(1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      const uint8_t maxT = s_seqGestureMaxTouches;
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      s_seqChordDropStep = -1;
      if (maxT <= 1) {
        s_seqSelectHeld = !s_seqSelectHeld;
      }
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
        else v = kSeqRest;
        g_seqPattern[g_seqLane][s_seqChordDropStep] = v;
      }
      s_seqChordDropStep = -1;
      drawSequencerSurface();
      return;
    }

    if (s_seqSelectHeld) {
      const int ht = hitTestSeq(hx, hy);
      if (ht >= 0 && ht < 4) {
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
        seqExtrasSave(&g_seqExtras);
        drawSequencerSurface();
        return;
      }
    }

    const int tabHit = hitTestSeqTab(hx, hy);
    if (tabHit >= 0) {
      g_seqLane = static_cast<uint8_t>(tabHit);
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

  s_drawPlayVoicingShift = (touchCount >= 2 && selectDown && pad0Down && !keyDown);
  if (touchCount >= 2) {
    s_playVoicingCombo = selectDown && pad0Down && !keyDown;
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

    if (s_playGestureMaxTouches >= 2 && s_playVoicingCombo) {
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

    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;
    if (pointInRect(hx, hy, g_bezelBack)) {
      s_playSelectLatched = false;
      navigateMainRing(-1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      s_playSelectLatched = false;
      navigateMainRing(1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      if (gestureMax <= 1) {
        s_playSelectLatched = !s_playSelectLatched;
      }
      drawPlaySurface();
      return;
    }

    const int hit = hitTestPlay(hx, hy);
    if (hit == -2) {
      if (s_playSelectLatched) {
        g_pickTonic = g_model.keyIndex;
        g_pickMode = g_model.mode;
        g_screen = Screen::KeyPicker;
        s_playSelectLatched = false;
        drawKeyPicker();
        return;
      }
      if (g_model.heartAvailable) {
        g_model.nextSurprise();
        g_model.consumeHeart();
        g_lastPlayedOutline = -2;
      } else {
        g_lastPlayedOutline = -2;
        g_model.registerPlay();
      }
      transportSetLiveChord(-1);
      playRedrawAfterOutlineChange();
      return;
    }
    if (hit >= 0 && hit < ChordModel::kSurroundCount) {
      g_lastPlayedOutline = hit;
      g_model.registerPlay();
      transportSetLiveChord(static_cast<int8_t>(hit));
      playRedrawAfterOutlineChange();
      return;
    }
    if (hadHighlight) {
      playRedrawClearFingerHighlight();
    }
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
        g_model.setTonicAndMode(g_pickTonic, g_pickMode);
        chordStateSave(g_model);
        seqPatternSave(g_seqPattern);
        g_lastAction = "Key saved";
        g_screen = Screen::Play;
        drawPlaySurface();
        return;
      }
      if (pointInRect(px, py, g_keyPickCancel)) {
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
  if (!sdBackupReadAll(g_settings, g_model, g_seqPattern, g_seqMidiCh, &g_xyCcA, &g_xyCcB, &bpm,
                       &g_chordVoicing, &g_seqExtras, folder)) {
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
  xyMappingSave(g_xyCcA, g_xyCcB);
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
    g_screen = Screen::Settings;
    drawSettingsUi();
    return;
  }
  for (int i = 0; i < g_sdPickCount && i < 8; ++i) {
    if (pointInRect(hx, hy, g_sdPickRows[i])) {
      applySdRestoreFromFolder(g_sdPickNames[i]);
      g_screen = Screen::Settings;
      drawSettingsUi();
      return;
    }
  }
  drawSdProjectPick();
}

void drawTransportSurface() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  layoutBottomBezels(w, h);

  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Transport", w / 2, 4);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("BACK/FWD: Transport / Pad / Seq / XY", w / 2, 22);

  constexpr int margin = 8;
  constexpr int row1y = 42;
  constexpr int btnH = 44;
  constexpr int gap = 8;
  const int totalW = w - 2 * margin;
  const int half = (totalW - gap) / 2;
  g_trPlay = {margin, row1y, half, btnH};
  g_trStop = {margin + half + gap, row1y, half, btnH};

  const char* playLab = "Play";
  if (transportIsPaused()) {
    playLab = "Play";
  } else if (transportIsPlaying() || transportIsCountIn()) {
    playLab = "Pause";
  }

  drawRoundedButton(g_trPlay, g_uiPalette.settingsBtnActive, playLab, 2);
  drawRoundedButton(g_trStop, g_uiPalette.settingsBtnIdle, "Stop", 2);

  const int row2y = row1y + btnH + gap;
  g_trRec = {margin, row2y, totalW, 38};
  char rlab[16];
  snprintf(rlab, sizeof(rlab), "Record %s", transportRecordArmed() ? "[ARM]" : "");
  drawRoundedButton(g_trRec, transportRecordArmed() ? g_uiPalette.danger : g_uiPalette.panelMuted, rlab,
                    2);

  const int row3y = row2y + 38 + gap;
  const int smallH = 30;
  const int smallHalf = (totalW - gap) / 2;
  g_trMetro = {margin, row3y, smallHalf, smallH};
  g_trCntIn = {margin + smallHalf + gap, row3y, smallHalf, smallH};
  char ml[20];
  char cl[24];
  snprintf(ml, sizeof(ml), "Metro %s", g_prefsMetronome ? "on" : "off");
  snprintf(cl, sizeof(cl), "CntIn %s", g_prefsCountIn ? "on" : "off");
  drawRoundedButton(g_trMetro, g_prefsMetronome ? g_uiPalette.seqLaneTab : g_uiPalette.panelMuted, ml,
                    1);
  drawRoundedButton(g_trCntIn, g_prefsCountIn ? g_uiPalette.seqLaneTab : g_uiPalette.panelMuted, cl,
                    1);

  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  char st[48];
  snprintf(st, sizeof(st), "BPM %u  step %u", (unsigned)g_projectBpm,
           (unsigned)(transportIsPlaying() ? transportAudibleStep() : 0U));
  M5.Display.drawString(st, w / 2, row3y + smallH + 6);

  drawBezelBarStrip();
  M5.Display.endWrite();
}

void processTransportTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);

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
    wasTouchActive = true;
    return;
  }

  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;
  const uint32_t now = millis();

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

static const uint8_t kSetMidi[] = {0, 1, 2, 3, 4};
static const uint8_t kSetDisplay[] = {5, 8, 9, 7};
static const uint8_t kSetSeq[] = {6, 10, 11, 12};
static const uint8_t kSetStorage[] = {13, 14, 15, 16};

static int settingsPanelRowCount(SettingsPanel p) {
  switch (p) {
    case SettingsPanel::Midi:
      return 5;
    case SettingsPanel::Display:
      return 4;
    case SettingsPanel::SeqArp:
      return 4;
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
      c = 5;
      break;
    case SettingsPanel::Display:
      t = kSetDisplay;
      c = 4;
      break;
    case SettingsPanel::SeqArp:
      t = kSetSeq;
      c = 4;
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
      return 17;
    case 2:
    case 3:
    case 4:
      return 4;
    case 5:
      return 10;
    case 6:
      return 9;
    case 8:
      return kUiThemeCount;
    case 9:
      return 11;
    case 10:
      return kArpeggiatorModeCount;
    case 11:
      return 33;
    case 13:
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
      idx = (g_settings.midiInChannel == 0) ? 0 : g_settings.midiInChannel;
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
    case 5:
      snprintf(buf, n, "%u%%", static_cast<unsigned>((opt + 1) * 10));
      break;
    case 6:
      snprintf(buf, n, "%u", static_cast<unsigned>(40 + opt * 10));
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
    case 13:
      snprintf(buf, n, "%s", opt == 0 ? "Cancel" : "Erase everything");
      break;
    default:
      buf[0] = '\0';
      break;
  }
}

static void settingsRunFactoryReset() {
  factoryResetAll(g_settings, g_model);
  memset(g_seqPattern, kSeqRest, sizeof(g_seqPattern));
  seqExtrasLoad(&g_seqExtras);
  seqLaneChannelsLoad(g_seqMidiCh);
  chordVoicingLoad(&g_chordVoicing);
  g_seqLane = 0;
  xyMappingLoad(&g_xyCcA, &g_xyCcB);
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
  if (sdBackupWriteAll(g_settings, g_model, g_seqPattern, g_seqMidiCh, g_xyCcA, g_xyCcB, bm,
                       g_chordVoicing, &g_seqExtras, folder)) {
    lastProjectFolderSave(folder);
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "OK %s/%s", SD_BACKUP_ROOT, folder);
  } else {
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SD backup failed");
  }
}

static void settingsRunRestoreFlow() {
  char dirs[8][48];
  int n = 0;
  if (!sdBackupListProjects(dirs, 8, &n)) {
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SD list failed");
    return;
  }
  if (n == 0) {
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "No projects in " SD_BACKUP_ROOT);
    return;
  }
  if (n == 1) {
    applySdRestoreFromFolder(dirs[0]);
    return;
  }
  g_sdPickCount = n;
  for (int i = 0; i < n && i < 8; ++i) {
    strncpy(g_sdPickNames[i], dirs[i], 47);
    g_sdPickNames[i][47] = '\0';
  }
  g_screen = Screen::SdProjectPick;
  drawSdProjectPick();
}

static void settingsSaveAndExit() {
  g_settings.normalize();
  settingsSave(g_settings);
  chordStateSave(g_model);
  seqPatternSave(g_seqPattern);
  seqExtrasSave(&g_seqExtras);
  seqLaneChannelsSave(g_seqMidiCh);
  chordVoicingSave(g_chordVoicing);
  xyMappingSave(g_xyCcA, g_xyCcB);
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
      g_settings.midiInChannel = static_cast<uint8_t>(opt == 0 ? 0 : opt);
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
    case 5:
      g_settings.brightnessPercent = static_cast<uint8_t>((opt + 1) * 10);
      applyBrightness();
      break;
    case 6:
      g_settings.outputVelocity = static_cast<uint8_t>(40 + opt * 10);
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
    case 13:
      if (opt == 1) {
        settingsRunFactoryReset();
      }
      break;
    default:
      break;
  }
  g_settings.normalize();
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
      settingsRunRestoreFlow();
      return;
    case 16:
      settingsSaveAndExit();
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

static const char* settingsRowTitle(uint8_t rid) {
  switch (rid) {
    case 0:
      return "MIDI out";
    case 1:
      return "MIDI in";
    case 2:
      return "Transport send";
    case 3:
      return "Transport recv";
    case 4:
      return "MIDI clock";
    case 5:
      return "Brightness";
    case 6:
      return "Velocity";
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
      if (g_settings.midiInChannel == 0) {
        snprintf(buf, n, "OMNI");
      } else {
        snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.midiInChannel));
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
    case 5:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(g_settings.brightnessPercent));
      break;
    case 6:
      snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.outputVelocity));
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

}  // namespace

// =====================================================================
//  SETUP / LOOP
// =====================================================================

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(1);
  M5.Display.setTextSize(2);
  settingsLoad(g_settings);
  g_settings.normalize();
  applyBrightness();

  chordStateLoad(g_model);
  seqPatternLoad(g_seqPattern);
  seqExtrasLoad(&g_seqExtras);
  seqLaneChannelsLoad(g_seqMidiCh);
  chordVoicingLoad(&g_chordVoicing);
  xyMappingLoad(&g_xyCcA, &g_xyCcB);
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

  const uint32_t now = millis();
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  const uint8_t touchCount = M5.Touch.getCount();

  transportTick(now);

  static uint8_t s_prevAudible = 255;
  static uint8_t s_prevCntIn = 255;
  if (transportIsPlaying()) {
    const uint8_t au = transportAudibleStep();
    if (au != s_prevAudible) {
      s_prevAudible = au;
      if (g_screen == Screen::Sequencer) {
        drawSequencerSurface();
      }
    }
  } else {
    s_prevAudible = 255;
  }
  if (transportIsCountIn()) {
    const uint8_t cn = transportCountInNumber();
    if (cn != s_prevCntIn) {
      s_prevCntIn = cn;
      if (g_screen == Screen::Play) {
        drawPlaySurface();
      }
    }
  } else {
    s_prevCntIn = 255;
  }

  if (g_screen == Screen::Play || g_screen == Screen::Sequencer ||
      g_screen == Screen::XyPad || g_screen == Screen::Transport) {
    layoutBottomBezels(w, h);
    if (checkBezelLongPressSettings(touchCount)) {
      wasTouchActive = false;
      drawSettingsUi();
      delay(10);
      return;
    }
  }

  switch (g_screen) {
    case Screen::Play:
      processPlayTouch(touchCount, w, h);
      break;
    case Screen::Sequencer:
      processSequencerTouch(touchCount, w, h);
      break;
    case Screen::XyPad:
      processXyTouch(touchCount, w, h);
      break;
    case Screen::Transport:
      processTransportTouch(touchCount, w, h);
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
