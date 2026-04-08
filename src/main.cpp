#include <M5Unified.h>

#include "AppSettings.h"
#include "ChordModel.h"
#include "SettingsStore.h"

namespace {

// --- Color palette (RGB565) ---
constexpr uint16_t kColorPrincipal = 0x0660;   // green  (#00C853)
constexpr uint16_t kColorStandard  = 0x05BF;   // blue   (#00B0FF)
constexpr uint16_t kColorTension   = 0xF8A8;   // red    (#FF1744)
constexpr uint16_t kColorSurprise  = 0xFA10;   // pink   (#FF4081)
constexpr uint16_t kColorKeyCenterBg = 0x2B1F;  // bright blue (#2962FF)
constexpr uint16_t kColorHeartBg   = 0xFA10;    // pink
constexpr uint16_t kColorSplashBg  = 0x2B1F;    // bright blue

uint16_t colorForRole(ChordRole role) {
  switch (role) {
    case ChordRole::Principal: return kColorPrincipal;
    case ChordRole::Standard:  return kColorStandard;
    case ChordRole::Tension:   return kColorTension;
    case ChordRole::Surprise:  return kColorSurprise;
  }
  return TFT_DARKGREY;
}

// --- State ---
enum class Screen : uint8_t { Boot, Play, Settings };

ChordModel g_model;
AppSettings g_settings;
Screen g_screen = Screen::Boot;

String g_lastAction = "";

bool wasTouchActive = false;
uint32_t touchStartMs = 0;

uint32_t dualCornerHoldStartMs = 0;
bool dualGestureNeedRelease = false;
bool suppressNextPlayTap = false;

int g_settingsRow = 0;

constexpr uint32_t kDualCornerHoldMs = 800;

// Play surface hit regions (computed once per draw)
struct Rect {
  int x, y, w, h;
};
Rect g_keyRect;
Rect g_chordRects[ChordModel::kSurroundCount];

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

bool touchCoversLeftAndRightCorners(uint8_t touchCount, int w, int h) {
  if (touchCount < 2) return false;
  bool left = false, right = false;
  for (uint8_t i = 0; i < touchCount; ++i) {
    const auto& d = M5.Touch.getDetail(i);
    if (!d.isPressed()) continue;
    int z = buttonZoneForPoint(d.x, d.y, w, h);
    if (z == 0) left = true;
    if (z == 2) right = true;
  }
  return left && right;
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
  M5.Display.fillScreen(TFT_BLACK);

  // Square button: same vertical size as the old 64px-tall bar, width = height.
  constexpr int btnSide = 64;
  constexpr int cornerR = 10;
  const int btnX = (w - btnSide) / 2;
  const int btnY = (h - btnSide) / 2;

  M5.Display.fillRoundRect(btnX, btnY, btnSide, btnSide, cornerR, kColorSplashBg);
  M5.Display.drawRoundRect(btnX, btnY, btnSide, btnSide, cornerR, TFT_WHITE);

  // Size-1 default font keeps two short lines readable inside a 64x64 hit area.
  M5.Display.setFont(nullptr);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, kColorSplashBg);
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
  M5.Display.fillRoundRect(r.x, r.y, r.w, r.h, 8, bg);
  M5.Display.drawRoundRect(r.x, r.y, r.w, r.h, 8,
                            (uint16_t)(bg == TFT_BLACK ? TFT_DARKGREY : TFT_WHITE));
  M5.Display.setTextColor(TFT_WHITE, bg);
  M5.Display.setTextSize(textSize);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
}

void drawPlaySurface(int highlightIdx = -1) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.fillScreen(TFT_BLACK);

  // --- Key center button (top area) ---
  const int keyCenterW = 120;
  const int keyCenterH = 50;
  const int keyCenterX = (w - keyCenterW) / 2;
  const int keyCenterY = 8;
  g_keyRect = {keyCenterX, keyCenterY, keyCenterW, keyCenterH};

  if (g_model.heartAvailable) {
    drawRoundedButton(g_keyRect, kColorHeartBg, "\x03", 3);  // heart char
  } else {
    drawRoundedButton(g_keyRect, kColorKeyCenterBg, g_model.keyName(), 3);
  }

  // Small label above key
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.setTextDatum(bottom_center);
  M5.Display.drawString(g_model.heartAvailable ? "Surprise!" : "Key", w / 2,
                          keyCenterY - 1);

  // --- 6 chord buttons: 3 columns x 2 rows ---
  constexpr int cols = 3;
  constexpr int rows = 2;
  const int gridTop = keyCenterY + keyCenterH + 14;
  const int gridBottom = h - 36;
  const int gridH = gridBottom - gridTop;
  const int gap = 6;
  const int btnW = (w - gap * (cols + 1)) / cols;
  const int btnH = (gridH - gap * (rows - 1)) / rows;

  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    int col = i % cols;
    int row = i / cols;
    int bx = gap + col * (btnW + gap);
    int by = gridTop + row * (btnH + gap);
    g_chordRects[i] = {bx, by, btnW, btnH};

    uint16_t bg = colorForRole(g_model.surround[i].role);
    if (i == highlightIdx) {
      bg = TFT_ORANGE;
    }
    drawRoundedButton(g_chordRects[i], bg, g_model.surround[i].name, 2);
  }

  // --- Status bar ---
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.setTextDatum(bottom_center);
  if (g_lastAction.length() > 0) {
    M5.Display.drawString(g_lastAction.c_str(), w / 2, h - 4);
  }
}

int hitTestPlay(int px, int py) {
  if (pointInRect(px, py, g_keyRect)) return -2;  // key center
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    if (pointInRect(px, py, g_chordRects[i])) return i;
  }
  return -1;  // nothing
}

bool tryEnterSettingsTwoFingerLong(uint8_t touchCount, int w, int h) {
  if (dualGestureNeedRelease) {
    if (touchCount == 0) dualGestureNeedRelease = false;
    return false;
  }
  uint32_t now = millis();
  if (touchCoversLeftAndRightCorners(touchCount, w, h)) {
    if (dualCornerHoldStartMs == 0) dualCornerHoldStartMs = now;
    if (now - dualCornerHoldStartMs >= kDualCornerHoldMs) {
      dualCornerHoldStartMs = 0;
      dualGestureNeedRelease = true;
      suppressNextPlayTap = true;
      g_screen = Screen::Settings;
      g_settingsRow = 0;
      return true;
    }
  } else {
    dualCornerHoldStartMs = 0;
  }
  return false;
}

void processPlayTouch(uint8_t touchCount, int w, int h) {
  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    // settings draw happens from loop
    return;
  }

  if (touchCount >= 2) {
    wasTouchActive = false;
    return;
  }

  if (touchCount == 0) {
    if (wasTouchActive) {
      wasTouchActive = false;
      if (suppressNextPlayTap) {
        suppressNextPlayTap = false;
        drawPlaySurface();
        return;
      }
      auto last = M5.Touch.getDetail();
      int hit = hitTestPlay(last.x, last.y);
      if (hit == -2) {
        // Key center tapped
        if (g_model.heartAvailable) {
          const ChordInfo& sc = g_model.nextSurprise();
          g_lastAction = String("Surprise: ") + sc.name;
          g_model.consumeHeart();
        } else {
          g_model.cycleKeyForward();
          g_lastAction = String("Key: ") + g_model.keyName();
        }
        drawPlaySurface();
      } else if (hit >= 0 && hit < ChordModel::kSurroundCount) {
        g_model.registerPlay();
        g_lastAction = g_model.surround[hit].name;
        drawPlaySurface(hit);
      } else {
        drawPlaySurface();
      }
    }
    return;
  }

  // Finger down — highlight
  const auto& d = M5.Touch.getDetail(0);
  int hit = hitTestPlay(d.x, d.y);
  if (hit >= 0) {
    drawPlaySurface(hit);
  }
  if (!wasTouchActive) {
    wasTouchActive = true;
    touchStartMs = millis();
  }
}

// =====================================================================
//  SETTINGS (preserved from previous implementation)
// =====================================================================

void drawButton(int x, int y, int w, int h, const char* label, bool active) {
  uint16_t bg = active ? TFT_ORANGE : TFT_DARKGREY;
  M5.Display.fillRoundRect(x, y, w, h, 10, bg);
  M5.Display.drawRoundRect(x, y, w, h, 10, TFT_LIGHTGREY);
  M5.Display.setTextColor(TFT_WHITE, bg);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(label, x + w / 2, y + h / 2);
}

void drawSettingsUi(int activeZone = -1) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.fillScreen(TFT_BLACK);

  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Settings", w / 2, 6);
  M5.Display.setTextSize(1);

  const int rowY0 = 38;
  const int rowH = 22;

  auto rowColor = [&](int row) {
    return row == g_settingsRow ? TFT_YELLOW : TFT_WHITE;
  };

  char line[56];
  snprintf(line, sizeof(line), " MIDI out channel:  %u",
           (unsigned)g_settings.midiOutChannel);
  M5.Display.setTextColor(rowColor(0), TFT_BLACK);
  M5.Display.setTextDatum(middle_left);
  M5.Display.drawString(line, 8, rowY0 + rowH * 0);

  if (g_settings.midiInChannel == 0) {
    snprintf(line, sizeof(line), " MIDI in:  OMNI (all)");
  } else {
    snprintf(line, sizeof(line), " MIDI in channel:  %u",
             (unsigned)g_settings.midiInChannel);
  }
  M5.Display.setTextColor(rowColor(1), TFT_BLACK);
  M5.Display.drawString(line, 8, rowY0 + rowH * 1);

  snprintf(line, sizeof(line), " Brightness:  %u%%",
           (unsigned)g_settings.brightnessPercent);
  M5.Display.setTextColor(rowColor(2), TFT_BLACK);
  M5.Display.drawString(line, 8, rowY0 + rowH * 2);

  snprintf(line, sizeof(line), " Note velocity:  %u",
           (unsigned)g_settings.outputVelocity);
  M5.Display.setTextColor(rowColor(3), TFT_BLACK);
  M5.Display.drawString(line, 8, rowY0 + rowH * 3);

  M5.Display.setTextColor(rowColor(4), TFT_BLACK);
  M5.Display.drawString(" Save & exit", 8, rowY0 + rowH * 4);

  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.drawString("BACK/FWD = row  SELECT = change", w / 2,
                          rowY0 + rowH * 5 + 6);

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
  drawSettingsUi();
}

void settingsApplySelect() {
  switch (g_settingsRow) {
    case 0:
      g_settings.cycleMidiOut();
      break;
    case 1:
      g_settings.cycleMidiIn();
      break;
    case 2:
      g_settings.cycleBrightness();
      applyBrightness();
      break;
    case 3:
      g_settings.cycleVelocity();
      break;
    case 4:
      g_settings.normalize();
      settingsSave(g_settings);
      g_screen = Screen::Play;
      g_lastAction = "Settings saved";
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

  g_model.rebuildChords();
  drawBootSplash();
}

void loop() {
  M5.update();

  const int w = M5.Display.width();
  const int h = M5.Display.height();
  const uint8_t touchCount = M5.Touch.getCount();

  switch (g_screen) {
    case Screen::Boot:
      processBootTouch(touchCount);
      break;
    case Screen::Play:
      processPlayTouch(touchCount, w, h);
      break;
    case Screen::Settings:
      processSettingsTouch(touchCount, w, h);
      break;
  }

  delay(10);
}
