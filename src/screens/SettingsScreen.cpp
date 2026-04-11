#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

using Rect = ::ui::Rect;

static int8_t s_settingsDropRowId = -1;
static int s_settingsDropOptScroll = 0;
static int s_settingsDropDragLastY = -1;
static int s_settingsDropFingerScrollCount = 0;
static int s_settingsTouchStartX = 0;
static int s_settingsTouchStartY = 0;

void resetSettingsNav() {
  g_settingsPanel = SettingsPanel::Menu;
  g_settingsCursorRow = 0;
  g_settingsListScroll = 0;
  g_userGuideScroll = 0;
  s_settingsDropRowId = -1;
  s_settingsDropOptScroll = 0;
  s_settingsDropDragLastY = -1;
  s_settingsDropFingerScrollCount = 0;
  g_factoryResetConfirmArmed = false;
  g_settingsConfirmAction = SettingsConfirmAction::None;
  g_settingsRow = 0;
  g_settingsFeedback[0] = '\0';
}

#include "SettingsScreen.inc"

}  // namespace m5chords_app
