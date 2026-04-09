#include "SettingsEntryGesture.h"

namespace {

bool pointIn(int px, int py, const IntRect& r) {
  return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

}  // namespace

void settingsEntryGesture_computeBezelRects(int w, int h, int bezelBarH, IntRect* out_back,
                                            IntRect* out_select, IntRect* out_fwd) {
  const int bz = w / 3;
  const int bezelY = h - bezelBarH;
  if (out_back) *out_back = {0, bezelY, bz, bezelBarH};
  if (out_select) *out_select = {bz, bezelY, bz, bezelBarH};
  if (out_fwd) *out_fwd = {2 * bz, bezelY, bz, bezelBarH};
}

bool settingsEntryGesture_touchesCoverBackAndFwd(uint8_t touchCount, const int* xs,
                                                 const int* ys, const bool* pressed,
                                                 const IntRect& back, const IntRect& fwd) {
  if (touchCount < 2 || xs == nullptr || ys == nullptr) return false;
  bool haveBack = false;
  bool haveFwd = false;
  for (uint8_t i = 0; i < touchCount; ++i) {
    if (pressed != nullptr && !pressed[i]) continue;
    if (pointIn(xs[i], ys[i], back)) haveBack = true;
    if (pointIn(xs[i], ys[i], fwd)) haveFwd = true;
  }
  return haveBack && haveFwd;
}

SettingsEntryGestureResult settingsEntryGesture_update(SettingsEntryGestureState& state,
                                                       uint32_t nowMs, uint8_t touchCount,
                                                       const int* xs, const int* ys,
                                                       const bool* pressed, const IntRect& back,
                                                       const IntRect& fwd,
                                                       uint32_t holdThresholdMs) {
  SettingsEntryGestureResult out;
  if (state.needRelease) {
    if (touchCount == 0) state.needRelease = false;
    return out;
  }

  if (settingsEntryGesture_touchesCoverBackAndFwd(touchCount, xs, ys, pressed, back, fwd)) {
    if (state.holdStartMs == 0) state.holdStartMs = nowMs;
    if (nowMs - state.holdStartMs >= holdThresholdMs) {
      state.holdStartMs = 0;
      state.needRelease = true;
      out.enteredSettings = true;
      out.suppressNextPlayTap = true;
    }
  } else {
    state.holdStartMs = 0;
  }
  return out;
}
