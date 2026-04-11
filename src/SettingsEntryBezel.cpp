#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

static uint32_t s_bezelLongPressMs = 0;
static bool s_bezelLongPressFired = false;
static HoldProgressOwner s_holdProgressOwner = HoldProgressOwner::None;

uint32_t settingsEntryHoldMs() {
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

void drawHoldProgressStrip(HoldProgressOwner owner, uint32_t elapsedMs, uint32_t targetMs, const char* label) {
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

void clearHoldProgressStrip(HoldProgressOwner owner) {
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

}  // namespace m5chords_app
