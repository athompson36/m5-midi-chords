#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

using Rect = ::ui::Rect;

extern bool s_shiftActive;

void drawRoundedButton(const Rect& r, uint16_t bg, const char* label, uint8_t textSize) {
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

void drawTopSystemStatus(int w, int yTop, const char* extBpmText, uint16_t extBpmColor) {
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

}  // namespace m5chords_app
