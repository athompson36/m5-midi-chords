#pragma once

#include "UiTypes.h"

namespace ui {

Rect insetRect(const Rect& r, int16_t pad);
Rect makeTopDrawerRect(int screenW, int screenH, int drawerH);
Rect makeDrawerContentRect(const Rect& drawer);
Rect makeStripRect(const Rect& content, int y, int h);

void layout2Up(const Rect& r, Rect out[2], int gap = LayoutMetrics::ControlGap);
void layout3Up(const Rect& r, Rect out[3], int gap = LayoutMetrics::ControlGap);
void layout4Up(const Rect& r, Rect out[4], int gap = LayoutMetrics::ControlGap);

}  // namespace ui
