#pragma once

#include "../ui/UiGloss.h"
#include "../ui/UiTypes.h"

namespace screens {

struct PlayScreenState {
  bool drawerOpen = false;
};

void drawPlayPage(ui::DisplayAdapter& d, const PlayScreenState& state);
void drawPlayTopDrawer(ui::DisplayAdapter& d);
bool processPlayPageTouch(PlayScreenState& state, int16_t x, int16_t y, bool pressed);
bool processPlayTopDrawerTouch(PlayScreenState& state, int16_t x, int16_t y, bool pressed);

}  // namespace screens
