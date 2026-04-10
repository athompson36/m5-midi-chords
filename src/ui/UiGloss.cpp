#include "UiGloss.h"
#include <cstdio>

#if !defined(PIO_NATIVE)
#include <M5Unified.h>
#include "UiTheme.h"
#endif

namespace ui {

namespace {

#if !defined(PIO_NATIVE)
void unpack565(uint16_t c, int* r, int* g, int* b) {
  *r = (c >> 11) & 0x1F;
  *g = (c >> 5) & 0x3F;
  *b = c & 0x1F;
}

uint16_t pack565(int r, int g, int b) {
  if (r < 0) r = 0;
  if (r > 31) r = 31;
  if (g < 0) g = 0;
  if (g > 63) g = 63;
  if (b < 0) b = 0;
  if (b > 31) b = 31;
  return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

uint16_t lerp565(uint16_t a, uint16_t b, int num, int den) {
  int ar, ag, ab, br, bg, bb;
  unpack565(a, &ar, &ag, &ab);
  unpack565(b, &br, &bg, &bb);
  const int rr = ar + (br - ar) * num / den;
  const int gg = ag + (bg - ag) * num / den;
  const int bb2 = ab + (bb - ab) * num / den;
  return pack565(rr, gg, bb2);
}

uint16_t lighten(uint16_t c, int amt) {
  int r, g, b;
  unpack565(c, &r, &g, &b);
  r = (r + amt > 31) ? 31 : r + amt;
  g = (g + amt > 63) ? 63 : g + amt * 2;
  b = (b + amt > 31) ? 31 : b + amt;
  return pack565(r, g, b);
}

uint16_t darken(uint16_t c, int amt) {
  int r, g, b;
  unpack565(c, &r, &g, &b);
  r = (r < amt) ? 0 : r - amt;
  g = (g < amt * 2) ? 0 : g - amt * 2;
  b = (b < amt) ? 0 : b - amt;
  return pack565(r, g, b);
}

GlossColors colorsFromBase(uint16_t base, GlossState st) {
  GlossColors c{};
  c.base = base;
  c.topBand = lighten(base, 4);
  c.bottomBand = darken(base, 5);
  c.borderTop = lighten(base, 6);
  c.borderBottom = darken(base, 7);
  c.innerHighlight = lighten(base, 3);
  c.ring = g_uiPalette.highlightRing;
  c.text = TFT_WHITE;
  if (st == GlossState::Pressed) {
    c.base = darken(base, 3);
    c.topBand = darken(c.topBand, 2);
    c.bottomBand = darken(c.bottomBand, 2);
  }
  if (st == GlossState::Disabled) {
    c.text = g_uiPalette.subtle;
  }
  return c;
}
#endif

}  // namespace

DisplayAdapter displayAdapterForM5() {
#if defined(PIO_NATIVE)
  return DisplayAdapter{};
#else
  return DisplayAdapter{static_cast<void*>(&M5.Display)};
#endif
}

#if !defined(PIO_NATIVE)
static M5GFX& gfxOf(DisplayAdapter& d) { return *static_cast<M5GFX*>(d.gfx); }
#endif

GlossColors resolveGlossColors(GlossRole role, GlossState state) {
#if defined(PIO_NATIVE)
  (void)role;
  (void)state;
  return GlossColors{};
#else
  uint16_t base = g_uiPalette.panelMuted;
  switch (role) {
    case GlossRole::Primary:
      base = g_uiPalette.settingsBtnActive;
      break;
    case GlossRole::Accent:
      base = g_uiPalette.keyCenter;
      break;
    case GlossRole::Muted:
      base = g_uiPalette.panelMuted;
      break;
    case GlossRole::Danger:
      base = g_uiPalette.danger;
      break;
    case GlossRole::Selected:
      base = g_uiPalette.rowSelect;
      break;
    case GlossRole::Neutral:
    default:
      base = g_uiPalette.rowNormal;
      break;
  }
  return colorsFromBase(base, state);
#endif
}

void drawGlossButton(DisplayAdapter& d, const GlossButtonSpec& spec) {
#if defined(PIO_NATIVE)
  (void)d;
  (void)spec;
#else
  if (!d.gfx) return;
  M5GFX& D = gfxOf(d);
  const Rect& r = spec.bounds;
  if (r.w <= 0 || r.h <= 0) return;
  GlossColors gc = (spec.customBase565 != 0) ? colorsFromBase(spec.customBase565, spec.state)
                                              : resolveGlossColors(spec.role, spec.state);
  const int rad = spec.radius > 0 ? spec.radius : max(4, min(static_cast<int>(r.w), static_cast<int>(r.h)) / 8);
  D.fillRoundRect(r.x, r.y, r.w, r.h, rad, gc.base);
  D.drawRoundRect(r.x, r.y, r.w, r.h, rad, gc.borderTop);
  const int bandH = max(1, static_cast<int>(r.h) / 5);
  D.fillRoundRect(r.x + 1, r.y + 1, r.w - 2, bandH, max(1, rad - 1), gc.topBand);
  D.drawFastHLine(r.x + 1, r.y + r.h - 1, r.w - 2, gc.borderBottom);
  if (spec.state == GlossState::Focused || spec.state == GlossState::Active) {
    D.drawRoundRect(r.x - 1, r.y - 1, r.w + 2, r.h + 2, rad + 1, gc.ring);
  }
  D.setFont(nullptr);
  D.setTextColor(gc.text, gc.base);
  D.setTextSize(spec.textSize);
  const int ty = spec.state == GlossState::Pressed ? 1 : 0;
  if (spec.centered) {
    D.setTextDatum(middle_center);
    D.drawString(spec.label, r.x + r.w / 2, r.y + r.h / 2 + ty);
  } else {
    D.setTextDatum(middle_left);
    D.drawString(spec.label, r.x + 4, r.y + r.h / 2 + ty);
  }
#endif
}

void drawDrawerSheet(DisplayAdapter& d, const DrawerSheetSpec& spec) {
#if defined(PIO_NATIVE)
  (void)d;
  (void)spec;
#else
  if (!d.gfx) return;
  M5GFX& D = gfxOf(d);
  const Rect& r = spec.bounds;
  const int rad = spec.radius;
  D.fillRoundRect(r.x, r.y, r.w, r.h, rad, g_uiPalette.panelMuted);
  D.drawRoundRect(r.x, r.y, r.w, r.h, rad, g_uiPalette.subtle);
  if (spec.showHandle) {
    const int hx = r.x + r.w / 2 - 14;
    D.drawFastHLine(hx, r.y + 5, 28, g_uiPalette.hintText);
  }
  if (spec.title && spec.title[0]) {
    D.setFont(nullptr);
    D.setTextDatum(top_center);
    D.setTextSize(1);
    D.setTextColor(g_uiPalette.rowNormal, g_uiPalette.panelMuted);
    D.drawString(spec.title, r.x + r.w / 2, r.y + (spec.showHandle ? 10 : 4));
  }
#endif
}

void drawStripShell(DisplayAdapter& d, const StripShellSpec& spec) {
#if defined(PIO_NATIVE)
  (void)d;
  (void)spec;
#else
  if (!d.gfx) return;
  M5GFX& D = gfxOf(d);
  const Rect& r = spec.bounds;
  const uint16_t fill = g_uiPalette.bg;
  D.fillRoundRect(r.x, r.y, r.w, r.h, spec.radius, fill);
  D.drawRoundRect(r.x, r.y, r.w, r.h, spec.radius, g_uiPalette.subtle);
#endif
}

void drawValuePill(DisplayAdapter& d, const ValuePillSpec& spec) {
#if defined(PIO_NATIVE)
  (void)d;
  (void)spec;
#else
  if (!d.gfx) return;
  GlossColors gc = resolveGlossColors(spec.role, spec.state);
  M5GFX& D = gfxOf(d);
  const Rect& r = spec.bounds;
  D.fillRoundRect(r.x, r.y, r.w, r.h, spec.radius, gc.base);
  D.drawRoundRect(r.x, r.y, r.w, r.h, spec.radius, gc.borderTop);
  D.setFont(nullptr);
  D.setTextSize(spec.textSize);
  D.setTextDatum(middle_left);
  D.setTextColor(gc.text, gc.base);
  char buf[64];
  if (spec.value && spec.value[0]) {
    snprintf(buf, sizeof(buf), "%s  %s", spec.label ? spec.label : "", spec.value);
  } else {
    snprintf(buf, sizeof(buf), "%s", spec.label ? spec.label : "");
  }
  D.drawString(buf, r.x + 4, r.y + r.h / 2);
#endif
}

void drawSliderTile(DisplayAdapter& d, const SliderTileSpec& spec) {
#if defined(PIO_NATIVE)
  (void)d;
  (void)spec;
#else
  if (!d.gfx) return;
  GlossColors gc = resolveGlossColors(spec.role, spec.state);
  M5GFX& D = gfxOf(d);
  const Rect& r = spec.bounds;
  D.fillRoundRect(r.x, r.y, r.w, r.h, spec.radius, g_uiPalette.bg);
  D.drawRoundRect(r.x, r.y, r.w, r.h, spec.radius, g_uiPalette.subtle);
  D.setFont(nullptr);
  D.setTextSize(spec.textSize);
  D.setTextDatum(top_left);
  D.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  D.drawString(spec.label ? spec.label : "", r.x + 4, r.y + 2);
  const int trackY = r.y + r.h / 2;
  const int trackH = 6;
  const int tx = r.x + 4;
  const int tw = r.w - 8;
  D.fillRoundRect(tx, trackY, tw, trackH, 3, g_uiPalette.panelMuted);
  const int fillW = (tw * spec.value) / 100;
  if (fillW > 0) {
    D.fillRoundRect(tx, trackY, fillW, trackH, 3, gc.base);
  }
  D.setTextDatum(middle_right);
  D.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  const char* vt = spec.valueText && spec.valueText[0] ? spec.valueText : "";
  D.drawString(vt, r.x + r.w - 4, r.y + r.h - 10);
#endif
}

void drawStatusBadge(DisplayAdapter& d, const StatusBadgeSpec& spec) {
#if defined(PIO_NATIVE)
  (void)d;
  (void)spec;
#else
  if (!d.gfx) return;
  GlossColors gc = resolveGlossColors(spec.role, spec.state);
  M5GFX& D = gfxOf(d);
  const Rect& r = spec.bounds;
  D.fillRoundRect(r.x, r.y, r.w, r.h, spec.radius, gc.base);
  D.setFont(nullptr);
  D.setTextSize(spec.textSize);
  D.setTextDatum(middle_center);
  D.setTextColor(gc.text, gc.base);
  D.drawString(spec.text ? spec.text : "", r.x + r.w / 2, r.y + r.h / 2);
#endif
}

}  // namespace ui
