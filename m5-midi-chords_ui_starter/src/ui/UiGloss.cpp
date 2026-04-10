#include "UiGloss.h"

// NOTE:
// This file intentionally contains scaffold logic rather than repo-specific M5GFX calls.
// Replace the TODO blocks with the project's actual drawRoundRect / fillRoundRect / drawString wrappers.

namespace ui {

namespace {
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
}  // namespace

GlossColors resolveGlossColors(GlossRole role, GlossState state) {
  // TODO: derive from the repo's existing UiPalette instead of these placeholder values.
  GlossColors c{};
  switch (role) {
    case GlossRole::Primary:
      c.base = rgb565(60, 110, 220);
      c.text = rgb565(255, 255, 255);
      break;
    case GlossRole::Accent:
      c.base = rgb565(150, 80, 220);
      c.text = rgb565(255, 255, 255);
      break;
    case GlossRole::Muted:
      c.base = rgb565(50, 55, 65);
      c.text = rgb565(225, 225, 225);
      break;
    case GlossRole::Danger:
      c.base = rgb565(185, 55, 55);
      c.text = rgb565(255, 255, 255);
      break;
    case GlossRole::Selected:
      c.base = rgb565(80, 150, 100);
      c.text = rgb565(255, 255, 255);
      break;
    case GlossRole::Neutral:
    default:
      c.base = rgb565(75, 80, 90);
      c.text = rgb565(245, 245, 245);
      break;
  }

  c.topBand = rgb565(120, 125, 135);
  c.bottomBand = rgb565(35, 40, 48);
  c.borderTop = rgb565(170, 175, 185);
  c.borderBottom = rgb565(20, 24, 28);
  c.innerHighlight = rgb565(220, 220, 230);
  c.ring = rgb565(150, 210, 255);

  if (state == GlossState::Disabled) {
    c.text = rgb565(170, 170, 170);
  }
  return c;
}

void drawGlossButton(DisplayAdapter& /*d*/, const GlossButtonSpec& /*spec*/) {
  // TODO:
  // - fill outer shell
  // - fill inner body
  // - draw top gloss band
  // - draw 1px top highlight
  // - draw 1px lower shadow
  // - draw focus ring if needed
  // - draw text, shifting down by 1px when pressed
}

void drawDrawerSheet(DisplayAdapter& /*d*/, const DrawerSheetSpec& /*spec*/) {
  // TODO:
  // - draw full-width rounded sheet
  // - optional top handle line
  // - title text
}

void drawStripShell(DisplayAdapter& /*d*/, const StripShellSpec& /*spec*/) {
  // TODO:
  // - draw strip backing rail inside drawer
}

void drawValuePill(DisplayAdapter& /*d*/, const ValuePillSpec& /*spec*/) {
  // TODO:
  // - draw capsule shell
  // - label left
  // - value right
}

void drawSliderTile(DisplayAdapter& /*d*/, const SliderTileSpec& /*spec*/) {
  // TODO:
  // - draw shell
  // - label + value
  // - track + fill
}

void drawStatusBadge(DisplayAdapter& /*d*/, const StatusBadgeSpec& /*spec*/) {
  // TODO:
  // - draw compact status chip
}

}  // namespace ui
