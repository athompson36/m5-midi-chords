#include "XyScreen.h"
#include "../ui/UiLayout.h"

namespace screens {

void drawXyPage(ui::DisplayAdapter& /*d*/, const XyScreenState& /*state*/) {
  // TODO:
  // Move current X-Y page rendering here.
  // Keep only:
  // - X-Y pad
  // - value feedback
  // - activity feedback
}

void drawXyTopDrawer(ui::DisplayAdapter& d) {
  // Suggested strip layout:
  // 1) Assignment: Ch / X CC / Y CC
  // 2) Curves: X Curve / Y Curve
  // 3) Behavior: Return / Rec

  const auto drawer = ui::makeTopDrawerRect(ui::LayoutMetrics::ScreenW, ui::LayoutMetrics::ScreenH, 120);
  const auto content = ui::makeDrawerContentRect(drawer);

  ui::DrawerSheetSpec sheet{};
  sheet.bounds = drawer;
  sheet.role = ui::GlossRole::Muted;
  ui::drawDrawerSheet(d, sheet);

  ui::Rect strip1 = ui::makeStripRect(content, 4, 30);
  ui::Rect strip2 = ui::makeStripRect(content, 40, 30);
  ui::Rect strip3 = ui::makeStripRect(content, 76, 30);

  ui::drawStripShell(d, ui::StripShellSpec{strip1});
  ui::drawStripShell(d, ui::StripShellSpec{strip2});
  ui::drawStripShell(d, ui::StripShellSpec{strip3});

  // TODO:
  // Add actual ValuePill components once current repo state is wired in.
}

bool processXyPageTouch(XyScreenState& /*state*/, int16_t /*x*/, int16_t /*y*/, bool /*pressed*/) {
  // TODO: move current X-Y touch handling here.
  return false;
}

bool processXyTopDrawerTouch(XyScreenState& /*state*/, int16_t /*x*/, int16_t /*y*/, bool /*pressed*/) {
  // TODO:
  // Touch handling for channel / CC / curves / return / rec.
  return false;
}

}  // namespace screens
