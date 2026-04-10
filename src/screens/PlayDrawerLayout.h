#pragma once

#include "ui/UiLayout.h"
#include "ui/UiTypes.h"

namespace play_screen {

struct PlayDrawerRects {
  ui::Rect transMinus{};
  ui::Rect transPlus{};
  ui::Rect velMinus{};
  ui::Rect velSlider{};
  ui::Rect velPlus{};
  ui::Rect midiCh{};
  ui::Rect voicing{};
};

/// Matches Play drawer layout (112px drawer height).
inline PlayDrawerRects computePlayDrawerLayout(int screenW, int screenH) {
  PlayDrawerRects out{};
  constexpr int kDrawerH = 112;
  const ui::Rect drawer = ui::makeTopDrawerRect(screenW, screenH, kDrawerH);
  const ui::Rect content = ui::makeDrawerContentRect(drawer);
  const int gap = ui::LayoutMetrics::ControlGap;

  ui::Rect row1 = ui::makeStripRect(content, 4, 28);
  ui::Rect row2 = ui::makeStripRect(content, 36, 28);
  ui::Rect row3 = ui::makeStripRect(content, 68, 28);
  ui::Rect a2[2]{};
  ui::Rect a3[3]{};
  ui::layout2Up(row1, a2, gap);
  out.transMinus = a2[0];
  out.transPlus = a2[1];
  ui::layout3Up(row2, a3, gap);
  out.velMinus = a3[0];
  out.velSlider = a3[1];
  out.velPlus = a3[2];
  ui::layout2Up(row3, a2, gap);
  out.midiCh = a2[0];
  out.voicing = a2[1];
  return out;
}

}  // namespace play_screen
