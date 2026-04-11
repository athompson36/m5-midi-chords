#include <unity.h>

#include "screens/XyDrawerLayout.h"

void test_xy_drawer_map_three_column_row() {
  const xy_screen::XyDrawerLayoutRects r = xy_screen::computeXyDrawerLayout(320, 240, 0);
  TEST_ASSERT_TRUE(r.mapCh.w > 0);
  TEST_ASSERT_TRUE(r.mapCcA.w > 0);
  TEST_ASSERT_TRUE(r.mapCcB.w > 0);
  TEST_ASSERT_EQUAL_INT(r.mapCh.y, r.mapCcA.y);
  TEST_ASSERT_EQUAL_INT(r.mapCcA.y, r.mapCcB.y);
}

void test_xy_drawer_options_full_width_rows() {
  const xy_screen::XyDrawerLayoutRects r = xy_screen::computeXyDrawerLayout(320, 240, 1);
  TEST_ASSERT_TRUE(r.optReturn.w > 100);
  TEST_ASSERT_TRUE(r.optRec.w > 100);
  TEST_ASSERT_TRUE(r.optRec.y > r.optReturn.y);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_xy_drawer_map_three_column_row);
  RUN_TEST(test_xy_drawer_options_full_width_rows);
  return UNITY_END();
}
