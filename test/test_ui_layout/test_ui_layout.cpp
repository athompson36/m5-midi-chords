#include <unity.h>

#include "ui/UiLayout.h"

void test_inset_rect_shrinks_by_pad() {
  const ui::Rect r{10, 20, 100, 50};
  const ui::Rect i = ui::insetRect(r, 5);
  TEST_ASSERT_EQUAL_INT(15, i.x);
  TEST_ASSERT_EQUAL_INT(25, i.y);
  TEST_ASSERT_EQUAL_INT(90, i.w);
  TEST_ASSERT_EQUAL_INT(40, i.h);
}

void test_make_top_drawer_rect_uses_margins_and_status() {
  const ui::Rect d = ui::makeTopDrawerRect(320, 240, 80);
  TEST_ASSERT_EQUAL_INT(ui::LayoutMetrics::OuterMargin, d.x);
  TEST_ASSERT_EQUAL_INT(ui::LayoutMetrics::StatusH, d.y);
  TEST_ASSERT_EQUAL_INT(320 - 2 * ui::LayoutMetrics::OuterMargin, d.w);
  TEST_ASSERT_EQUAL_INT(80, d.h);
}

void test_layout2_up_splits_width_with_gap() {
  const ui::Rect r{0, 0, 100, 10};
  ui::Rect out[2];
  ui::layout2Up(r, out, 4);
  TEST_ASSERT_EQUAL_INT(0, out[0].x);
  TEST_ASSERT_EQUAL_INT(48, out[0].w);
  TEST_ASSERT_EQUAL_INT(52, out[1].x);
  TEST_ASSERT_EQUAL_INT(48, out[1].w);
  TEST_ASSERT_EQUAL_INT(100, out[0].w + 4 + out[1].w);
}

void test_layout3_up_last_cell_fills_remainder() {
  const ui::Rect r{0, 0, 100, 8};
  ui::Rect out[3];
  ui::layout3Up(r, out, 4);
  TEST_ASSERT_EQUAL_INT(0, out[0].x);
  TEST_ASSERT_EQUAL_INT(30, out[0].w);
  TEST_ASSERT_EQUAL_INT(34, out[1].x);
  TEST_ASSERT_EQUAL_INT(30, out[1].w);
  TEST_ASSERT_EQUAL_INT(68, out[2].x);
  TEST_ASSERT_EQUAL_INT(32, out[2].w);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_inset_rect_shrinks_by_pad);
  RUN_TEST(test_make_top_drawer_rect_uses_margins_and_status);
  RUN_TEST(test_layout2_up_splits_width_with_gap);
  RUN_TEST(test_layout3_up_last_cell_fills_remainder);
  return UNITY_END();
}
