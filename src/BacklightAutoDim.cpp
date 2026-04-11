#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

static uint32_t s_lastDisplayActivityMs = 0;
static bool s_backlightDimmedIdle = false;

void applyBrightness() {
  uint8_t pct = g_settings.brightnessPercent;
  if (s_backlightDimmedIdle && g_settings.displayAutoDimPreset != 0) {
    uint16_t scaled = (static_cast<uint16_t>(pct) * 20U) / 100U;
    uint8_t low = static_cast<uint8_t>(scaled < 8U ? 8U : scaled);
    if (low > pct) low = pct;
    pct = low;
  }
  uint8_t v = static_cast<uint8_t>((static_cast<uint16_t>(pct) * 255U) / 100U);
  M5.Display.setBrightness(v);
}

void wakeDisplayBacklightFromIdleDim() {
  s_lastDisplayActivityMs = millis();
  s_backlightDimmedIdle = false;
  applyBrightness();
}

void updateBacklightAutoDim(uint32_t now, uint8_t touchCount) {
  if (s_lastDisplayActivityMs == 0) s_lastDisplayActivityMs = now;
  if (touchCount > 0) {
    if (s_backlightDimmedIdle) {
      s_backlightDimmedIdle = false;
      applyBrightness();
    }
    s_lastDisplayActivityMs = now;
    return;
  }
  const uint32_t timeoutMs = g_settings.displayAutoDimIdleTimeoutMs();
  if (timeoutMs == 0) {
    if (s_backlightDimmedIdle) {
      s_backlightDimmedIdle = false;
      applyBrightness();
    }
    return;
  }
  if (!s_backlightDimmedIdle && (now - s_lastDisplayActivityMs) >= timeoutMs) {
    s_backlightDimmedIdle = true;
    applyBrightness();
  }
}

}  // namespace m5chords_app
