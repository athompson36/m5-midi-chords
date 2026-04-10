#include "SettingsScreen.h"

namespace screens {

void drawSettingsPage(ui::DisplayAdapter& /*d*/, const SettingsScreenState& /*state*/) {
  // TODO:
  // Move settings rendering here over time:
  // - rows
  // - dropdowns
  // - confirms
  // - admin/settings sections
}

bool processSettingsPageTouch(SettingsScreenState& /*state*/, int16_t /*x*/, int16_t /*y*/, bool /*pressed*/) {
  // TODO:
  return false;
}

}  // namespace screens
