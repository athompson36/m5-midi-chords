#include "UiDrawers.h"

namespace ui {

namespace {
constexpr int16_t kTopEdgeBand = 28;
constexpr int16_t kSwipeOpenThreshold = 36;
}

bool topDrawerAllowedForScreen(HostScreen s) {
  return s == HostScreen::Play || s == HostScreen::Sequencer || s == HostScreen::XyPad;
}

TopDrawerKind topDrawerKindForScreen(HostScreen s) {
  switch (s) {
    case HostScreen::Play:
      return TopDrawerKind::Play;
    case HostScreen::Sequencer:
      return TopDrawerKind::Sequencer;
    case HostScreen::XyPad:
      return TopDrawerKind::Xy;
    default:
      return TopDrawerKind::None;
  }
}

void topDrawerOpenForScreen(TopDrawerState& state, HostScreen s) {
  state.kind = topDrawerKindForScreen(s);
  // Ignore stray down-events until all fingers lift (same idea as transport drawer).
  state.ignoreUntilTouchUp = true;
  state.swipeTracking = false;
  state.subpage = 0;
}

void topDrawerClose(TopDrawerState& state) {
  state.kind = TopDrawerKind::None;
  state.ignoreUntilTouchUp = false;
  state.swipeTracking = false;
  state.subpage = 0;
}

bool topDrawerIsOpen(const TopDrawerState& state) {
  return state.kind != TopDrawerKind::None;
}

void topDrawerOnTouchDown(TopDrawerState& state, HostScreen currentScreen, bool transportDrawerOpen,
                          bool modalOpen, int16_t touchY) {
  if (transportDrawerOpen || modalOpen) return;
  if (!topDrawerAllowedForScreen(currentScreen)) return;
  if (topDrawerIsOpen(state)) return;
  if (touchY <= kTopEdgeBand) {
    state.swipeTracking = true;
    state.swipeStartY = touchY;
  } else {
    state.swipeTracking = false;
  }
}

bool topDrawerOnTouchUpAfterSwipe(TopDrawerState& state, HostScreen currentScreen, bool transportDrawerOpen,
                                  bool modalOpen, int16_t endY) {
  if (transportDrawerOpen || modalOpen) {
    state.swipeTracking = false;
    return false;
  }
  if (!topDrawerAllowedForScreen(currentScreen)) {
    state.swipeTracking = false;
    return false;
  }
  if (!state.swipeTracking) return false;
  const int16_t startY = state.swipeStartY;
  state.swipeTracking = false;
  if (topDrawerIsOpen(state)) return false;
  const int16_t delta = endY - startY;
  if (startY <= kTopEdgeBand && delta >= kSwipeOpenThreshold) {
    topDrawerOpenForScreen(state, currentScreen);
    return true;
  }
  return false;
}

}  // namespace ui
