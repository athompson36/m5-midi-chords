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

struct DisplayAdapter;

bool topDrawerAllowedForScreen(HostScreen s);
TopDrawerKind topDrawerKindForScreen(HostScreen s);

void topDrawerOpenForScreen(TopDrawerState& state, HostScreen s);
void topDrawerClose(TopDrawerState& state);
bool topDrawerIsOpen(const TopDrawerState& state);

// Gesture helper scaffold.
// Returns true if the gesture was consumed.
bool topDrawerHandleSwipeGesture(
    TopDrawerState& state,
    HostScreen currentScreen,
    bool transportDrawerOpen,
    bool modalOpen,
    uint8_t touchCount,
    int16_t touchY);

// Draw/dispatch entry points.
// These are generic dispatchers that should call page-specific drawer implementations.
void drawTopDrawerOverlay(DisplayAdapter& d, const TopDrawerState& state, HostScreen currentScreen);

}  // namespace ui
