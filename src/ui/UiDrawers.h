#pragma once

#include "UiTypes.h"

namespace ui {

enum class HostScreen : uint8_t {
  Unknown = 0,
  Play,
  Sequencer,
  XyPad,
  Transport,
  Settings,
  Other
};

bool topDrawerAllowedForScreen(HostScreen s);
TopDrawerKind topDrawerKindForScreen(HostScreen s);

void topDrawerOpenForScreen(TopDrawerState& state, HostScreen s);
void topDrawerClose(TopDrawerState& state);
bool topDrawerIsOpen(const TopDrawerState& state);

/// Start tracking a downward swipe from the top edge. Call on touch-down.
void topDrawerOnTouchDown(TopDrawerState& state, HostScreen currentScreen, bool transportDrawerOpen,
                          bool modalOpen, int16_t touchY);

/// Returns true if drawer was opened (caller should redraw).
bool topDrawerOnTouchUpAfterSwipe(TopDrawerState& state, HostScreen currentScreen, bool transportDrawerOpen,
                                  bool modalOpen, int16_t endY);

}  // namespace ui
