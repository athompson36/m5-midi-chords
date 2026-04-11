#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

using Rect = ::ui::Rect;

struct XyComboTrack {
  bool armed;
  int sx, sy, lx, ly;
};

XyComboTrack s_xyCombo{};

#include "XyScreen.inc"

}  // namespace m5chords_app
