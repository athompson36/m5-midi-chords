#include "UiDrawers.h"
#include "UiGloss.h"

namespace ui {

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

bool topDrawerHandleSwipeGesture(
    TopDrawerState& state,
    HostScreen currentScreen,
    bool transportDrawerOpen,
    bool modalOpen,
    uint8_t touchCount,
    int16_t touchY) {
  if (transportDrawerOpen || modalOpen) return false;
  if (!topDrawerAllowedForScreen(currentScreen)) return false;

  // TODO:
  // Replace this scaffold with real swipe tracking using touch start/end positions.
  // Current simple rule: if touch is present near top edge and no drawer is open, open it.
  if (!topDrawerIsOpen(state) && touchCount > 0 && touchY <= LayoutMetrics::StatusH + 6) {
    topDrawerOpenForScreen(state, currentScreen);
    return true;
  }
  return false;
}

void drawTopDrawerOverlay(DisplayAdapter& d, const TopDrawerState& state, HostScreen /*currentScreen*/) {
  if (!topDrawerIsOpen(state)) return;

  DrawerSheetSpec sheet{};
  sheet.bounds = Rect{6, LayoutMetrics::StatusH, static_cast<int16_t>(LayoutMetrics::ScreenW - 12), 112};
  sheet.title = "";
  sheet.role = GlossRole::Muted;
  drawDrawerSheet(d, sheet);

  // TODO:
  // dispatch to drawPlayTopDrawer / drawSequencerTopDrawer / drawXyTopDrawer
  // once those modules are wired in.
}

}  // namespace ui
