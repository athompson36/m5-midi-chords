#pragma once

#include <stdint.h>

struct IntRect {
  int x, y, w, h;
};

struct SettingsEntryGestureState {
  uint32_t holdStartMs = 0;
  bool needRelease = false;
};

struct SettingsEntryGestureResult {
  bool enteredSettings = false;
  bool suppressNextPlayTap = false;
};

/// Match play-surface layout: bottom third-width strips for BACK | SELECT | FWD.
void settingsEntryGesture_computeBezelRects(int w, int h, int bezelBarH,
                                            IntRect* out_back, IntRect* out_select,
                                            IntRect* out_fwd);

/// True if at least one touch lies in `back` and at least one (possibly another) in `fwd`.
/// If `pressed` is nullptr, all samples are treated as pressed.
bool settingsEntryGesture_touchesCoverBackAndFwd(uint8_t touchCount, const int* xs,
                                                 const int* ys, const bool* pressed,
                                                 const IntRect& back, const IntRect& fwd);

/// State machine for long-hold BACK+FWD → settings. Caller applies screen change when
/// `enteredSettings` is true.
SettingsEntryGestureResult settingsEntryGesture_update(SettingsEntryGestureState& state,
                                                       uint32_t nowMs, uint8_t touchCount,
                                                       const int* xs, const int* ys,
                                                       const bool* pressed,
                                                       const IntRect& back, const IntRect& fwd,
                                                       uint32_t holdThresholdMs);
