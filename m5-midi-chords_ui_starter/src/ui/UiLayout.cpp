#include "UiLayout.h"

namespace ui {

Rect insetRect(const Rect& r, int16_t pad) {
  return Rect{
      static_cast<int16_t>(r.x + pad),
      static_cast<int16_t>(r.y + pad),
      static_cast<int16_t>(r.w - 2 * pad),
      static_cast<int16_t>(r.h - 2 * pad)};
}

Rect makeTopDrawerRect(int screenW, int /*screenH*/, int drawerH) {
  return Rect{
      static_cast<int16_t>(LayoutMetrics::OuterMargin),
      static_cast<int16_t>(LayoutMetrics::StatusH),
      static_cast<int16_t>(screenW - 2 * LayoutMetrics::OuterMargin),
      static_cast<int16_t>(drawerH)};
}

Rect makeDrawerContentRect(const Rect& drawer) {
  return insetRect(drawer, LayoutMetrics::DrawerPadding);
}

Rect makeStripRect(const Rect& content, int y, int h) {
  return Rect{
      content.x,
      static_cast<int16_t>(content.y + y),
      content.w,
      static_cast<int16_t>(h)};
}

void layout2Up(const Rect& r, Rect out[2], int gap) {
  const int w = (r.w - gap) / 2;
  out[0] = Rect{r.x, r.y, static_cast<int16_t>(w), r.h};
  out[1] = Rect{static_cast<int16_t>(r.x + w + gap), r.y, static_cast<int16_t>(r.w - w - gap), r.h};
}

void layout3Up(const Rect& r, Rect out[3], int gap) {
  const int w = (r.w - 2 * gap) / 3;
  out[0] = Rect{r.x, r.y, static_cast<int16_t>(w), r.h};
  out[1] = Rect{static_cast<int16_t>(r.x + w + gap), r.y, static_cast<int16_t>(w), r.h};
  out[2] = Rect{static_cast<int16_t>(r.x + 2 * (w + gap)), r.y, static_cast<int16_t>(r.w - 2 * (w + gap)), r.h};
}

void layout4Up(const Rect& r, Rect out[4], int gap) {
  const int w = (r.w - 3 * gap) / 4;
  out[0] = Rect{r.x, r.y, static_cast<int16_t>(w), r.h};
  out[1] = Rect{static_cast<int16_t>(r.x + (w + gap)), r.y, static_cast<int16_t>(w), r.h};
  out[2] = Rect{static_cast<int16_t>(r.x + 2 * (w + gap)), r.y, static_cast<int16_t>(w), r.h};
  out[3] = Rect{static_cast<int16_t>(r.x + 3 * (w + gap)), r.y, static_cast<int16_t>(r.w - 3 * (w + gap)), r.h};
}

}  // namespace ui
