#pragma once

#include "../ui/UiGloss.h"
#include "../ui/UiTypes.h"

namespace screens {

struct SettingsScreenState {};

void drawSettingsPage(ui::DisplayAdapter& d, const SettingsScreenState& state);
bool processSettingsPageTouch(SettingsScreenState& state, int16_t x, int16_t y, bool pressed);

}  // namespace screens
