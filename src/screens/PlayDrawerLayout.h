#pragma once

#include "ui/UiLayout.h"
#include "ui/UiTypes.h"

namespace play_screen {

/// All rectangles for the Play top drawer (tabs + two subpages sharing row1 geometry).
struct PlayDrawerLayoutRects {
  ui::Rect tabOutput{};
  ui::Rect tabKey{};
  ui::Rect transMinus{};
  ui::Rect transPlus{};
  ui::Rect velMinus{};
  ui::Rect velSlider{};
  ui::Rect velPlus{};
  ui::Rect midiCh{};
  ui::Rect voicing{};
  ui::Rect keyMinus{};
  ui::Rect keyPlus{};
  ui::Rect modeBtn{};
  ui::Rect arpBtn{};
  ui::Rect curveBtn{};
};

using PlayDrawerRects = PlayDrawerLayoutRects;

/// subpage 0 = output; 1 = key / mode / arp / curve. Layout is shared; subpage selects which controls are active.
inline PlayDrawerLayoutRects computePlayDrawerLayout(int screenW, int screenH, uint8_t /*subpage*/ = 0) {
  PlayDrawerLayoutRects out{};
  constexpr int kDrawerH = 200;
  const ui::Rect drawer = ui::makeTopDrawerRect(screenW, screenH, kDrawerH);
  const ui::Rect content = ui::makeDrawerContentRect(drawer);
  const int gap = ui::LayoutMetrics::ControlGap;

  ui::Rect tabStrip = ui::makeStripRect(content, 4, 28);
  ui::Rect tabPair[2]{};
  ui::layout2Up(tabStrip, tabPair, gap);
  out.tabOutput = tabPair[0];
  out.tabKey = tabPair[1];

  ui::Rect row1 = ui::makeStripRect(content, 36, 40);
  ui::Rect row2 = ui::makeStripRect(content, 80, 40);
  ui::Rect row3 = ui::makeStripRect(content, 124, 40);
  ui::Rect a2[2]{};
  ui::Rect a3[3]{};

  ui::layout2Up(row1, a2, gap);
  out.transMinus = out.keyMinus = a2[0];
  out.transPlus = out.keyPlus = a2[1];

  ui::layout3Up(row2, a3, gap);
  out.velMinus = a3[0];
  out.velSlider = a3[1];
  out.velPlus = a3[2];
  out.modeBtn = row2;

  ui::layout2Up(row3, a2, gap);
  out.midiCh = out.arpBtn = a2[0];
  out.voicing = out.curveBtn = a2[1];

  return out;
}

inline int playDrawerHeightPx() { return 200; }

}  // namespace play_screen
