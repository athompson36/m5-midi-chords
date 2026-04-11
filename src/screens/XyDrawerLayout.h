#pragma once

#include "ui/UiLayout.h"
#include "ui/UiTypes.h"

namespace xy_screen {

/// Rectangles for the X-Y top drawer (TabChip strip + Map or Options subpage).
struct XyDrawerLayoutRects {
  ui::Rect tabMap{};
  ui::Rect tabOpt{};
  ui::Rect mapCh{};
  ui::Rect mapCcA{};
  ui::Rect mapCcB{};
  ui::Rect mapCvA{};
  ui::Rect mapCvB{};
  ui::Rect optReturn{};
  ui::Rect optRec{};
};

/// subpage 0 = MIDI map (CH, CCs, curves); 1 = return policy + record-to-seq.
inline XyDrawerLayoutRects computeXyDrawerLayout(int screenW, int screenH, uint8_t /*subpage*/) {
  XyDrawerLayoutRects out{};
  constexpr int kDrawerH = 208;
  const ui::Rect drawer = ui::makeTopDrawerRect(screenW, screenH, kDrawerH);
  const ui::Rect content = ui::makeDrawerContentRect(drawer);
  const int gap = ui::LayoutMetrics::ControlGap;

  ui::Rect tabStrip = ui::makeStripRect(content, 4, 28);
  ui::Rect tabPair[2]{};
  ui::layout2Up(tabStrip, tabPair, gap);
  out.tabMap = tabPair[0];
  out.tabOpt = tabPair[1];

  ui::Rect rowMap1 = ui::makeStripRect(content, 36, 40);
  ui::Rect r3[3]{};
  ui::layout3Up(rowMap1, r3, gap);
  out.mapCh = r3[0];
  out.mapCcA = r3[1];
  out.mapCcB = r3[2];

  ui::Rect rowMap2 = ui::makeStripRect(content, 80, 40);
  ui::Rect r2m[2]{};
  ui::layout2Up(rowMap2, r2m, gap);
  out.mapCvA = r2m[0];
  out.mapCvB = r2m[1];

  out.optReturn = ui::makeStripRect(content, 124, 34);
  out.optRec = ui::makeStripRect(content, 162, 34);

  return out;
}

inline int xyDrawerHeightPx() { return 208; }

}  // namespace xy_screen
