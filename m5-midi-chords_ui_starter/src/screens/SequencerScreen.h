#pragma once

#include "../ui/UiGloss.h"
#include "../ui/UiTypes.h"

namespace screens {

struct SequencerScreenState {
  bool drawerOpen = false;
  uint8_t drawerPage = 0;  // 0 = Feel, 1 = Utility
};

void drawSequencerPage(ui::DisplayAdapter& d, const SequencerScreenState& state);
void drawSequencerTopDrawer(ui::DisplayAdapter& d, const SequencerScreenState& state);
bool processSequencerPageTouch(SequencerScreenState& state, int16_t x, int16_t y, bool pressed);
bool processSequencerTopDrawerTouch(SequencerScreenState& state, int16_t x, int16_t y, bool pressed);

}  // namespace screens
