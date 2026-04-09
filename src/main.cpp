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
  Boot,
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
void drawSettingsUi(int activeZone = -1);

ChordModel g_model;
AppSettings g_settings;
Screen g_screen = Screen::Boot;

String g_lastAction = "";

bool wasTouchActive = false;
uint32_t touchStartMs = 0;

SettingsEntryGestureState g_settingsEntryGesture;
bool suppressNextPlayTap = false;

int g_settingsRow = 0;
bool g_factoryResetConfirmArmed = false;
char g_settingsFeedback[48] = "";

constexpr uint32_t kDualCornerHoldMs = 800;

constexpr int kBezelBarH = 34;

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
static Rect g_seqToolRects[4];
static Rect g_seqSliderPanel{};
static bool g_seqSliderActive = false;
static bool g_seqDraggingSlider = false;
static uint8_t s_playGestureMaxTouches = 0;
static bool s_playVoicingCombo = false;

Rect g_xyPadRect;
uint8_t g_xyCcA = 74;
uint8_t g_xyCcB = 71;
static uint8_t s_xyMidiSentX = 255;
static uint8_t s_xyMidiSentY = 255;

int g_lastTouchX = 0;
int g_lastTouchY = 0;
int g_lastPlayedOutline = -1;
bool g_comboKeyPickerLatch = false;

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

int buttonZoneForPoint(int x, int y, int w, int h) {
  const int bandH = h / 5;
  const int top = h - bandH;
  if (y < top) return -1;
  const int zoneW = w / 3;
  if (x < zoneW) return 0;
  if (x < zoneW * 2) return 1;
  return 2;
}

bool pointInRect(int px, int py, const Rect& r) {
  return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

struct ShiftSwipeTrack {
  bool armed;
  int sx, sy, lx, ly;
};

ShiftSwipeTrack g_shiftSwipe{};

struct XyComboTrack {
  bool armed;
  int sx, sy, lx, ly;
};

XyComboTrack s_xyCombo{};

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

/// SELECT + swipe on main area: up=Transport, down=Sequencer, left=XyPad, right=Play.
bool pollShiftSwipeNavigate(int w, int h) {
  layoutBottomBezels(w, h);
  const uint8_t n = M5.Touch.getCount();
  if (g_screen != Screen::Play && g_screen != Screen::Sequencer && g_screen != Screen::XyPad &&
      g_screen != Screen::Transport) {
    g_shiftSwipe.armed = false;
    return false;
  }

  if (n >= 2) {
    int sel = -1;
    int main = -1;
    for (uint8_t i = 0; i < n; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (!d.isPressed()) continue;
      if (pointInRect(d.x, d.y, g_bezelSelect)) {
        sel = static_cast<int>(i);
      } else if (d.y < h - kBezelBarH) {
        main = static_cast<int>(i);
      }
    }
    if (sel >= 0 && main >= 0) {
      const auto& dm = M5.Touch.getDetail(static_cast<uint8_t>(main));
      if (!g_shiftSwipe.armed) {
        g_shiftSwipe.sx = dm.x;
        g_shiftSwipe.sy = dm.y;
        g_shiftSwipe.armed = true;
      }
      g_shiftSwipe.lx = dm.x;
      g_shiftSwipe.ly = dm.y;
    }
    return false;
  }

  if (n == 0 && g_shiftSwipe.armed) {
    const int dx = g_shiftSwipe.lx - g_shiftSwipe.sx;
    const int dy = g_shiftSwipe.ly - g_shiftSwipe.sy;
    g_shiftSwipe.armed = false;
    constexpr int kThresh = 45;
    if (abs(dx) < kThresh && abs(dy) < kThresh) {
      return false;
    }
    int dir = -1;
    if (abs(dx) > abs(dy)) {
      if (dx > kThresh) {
        dir = 3;
      } else if (dx < -kThresh) {
        dir = 2;
      }
    } else {
      if (dy > kThresh) {
        dir = 1;
      } else if (dy < -kThresh) {
        dir = 0;
      }
    }
    if (dir < 0) {
      return false;
    }
    beforeLeaveSequencer();
    s_xyMidiSentX = 255;
    s_xyMidiSentY = 255;
    switch (dir) {
      case 0:
        g_screen = Screen::Transport;
        drawTransportSurface();
        break;
      case 1:
        g_screen = Screen::Sequencer;
        drawSequencerSurface();
        break;
      case 2:
        g_screen = Screen::XyPad;
        drawXyPadSurface();
        break;
      case 3:
        g_screen = Screen::Play;
        drawPlaySurface();
        break;
      default:
        break;
    }
    return true;
  }

  if (n < 2) {
    g_shiftSwipe.armed = false;
  }
  return false;
}

// --- Brightness ---

void applyBrightness() {
  uint8_t v = (uint8_t)((uint16_t)g_settings.brightnessPercent * 255 / 100);
  M5.Display.setBrightness(v);
}

// =====================================================================
//  BOOT SPLASH
// =====================================================================

void drawBootSplash() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.fillScreen(g_uiPalette.bg);

  // Square button: same vertical size as the old 64px-tall bar, width = height.
  constexpr int btnSide = 64;
  constexpr int cornerR = 10;
  const int btnX = (w - btnSide) / 2;
  const int btnY = (h - btnSide) / 2;

  M5.Display.fillRoundRect(btnX, btnY, btnSide, btnSide, cornerR, g_uiPalette.splash);
  M5.Display.drawRoundRect(btnX, btnY, btnSide, btnSide, cornerR, TFT_WHITE);

  // Size-1 default font keeps two short lines readable inside a 64x64 hit area.
  M5.Display.setFont(nullptr);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, g_uiPalette.splash);
  M5.Display.setTextDatum(middle_center);
  const int cx = btnX + btnSide / 2;
  const int cy = btnY + btnSide / 2;
  constexpr int linePitch = 10;
  M5.Display.drawString("Hi!", cx, cy - linePitch / 2);
  M5.Display.drawString("Let's play", cx, cy + linePitch / 2);
}

void processBootTouch(uint8_t touchCount) {
  if (touchCount == 0 && wasTouchActive) {
    wasTouchActive = false;
    g_model.rebuildChords();
    g_screen = Screen::Play;
    g_lastAction = "";
    // drawPlaySurface called from loop after state change
    return;
  }
  if (touchCount > 0 && !wasTouchActive) {
    wasTouchActive = true;
  }
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
  const int bz = w / 3;
  const int bezelY = h - kBezelBarH;
  g_bezelBack = {0, bezelY, bz, kBezelBarH};
  g_bezelSelect = {bz, bezelY, bz, kBezelBarH};
  g_bezelFwd = {2 * bz, bezelY, bz, kBezelBarH};
}

void drawBezelBarStrip() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.fillRect(0, h - kBezelBarH, w, kBezelBarH, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("BACK", g_bezelBack.x + g_bezelBack.w / 2,
                        g_bezelBack.y + g_bezelBack.h / 2);
  M5.Display.drawString("SELECT", g_bezelSelect.x + g_bezelSelect.w / 2,
                        g_bezelSelect.y + g_bezelSelect.h / 2);
  M5.Display.drawString("FWD", g_bezelFwd.x + g_bezelFwd.w / 2,
                        g_bezelFwd.y + g_bezelFwd.h / 2);
}

void computePlaySurfaceLayout(int w, int h) {
  const int playH = h - kBezelBarH;
  constexpr int margin = 6;
  const int cell = min((w - 2 * margin) / 3, (playH - 2 * margin) / 3);
  const int gridPx = cell * 3;
  const int ox = (w - gridPx) / 2;
  const int oy = margin + max(0, (playH - 2 * margin - gridPx) / 2);

  g_keyRect = {ox + cell, oy + cell, cell, cell};
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    const int row = kChordSlotRow[i];
    const int col = kChordSlotCol[i];
    g_chordRects[i] = {ox + col * cell, oy + row * cell, cell, cell};
  }

  layoutBottomBezels(w, h);
}

void computeSequencerLayout(int w, int h) {
  layoutBottomBezels(w, h);
  const int playH = h - kBezelBarH;
  constexpr int margin = 4;
  constexpr int titleH = 14;
  constexpr int tabH = 22;
  const int toolY = margin + titleH + 4;
  constexpr int toolH = 22;
  constexpr int toolGap = 4;
  const int toolW = (w - 2 * margin - 3 * toolGap) / 4;
  for (int t = 0; t < 4; ++t) {
    g_seqToolRects[t] = {margin + t * (toolW + toolGap), toolY, toolW, toolH};
  }
  g_seqSliderActive = (g_seqTool != SeqTool::None);
  int tabY = toolY + toolH + 6;
  if (g_seqSliderActive) {
    const int panelY = toolY + toolH + 4;
    constexpr int panelH = 40;
    g_seqSliderPanel = {margin, panelY, w - 2 * margin, panelH};
    tabY = panelY + panelH + 6;
  }
  const int tabGap = 4;
  const int tabW = (w - margin * 2 - tabGap * 2) / 3;
  for (int t = 0; t < 3; ++t) {
    g_seqTabRects[t] = {margin + t * (tabW + tabGap), tabY, tabW, tabH};
  }
  const int gridTop = tabY + tabH + 4;
  const int availH = playH - gridTop - margin;
  const int availW = w - 2 * margin;
  const int cell = max(22, min(availW / 4, availH / 4));
  const int gridW = cell * 4;
  const int ox = (w - gridW) / 2;
  const int oy = gridTop;
  for (int i = 0; i < 16; ++i) {
    const int row = i / 4;
    const int col = i % 4;
    g_seqCellRects[i] = {ox + col * cell, oy + row * cell, cell, cell};
  }
}

int hitTestSeqTab(int px, int py) {
  for (int t = 0; t < 3; ++t) {
    if (pointInRect(px, py, g_seqTabRects[t])) return t;
  }
  return -1;
}

int hitTestSeqTool(int px, int py) {
  for (int t = 0; t < 4; ++t) {
    if (pointInRect(px, py, g_seqToolRects[t])) return t;
  }
  return -1;
}

uint8_t seqLaneClamped() { return g_seqLane > 2 ? 0 : g_seqLane; }

Rect seqSliderTrackRect() {
  const Rect& p = g_seqSliderPanel;
  return {p.x + 8, p.y + 22, max(8, p.w - 16), 12};
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

// fingerChord: -100 = none, -2 = key, 0-7 = chord index (finger-down highlight)
void drawPlaySurface(int fingerChord, bool fingerOnKey) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.fillScreen(g_uiPalette.bg);

  computePlaySurfaceLayout(w, h);

  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("SHIFT+swipe pages  |  SELECT+① voicing", 4, 4);

  const int cell = g_keyRect.w;
  if (g_model.heartAvailable) {
    const uint8_t keyText = (cell >= 56) ? 3 : 2;
    drawRoundedButton(g_keyRect, g_uiPalette.heart, "\x03", keyText);
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
  } else {
    char modeLine[20];
    g_model.formatKeyCenterLine2(modeLine, sizeof(modeLine));
    drawKeyCenterTwoLine(g_keyRect, g_model.keyName(), modeLine);
  }

  const uint8_t chordText = (cell >= 52) ? 2 : 1;
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    if (s_drawPlayVoicingShift && i == 0) {
      char lv[12];
      snprintf(lv, sizeof(lv), "V%u", (unsigned)g_chordVoicing);
      drawRoundedButton(g_chordRects[0], g_uiPalette.accentPress, lv, chordText);
      continue;
    }
    uint16_t bg = colorForRole(g_model.surround[i].role);
    if (i == fingerChord) {
      bg = g_uiPalette.accentPress;
    }
    drawRoundedButton(g_chordRects[i], bg, g_model.surround[i].name, chordText);
  }

  if (fingerChord < -50) {
    if (g_lastPlayedOutline == -2) {
      drawSelectionEdge(g_keyRect);
    } else if (g_lastPlayedOutline >= 0 && g_lastPlayedOutline < ChordModel::kSurroundCount) {
      drawSelectionEdge(g_chordRects[g_lastPlayedOutline]);
    }
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
  M5.Display.fillScreen(g_uiPalette.bg);
  computeSequencerLayout(w, h);

  M5.Display.setFont(nullptr);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Sequencer", w / 2, 2);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("tabs=lane  SELECT+tab=MIDI  row=Qnt Swg Prb Rnd", w / 2, 18);

  const char* toolLabs[4] = {"Qnt", "Swg", "Prb", "Rnd"};
  const SeqTool toolMap[4] = {SeqTool::Quantize, SeqTool::Swing, SeqTool::StepProb,
                              SeqTool::ChordRand};
  for (int t = 0; t < 4; ++t) {
    const bool on = (g_seqTool == toolMap[t]);
    uint16_t bg = on ? g_uiPalette.seqLaneTab : g_uiPalette.panelMuted;
    drawRoundedButton(g_seqToolRects[t], bg, toolLabs[t], 1);
    if (on) {
      M5.Display.drawRoundRect(g_seqToolRects[t].x, g_seqToolRects[t].y, g_seqToolRects[t].w,
                             g_seqToolRects[t].h, max(4, g_seqToolRects[t].h / 8),
                             g_uiPalette.highlightRing);
    }
  }

  if (g_seqSliderActive) {
    M5.Display.fillRoundRect(g_seqSliderPanel.x, g_seqSliderPanel.y, g_seqSliderPanel.w,
                             g_seqSliderPanel.h, 6, g_uiPalette.panelMuted);
    M5.Display.drawRoundRect(g_seqSliderPanel.x, g_seqSliderPanel.y, g_seqSliderPanel.w,
                             g_seqSliderPanel.h, 6, g_uiPalette.settingsBtnBorder);
    const char* title = "";
    switch (g_seqTool) {
      case SeqTool::Quantize:
        title = "Quantize (grid snap)";
        break;
      case SeqTool::Swing:
        title = "Swing";
        break;
      case SeqTool::StepProb:
        title = "Step probability";
        break;
      case SeqTool::ChordRand:
        title = "Chord random (octave spread)";
        break;
      default:
        title = "";
        break;
    }
    M5.Display.setTextDatum(top_center);
    M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
    M5.Display.setTextSize(1);
    M5.Display.drawString(title, g_seqSliderPanel.x + g_seqSliderPanel.w / 2,
                          g_seqSliderPanel.y + 4);
    char sub[40];
    if (g_seqTool == SeqTool::StepProb) {
      if (g_seqProbFocusStep >= 0) {
        snprintf(sub, sizeof(sub), "step %u  (tap another step)", (unsigned)g_seqProbFocusStep + 1);
      } else {
        snprintf(sub, sizeof(sub), "tap step or slide for all");
      }
      M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
      M5.Display.drawString(sub, g_seqSliderPanel.x + g_seqSliderPanel.w / 2,
                            g_seqSliderPanel.y + 14);
    }
    const Rect tr = seqSliderTrackRect();
    M5.Display.fillRoundRect(tr.x, tr.y, tr.w, tr.h, 4, g_uiPalette.bg);
    const uint8_t sv = seqSliderDisplayedValue();
    const int fillW = max(0, (tr.w * static_cast<int>(sv)) / 100);
    if (fillW > 0) {
      M5.Display.fillRoundRect(tr.x, tr.y, fillW, tr.h, 4, g_uiPalette.seqLaneTab);
    }
    M5.Display.drawRoundRect(tr.x, tr.y, tr.w, tr.h, 4, g_uiPalette.settingsBtnBorder);
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", (unsigned)sv);
    M5.Display.setTextDatum(middle_right);
    M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.panelMuted);
    M5.Display.drawString(pct, g_seqSliderPanel.x + g_seqSliderPanel.w - 6,
                          tr.y + tr.h / 2);
  }

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
    const Rect& r = g_seqCellRects[i];
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

  drawBezelBarStrip();
}

void computeXyLayout(int w, int h) {
  layoutBottomBezels(w, h);
  const int playH = h - kBezelBarH;
  constexpr int margin = 8;
  constexpr int topBlock = 58;
  const int padTop = topBlock;
  const int padH = playH - padTop - margin;
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

void drawXyPadSurface() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.fillScreen(g_uiPalette.bg);
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
  M5.Display.drawString("BACK/FWD = Play / Seq / XY", w / 2, 34);
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
  M5.Display.setTextDatum(bottom_center);
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
  M5.Display.drawString(vals, w / 2, pr.y + pr.h - 4);

  drawBezelBarStrip();
}

void beforeLeaveSequencer() {
  if (g_screen == Screen::Sequencer) {
    seqPatternSave(g_seqPattern);
    seqExtrasSave(&g_seqExtras);
  }
}

bool tryEnterSettingsTwoFingerLong(uint8_t touchCount, int w, int h);

void navigateMainRing(int direction) {
  static const Screen kRing[] = {Screen::Play, Screen::Sequencer, Screen::XyPad};
  beforeLeaveSequencer();
  Screen ringScreen = g_screen;
  if (ringScreen == Screen::Transport) {
    ringScreen = Screen::Play;
  }
  int idx = 0;
  for (int i = 0; i < 3; ++i) {
    if (kRing[i] == ringScreen) {
      idx = i;
      break;
    }
  }
  idx = (idx + direction + 300) % 3;
  g_screen = kRing[idx];
  s_xyMidiSentX = 255;
  s_xyMidiSentY = 255;
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
    drawXyPadSurface();
    return;
  }

  const uint8_t midiCh = static_cast<uint8_t>(g_settings.midiOutChannel - 1);

  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    if (pointInRect(d.x, d.y, g_bezelBack) || pointInRect(d.x, d.y, g_bezelSelect) ||
        pointInRect(d.x, d.y, g_bezelFwd)) {
      drawXyPadSurface();
    } else if (hitTestXyPad(d.x, d.y)) {
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
      drawXyPadSurface();
    } else {
      drawXyPadSurface();
    }
    wasTouchActive = true;
    return;
  }

  if (touchCount == 0) {
    if (!wasTouchActive) return;
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
    g_settingsRow = 0;
    g_factoryResetConfirmArmed = false;
    return true;
  }
  return false;
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

  if (touchCount >= 2) {
    bool sel = false;
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (!d.isPressed()) continue;
      if (pointInRect(d.x, d.y, g_bezelSelect)) sel = true;
    }
    s_seqComboTab = -1;
    if (sel) {
      for (uint8_t i = 0; i < touchCount; ++i) {
        const auto& d = M5.Touch.getDetail(i);
        if (!d.isPressed()) continue;
        const int th = hitTestSeqTab(d.x, d.y);
        if (th >= 0) {
          s_seqComboTab = th;
          break;
        }
      }
    }
    wasTouchActive = true;
    drawSequencerSurface(-1);
    return;
  }

  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    if (g_seqTool != SeqTool::None && g_seqSliderActive &&
        pointInRect(d.x, d.y, g_seqSliderPanel)) {
      g_seqDraggingSlider = true;
      seqSetSliderValueFromX(d.x);
      drawSequencerSurface();
      wasTouchActive = true;
      return;
    }
    const int ht = hitTestSeq(d.x, d.y);
    if (pointInRect(d.x, d.y, g_bezelBack) || pointInRect(d.x, d.y, g_bezelSelect) ||
        pointInRect(d.x, d.y, g_bezelFwd)) {
      drawSequencerSurface();
    } else if (ht >= 0) {
      drawSequencerSurface(ht);
    } else {
      drawSequencerSurface();
    }
    wasTouchActive = true;
    return;
  }

  if (touchCount == 0) {
    if (!wasTouchActive) return;
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
    if (pointInRect(hx, hy, g_bezelBack)) {
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      navigateMainRing(-1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      navigateMainRing(1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      drawSequencerSurface();
      return;
    }

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
    s_seqGestureMaxTouches = 0;
    s_seqComboTab = -1;

    const int toolHit = hitTestSeqTool(hx, hy);
    if (toolHit >= 0) {
      static const SeqTool kMap[4] = {SeqTool::Quantize, SeqTool::Swing, SeqTool::StepProb,
                                      SeqTool::ChordRand};
      const SeqTool tapped = kMap[toolHit];
      if (g_seqTool == tapped) {
        g_seqTool = SeqTool::None;
        g_seqProbFocusStep = -1;
      } else {
        g_seqTool = tapped;
        if (tapped != SeqTool::StepProb) {
          g_seqProbFocusStep = -1;
        }
      }
      seqExtrasSave(&g_seqExtras);
      drawSequencerSurface();
      return;
    }

    const int tabHit = hitTestSeqTab(hx, hy);
    if (tabHit >= 0) {
      g_seqLane = static_cast<uint8_t>(tabHit);
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
      seqCycleStep(cell);
      drawSequencerSurface();
      return;
    }
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
  if (keyDown && selectDown) {
    g_comboKeyPickerLatch = true;
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
    drawPlaySurface(ch, fk);
    return;
  }

  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    const int ht = hitTestPlay(d.x, d.y);
    if (pointInRect(d.x, d.y, g_bezelBack) || pointInRect(d.x, d.y, g_bezelSelect) ||
        pointInRect(d.x, d.y, g_bezelFwd)) {
      drawPlaySurface();
    } else if (ht == -2) {
      drawPlaySurface(-100, true);
    } else if (ht >= 0) {
      drawPlaySurface(ht, false);
    } else {
      drawPlaySurface();
    }
    wasTouchActive = true;
    return;
  }

  if (touchCount == 0) {
    if (!wasTouchActive) return;
    wasTouchActive = false;
    if (suppressNextPlayTap) {
      suppressNextPlayTap = false;
      g_comboKeyPickerLatch = false;
      drawPlaySurface();
      return;
    }

    const bool openKeyPicker = g_comboKeyPickerLatch;
    g_comboKeyPickerLatch = false;

    if (openKeyPicker) {
      g_pickTonic = g_model.keyIndex;
      g_pickMode = g_model.mode;
      g_screen = Screen::KeyPicker;
      drawKeyPicker();
      s_playGestureMaxTouches = 0;
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
      navigateMainRing(-1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      navigateMainRing(1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      drawPlaySurface();
      return;
    }

    const int hit = hitTestPlay(hx, hy);
    if (hit == -2) {
      if (g_model.heartAvailable) {
        g_model.nextSurprise();
        g_model.consumeHeart();
        g_lastPlayedOutline = -2;
      } else {
        g_lastPlayedOutline = -2;
        g_model.registerPlay();
      }
      transportSetLiveChord(-1);
      drawPlaySurface();
      return;
    }
    if (hit >= 0 && hit < ChordModel::kSurroundCount) {
      g_lastPlayedOutline = hit;
      g_model.registerPlay();
      transportSetLiveChord(static_cast<int8_t>(hit));
      drawPlaySurface();
      return;
    }
    drawPlaySurface();
  }
}

// =====================================================================
//  KEY + MODE PICKER (full-screen “dropdown”)
// =====================================================================

void drawKeyPicker() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
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
  M5.Display.fillScreen(g_uiPalette.bg);
  layoutBottomBezels(w, h);

  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Transport", w / 2, 4);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("SELECT+swipe: up / down / left / right", w / 2, 22);

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
    drawTransportSurface();
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

void drawButton(int x, int y, int w, int h, const char* label, bool active) {
  uint16_t bg = active ? g_uiPalette.settingsBtnActive : g_uiPalette.settingsBtnIdle;
  M5.Display.fillRoundRect(x, y, w, h, 10, bg);
  M5.Display.drawRoundRect(x, y, w, h, 10, g_uiPalette.settingsBtnBorder);
  M5.Display.setTextColor(TFT_WHITE, bg);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(label, x + w / 2, y + h / 2);
}

void drawSettingsUi(int activeZone) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.fillScreen(g_uiPalette.bg);

  M5.Display.setTextColor(g_uiPalette.settingsHeader, g_uiPalette.bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Settings", w / 2, 6);
  M5.Display.setTextSize(1);

  const int rowY0 = 16;
  const int rowH = 12;

  auto rowColor = [&](int row) {
    return row == g_settingsRow ? g_uiPalette.rowSelect : g_uiPalette.rowNormal;
  };

  char line[56];
  snprintf(line, sizeof(line), " MIDI out channel:  %u",
           (unsigned)g_settings.midiOutChannel);
  M5.Display.setTextColor(rowColor(0), g_uiPalette.bg);
  M5.Display.setTextDatum(middle_left);
  M5.Display.drawString(line, 8, rowY0 + rowH * 0);

  if (g_settings.midiInChannel == 0) {
    snprintf(line, sizeof(line), " MIDI in:  OMNI (all)");
  } else {
    snprintf(line, sizeof(line), " MIDI in channel:  %u",
             (unsigned)g_settings.midiInChannel);
  }
  M5.Display.setTextColor(rowColor(1), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 1);

  snprintf(line, sizeof(line), " MIDI transport send:  %s",
           midiTransportRouteLabel(g_settings.midiTransportSend));
  M5.Display.setTextColor(rowColor(2), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 2);

  snprintf(line, sizeof(line), " MIDI transport receive:  %s",
           midiTransportRouteLabel(g_settings.midiTransportReceive));
  M5.Display.setTextColor(rowColor(3), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 3);

  snprintf(line, sizeof(line), " MIDI clock:  %s",
           midiClockSourceLabel(g_settings.midiClockSource));
  M5.Display.setTextColor(rowColor(4), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 4);

  snprintf(line, sizeof(line), " Brightness:  %u%%",
           (unsigned)g_settings.brightnessPercent);
  M5.Display.setTextColor(rowColor(5), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 5);

  snprintf(line, sizeof(line), " Note velocity:  %u",
           (unsigned)g_settings.outputVelocity);
  M5.Display.setTextColor(rowColor(6), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 6);

  snprintf(line, sizeof(line), " Build: %s", M5CHORD_BUILD_STAMP);
  M5.Display.setTextColor(rowColor(7), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 7);

  snprintf(line, sizeof(line), " Color theme:  %s", uiThemeName(g_uiTheme));
  M5.Display.setTextColor(rowColor(8), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 8);

  snprintf(line, sizeof(line), " Click volume:  %u%%", (unsigned)g_clickVolumePercent);
  M5.Display.setTextColor(rowColor(9), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 9);

  snprintf(line, sizeof(line), " Arpeggiator:  %s",
           arpeggiatorModeLabel(g_settings.arpeggiatorMode));
  M5.Display.setTextColor(rowColor(10), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 10);

  snprintf(line, sizeof(line), " BPM (project):  %u", (unsigned)g_projectBpm);
  M5.Display.setTextColor(rowColor(11), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 11);

  if (g_projectCustomName[0] != '\0') {
    snprintf(line, sizeof(line), " Project folder:  %.18s", g_projectCustomName);
  } else {
    snprintf(line, sizeof(line), " Project folder:  (auto)");
  }
  M5.Display.setTextColor(rowColor(12), g_uiPalette.bg);
  M5.Display.drawString(line, 8, rowY0 + rowH * 12);

  if (g_factoryResetConfirmArmed && g_settingsRow == 13) {
    M5.Display.setTextColor(g_uiPalette.danger, g_uiPalette.bg);
    M5.Display.drawString(" ! Factory reset  SELECT again", 8, rowY0 + rowH * 13);
  } else {
    M5.Display.setTextColor(rowColor(13), g_uiPalette.bg);
    M5.Display.drawString(" Factory reset", 8, rowY0 + rowH * 13);
  }

  M5.Display.setTextColor(rowColor(14), g_uiPalette.bg);
  M5.Display.drawString(" Backup SD (global+project)", 8, rowY0 + rowH * 14);

  M5.Display.setTextColor(rowColor(15), g_uiPalette.bg);
  M5.Display.drawString(" Restore SD (global+project)", 8, rowY0 + rowH * 15);

  M5.Display.setTextColor(rowColor(16), g_uiPalette.bg);
  M5.Display.drawString(" Save & exit", 8, rowY0 + rowH * 16);

  if (g_settingsFeedback[0] != '\0') {
    M5.Display.setTextDatum(top_center);
    M5.Display.setTextColor(g_uiPalette.feedback, g_uiPalette.bg);
    M5.Display.drawString(g_settingsFeedback, w / 2, rowY0 + rowH * 17);
  }

  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.drawString("BACK/FWD = row  SELECT = change", w / 2,
                          rowY0 + rowH * 17 + 10);

  const int buttonBandHeight = h / 5;
  const int btnY = h - buttonBandHeight + 8;
  const int btnH = buttonBandHeight - 16;
  const int zoneW = w / 3;
  drawButton(6, btnY, zoneW - 12, btnH, "BACK", activeZone == 0);
  drawButton(zoneW + 6, btnY, zoneW - 12, btnH, "SELECT", activeZone == 1);
  drawButton(zoneW * 2 + 6, btnY, zoneW - 12, btnH, "FWD", activeZone == 2);
  M5.Display.setTextSize(2);
}

void settingsMoveRow(int delta) {
  const int n = static_cast<int>(AppSettings::kRowCount);
  int r = g_settingsRow + delta;
  r %= n;
  if (r < 0) r += n;
  g_settingsRow = r;
  g_factoryResetConfirmArmed = false;
  g_settingsFeedback[0] = '\0';
  drawSettingsUi();
}

void settingsApplySelect() {
  const int row = g_settingsRow;
  if (row != 13) {
    g_factoryResetConfirmArmed = false;
  }

  switch (row) {
    case 0:
      g_settings.cycleMidiOut();
      break;
    case 1:
      g_settings.cycleMidiIn();
      break;
    case 2:
      g_settings.cycleMidiTransportSend();
      settingsSave(g_settings);
      break;
    case 3:
      g_settings.cycleMidiTransportReceive();
      settingsSave(g_settings);
      break;
    case 4:
      g_settings.cycleMidiClockSource();
      settingsSave(g_settings);
      break;
    case 5:
      g_settings.cycleBrightness();
      applyBrightness();
      break;
    case 6:
      g_settings.cycleVelocity();
      break;
    case 7:
      break;
    case 8: {
      unsigned next = static_cast<unsigned>(g_uiTheme) + 1U;
      if (next >= static_cast<unsigned>(kUiThemeCount)) {
        next = 0;
      }
      g_uiTheme = static_cast<uint8_t>(next);
      uiThemeSave(g_uiTheme);
      uiThemeApply(g_uiTheme);
      break;
    }
    case 9: {
      uint16_t v = static_cast<uint16_t>(g_clickVolumePercent) + 10U;
      if (v > 100U) {
        v = 0;
      }
      g_clickVolumePercent = static_cast<uint8_t>(v);
      transportApplyClickVolume();
      transportPrefsSave();
      break;
    }
    case 10:
      g_settings.cycleArpeggiatorMode();
      settingsSave(g_settings);
      break;
    case 11:
      g_projectBpm += 5;
      if (g_projectBpm > 200) {
        g_projectBpm = 40;
      }
      projectBpmSave(g_projectBpm);
      break;
    case 12:
      openProjectNameEditor();
      return;
    case 13:
      if (g_factoryResetConfirmArmed) {
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
      } else {
        g_factoryResetConfirmArmed = true;
      }
      drawSettingsUi();
      return;
    case 14: {
      char folder[48];
      resolveBackupFolder(folder);
      const uint16_t bm = g_projectBpm;
      if (sdBackupWriteAll(g_settings, g_model, g_seqPattern, g_seqMidiCh, g_xyCcA, g_xyCcB, bm,
                           g_chordVoicing, &g_seqExtras, folder)) {
        lastProjectFolderSave(folder);
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "OK %s/%s", SD_BACKUP_ROOT,
                 folder);
      } else {
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SD backup failed");
      }
      drawSettingsUi();
      return;
    }
    case 15: {
      char dirs[8][48];
      int n = 0;
      if (!sdBackupListProjects(dirs, 8, &n)) {
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SD list failed");
        drawSettingsUi();
        return;
      }
      if (n == 0) {
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "No projects in " SD_BACKUP_ROOT);
        drawSettingsUi();
        return;
      }
      if (n == 1) {
        applySdRestoreFromFolder(dirs[0]);
        drawSettingsUi();
        return;
      }
      g_sdPickCount = n;
      for (int i = 0; i < n && i < 8; ++i) {
        strncpy(g_sdPickNames[i], dirs[i], 47);
        g_sdPickNames[i][47] = '\0';
      }
      g_screen = Screen::SdProjectPick;
      drawSdProjectPick();
      return;
    }
    case 16:
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
      g_factoryResetConfirmArmed = false;
      g_settingsFeedback[0] = '\0';
      drawPlaySurface();
      return;
    default:
      break;
  }
  g_settings.normalize();
  drawSettingsUi();
}

void processSettingsTouch(uint8_t touchCount, int w, int h) {
  if (touchCount == 0) {
    if (wasTouchActive) {
      wasTouchActive = false;
      auto last = M5.Touch.getDetail();
      int zone = buttonZoneForPoint(last.x, last.y, w, h);
      if (zone == 0) {
        settingsMoveRow(-1);
      } else if (zone == 2) {
        settingsMoveRow(1);
      } else if (zone == 1) {
        settingsApplySelect();
      } else {
        drawSettingsUi();
      }
    }
    return;
  }

  const auto& d = M5.Touch.getDetail(0);
  int zone = buttonZoneForPoint(d.x, d.y, w, h);
  drawSettingsUi(zone);
  if (!wasTouchActive) {
    wasTouchActive = true;
    touchStartMs = millis();
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
  drawBootSplash();
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

  if (pollShiftSwipeNavigate(w, h)) {
    delay(10);
    return;
  }

  switch (g_screen) {
    case Screen::Boot:
      processBootTouch(touchCount);
      break;
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
