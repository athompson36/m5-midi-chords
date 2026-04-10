#pragma once

#include "../ui/UiGloss.h"
#include "../ui/UiTypes.h"

namespace screens {

struct XyScreenState {
  bool drawerOpen = false;
};

void drawXyPage(ui::DisplayAdapter& d, const XyScreenState& state);
void drawXyTopDrawer(ui::DisplayAdapter& d);
bool processXyPageTouch(XyScreenState& state, int16_t x, int16_t y, bool pressed);
bool processXyTopDrawerTouch(XyScreenState& state, int16_t x, int16_t y, bool pressed);

}  // namespace screens
