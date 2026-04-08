#include <M5Unified.h>

#include "AppSettings.h"
#include "ChordModel.h"
#include "SettingsStore.h"

namespace {

enum class Screen : uint8_t { Main, Settings };

ChordModel g_model;
AppSettings g_settings;
Screen g_screen = Screen::Main;

String g_lastAction = "Ready";

bool wasTouchActive = false;
uint32_t touchStartMs = 0;

uint32_t dualCornerHoldStartMs = 0;
bool dualGestureNeedRelease = false;
bool suppressNextMainTap = false;

int g_settingsRow = 0;

constexpr uint32_t kDualCornerHoldMs = 800;
constexpr uint32_t kLongCenterMs = 700;

int buttonZoneForPoint(int x, int y, int w, int h) {
  const int buttonBandHeight = h / 5;
  const int top = h - buttonBandHeight;
  if (y < top) return -1;
  const int zoneW = w / 3;
  if (x < zoneW) return 0;
  if (x < zoneW * 2) return 1;
  return 2;
}

bool touchCoversLeftAndRightCorners(uint8_t touchCount, int w, int h) {
  if (touchCount < 2) return false;
  bool left = false;
  bool right = false;
  for (uint8_t i = 0; i < touchCount; ++i) {
    const auto& d = M5.Touch.getDetail(i);
    if (!d.isPressed()) continue;
    const int z = buttonZoneForPoint(d.x, d.y, w, h);
    if (z == 0) left = true;
    if (z == 2) right = true;
  }
  return left && right;
}

void applyBrightness() {
  const uint8_t v =
      (uint8_t)((uint16_t)g_settings.brightnessPercent * 255 / 100);
  M5.Display.setBrightness(v);
}

void drawButton(int x, int y, int w, int h, const char* label, bool active) {
  const uint16_t bg = active ? TFT_ORANGE : TFT_DARKGREY;
  const uint16_t fg = TFT_WHITE;
  M5.Display.fillRoundRect(x, y, w, h, 10, bg);
  M5.Display.drawRoundRect(x, y, w, h, 10, TFT_LIGHTGREY);
  M5.Display.setTextColor(fg, bg);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(label, x + w / 2, y + h / 2);
}

void drawMainUi(int activeZone = -1) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.fillScreen(TFT_BLACK);

  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setTextDatum(top_center);
  M5.Display.drawString("M5 CoreS3 Chord Suggester", w / 2, 8);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
  char hdr[48];
  snprintf(hdr, sizeof(hdr), "Out CH%02u  In %s  Vel %u",
           (unsigned)g_settings.midiOutChannel,
           g_settings.midiInChannel == 0 ? "OMNI" : "CH",
           (unsigned)g_settings.outputVelocity);
  if (g_settings.midiInChannel != 0) {
    snprintf(hdr, sizeof(hdr), "Out CH%02u  In CH%02u  Vel %u",
             (unsigned)g_settings.midiOutChannel,
             (unsigned)g_settings.midiInChannel,
             (unsigned)g_settings.outputVelocity);
  }
  M5.Display.drawString(hdr, w / 2, 30);
  M5.Display.setTextSize(2);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString("Selected", w / 2, 52);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Display.drawString(g_model.selectedChord(), w / 2, 82);

  M5.Display.setTextColor(TFT_SILVER, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.drawString(g_lastAction, w / 2, 118);
  M5.Display.setTextSize(2);

  const int buttonBandHeight = h / 5;
  const int btnY = h - buttonBandHeight + 8;
  const int btnH = buttonBandHeight - 16;
  const int zoneW = w / 3;
  drawButton(6, btnY, zoneW - 12, btnH, "BACK", activeZone == 0);
  drawButton(zoneW + 6, btnY, zoneW - 12, btnH, "SELECT", activeZone == 1);
  drawButton(zoneW * 2 + 6, btnY, zoneW - 12, btnH, "FWD", activeZone == 2);
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
  M5.Display.drawString("BACK/FWD = row  SELECT = change", w / 2, rowY0 + rowH * 5 + 6);

  const int buttonBandHeight = h / 5;
  const int btnY = h - buttonBandHeight + 8;
  const int btnH = buttonBandHeight - 16;
  const int zoneW = w / 3;
  drawButton(6, btnY, zoneW - 12, btnH, "BACK", activeZone == 0);
  drawButton(zoneW + 6, btnY, zoneW - 12, btnH, "SELECT", activeZone == 1);
  drawButton(zoneW * 2 + 6, btnY, zoneW - 12, btnH, "FWD", activeZone == 2);
  M5.Display.setTextSize(2);
}

void handleMainTap(int zone) {
  if (zone == 0) {
    g_model.moveUp();
    g_lastAction = "BACK";
  } else if (zone == 1) {
    g_lastAction = String("SELECT: ") + g_model.selectedChord();
  } else if (zone == 2) {
    g_model.moveDown();
    g_lastAction = "FWD";
  } else {
    return;
  }
  drawMainUi(zone);
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
      g_screen = Screen::Main;
      g_lastAction = "Settings saved";
      drawMainUi();
      return;
    default:
      break;
  }
  g_settings.normalize();
  drawSettingsUi();
}

bool tryEnterSettingsTwoFingerLong(uint8_t touchCount, int w, int h) {
  if (dualGestureNeedRelease) {
    if (touchCount == 0) dualGestureNeedRelease = false;
    return false;
  }

  const uint32_t now = millis();
  if (touchCoversLeftAndRightCorners(touchCount, w, h)) {
    if (dualCornerHoldStartMs == 0) dualCornerHoldStartMs = now;
    if (now - dualCornerHoldStartMs >= kDualCornerHoldMs) {
      dualCornerHoldStartMs = 0;
      dualGestureNeedRelease = true;
      suppressNextMainTap = true;
      g_screen = Screen::Settings;
      g_settingsRow = 0;
      drawSettingsUi();
      return true;
    }
  } else {
    dualCornerHoldStartMs = 0;
  }
  return false;
}

void processSettingsTouch(uint8_t touchCount, int w, int h) {
  if (touchCount == 0) {
    if (wasTouchActive) {
      wasTouchActive = false;
      auto last = M5.Touch.getDetail();
      const int zone = buttonZoneForPoint(last.x, last.y, w, h);
      const uint32_t heldMs = millis() - touchStartMs;
      if (zone == 0) {
        settingsMoveRow(-1);
      } else if (zone == 2) {
        settingsMoveRow(1);
      } else if (zone == 1) {
        if (heldMs > kLongCenterMs) {
          drawSettingsUi(zone);
        } else {
          settingsApplySelect();
        }
      } else {
        drawSettingsUi();
      }
    }
    return;
  }

  const auto& d = M5.Touch.getDetail(0);
  const int zone = buttonZoneForPoint(d.x, d.y, w, h);
  drawSettingsUi(zone);
  if (!wasTouchActive) {
    wasTouchActive = true;
    touchStartMs = millis();
  }
}

void processMainTouch(uint8_t touchCount, int w, int h) {
  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount >= 2) {
    wasTouchActive = false;
    drawMainUi(-1);
    return;
  }

  if (touchCount == 0) {
    if (wasTouchActive) {
      wasTouchActive = false;
      if (suppressNextMainTap) {
        suppressNextMainTap = false;
        drawMainUi();
        return;
      }
      auto last = M5.Touch.getDetail();
      const int zone = buttonZoneForPoint(last.x, last.y, w, h);
      const uint32_t heldMs = millis() - touchStartMs;
      if (zone == 1 && heldMs > kLongCenterMs) {
        g_lastAction = String("LONG SELECT: ") + g_model.selectedChord();
        drawMainUi(zone);
      } else {
        handleMainTap(zone);
      }
    }
    return;
  }

  const auto& d = M5.Touch.getDetail(0);
  const int zone = buttonZoneForPoint(d.x, d.y, w, h);
  drawMainUi(zone);
  if (!wasTouchActive) {
    wasTouchActive = true;
    touchStartMs = millis();
  }
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(1);
  M5.Display.setTextSize(2);
  settingsLoad(g_settings);
  g_settings.normalize();
  applyBrightness();

  drawMainUi();
}

void loop() {
  M5.update();

  const int w = M5.Display.width();
  const int h = M5.Display.height();
  const uint8_t touchCount = M5.Touch.getCount();

  if (g_screen == Screen::Settings) {
    processSettingsTouch(touchCount, w, h);
  } else {
    processMainTouch(touchCount, w, h);
  }

  delay(10);
}
