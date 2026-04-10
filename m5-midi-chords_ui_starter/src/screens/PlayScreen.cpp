#include "PlayScreen.h"
#include "../ui/UiLayout.h"

namespace screens {

void drawPlayPage(ui::DisplayAdapter& /*d*/, const PlayScreenState& /*state*/) {
  // TODO:
  // Move current Play surface rendering here.
  // Keep the main page focused on:
  // - center key pad
  // - chord pads
  // - immediate feedback
}

void drawPlayTopDrawer(ui::DisplayAdapter& d) {
  // Suggested strips:
  // 1) Key / Mode
  // 2) Trans / Voice / Arp / Ch
  // 3) Velocity / Curve

  const auto drawer = ui::makeTopDrawerRect(ui::LayoutMetrics::ScreenW, ui::LayoutMetrics::ScreenH, 120);
  const auto content = ui::makeDrawerContentRect(drawer);

  ui::DrawerSheetSpec sheet{};
  sheet.bounds = drawer;
  ui::drawDrawerSheet(d, sheet);

  ui::Rect strip1 = ui::makeStripRect(content, 4, 30);
  ui::Rect strip2 = ui::makeStripRect(content, 40, 30);
  ui::Rect strip3 = ui::makeStripRect(content, 76, 36);

  ui::drawStripShell(d, ui::StripShellSpec{strip1});
  ui::drawStripShell(d, ui::StripShellSpec{strip2});
  ui::drawStripShell(d, ui::StripShellSpec{strip3});
}

bool processPlayPageTouch(PlayScreenState& /*state*/, int16_t /*x*/, int16_t /*y*/, bool /*pressed*/) {
  // TODO: move current Play touch handling here.
  return false;
}

bool processPlayTopDrawerTouch(PlayScreenState& /*state*/, int16_t /*x*/, int16_t /*y*/, bool /*pressed*/) {
  // TODO:
  // Key / mode / transpose / voicing / arp / channel / velocity / curve
  return false;
}

}  // namespace screens
