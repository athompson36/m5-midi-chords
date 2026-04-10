#include "SequencerScreen.h"
#include "../ui/UiLayout.h"

namespace screens {

void drawSequencerPage(ui::DisplayAdapter& /*d*/, const SequencerScreenState& /*state*/) {
  // TODO:
  // Move current sequencer grid / lane tab rendering here.
  // Main page should stay focused on:
  // - lane tabs
  // - grid
  // - playhead
  // - immediate step feedback
}

void drawSequencerTopDrawer(ui::DisplayAdapter& d, const SequencerScreenState& state) {
  const auto drawer = ui::makeTopDrawerRect(ui::LayoutMetrics::ScreenW, ui::LayoutMetrics::ScreenH, 126);
  const auto content = ui::makeDrawerContentRect(drawer);

  ui::DrawerSheetSpec sheet{};
  sheet.bounds = drawer;
  ui::drawDrawerSheet(d, sheet);

  if (state.drawerPage == 0) {
    // Feel page:
    // lane / channel
    // swing / quant
    // random / humanize
    ui::drawStripShell(d, ui::StripShellSpec{ui::makeStripRect(content, 4, 30)});
    ui::drawStripShell(d, ui::StripShellSpec{ui::makeStripRect(content, 40, 36)});
    ui::drawStripShell(d, ui::StripShellSpec{ui::makeStripRect(content, 82, 36)});
  } else {
    // Utility page:
    // BPM / transpose
    // arp / clear
    // copy / rotate or placeholders
    ui::drawStripShell(d, ui::StripShellSpec{ui::makeStripRect(content, 4, 30)});
    ui::drawStripShell(d, ui::StripShellSpec{ui::makeStripRect(content, 40, 30)});
    ui::drawStripShell(d, ui::StripShellSpec{ui::makeStripRect(content, 76, 30)});
  }
}

bool processSequencerPageTouch(SequencerScreenState& /*state*/, int16_t /*x*/, int16_t /*y*/, bool /*pressed*/) {
  // TODO: move current sequencer touch handling here.
  return false;
}

bool processSequencerTopDrawerTouch(SequencerScreenState& /*state*/, int16_t /*x*/, int16_t /*y*/, bool /*pressed*/) {
  // TODO:
  // Handle feel and utility drawer controls.
  // Keep per-step micro-editing out of the top drawer.
  return false;
}

}  // namespace screens
