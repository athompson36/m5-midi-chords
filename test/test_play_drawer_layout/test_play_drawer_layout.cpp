#include <unity.h>

#include "screens/PlayDrawerLayout.h"

void test_play_drawer_three_column_velocity_row() {
  const play_screen::PlayDrawerRects r = play_screen::computePlayDrawerLayout(320, 240);
  TEST_ASSERT_TRUE(r.velMinus.w > 0);
  TEST_ASSERT_TRUE(r.velSlider.w > 0);
  TEST_ASSERT_TRUE(r.velPlus.w > 0);
  TEST_ASSERT_EQUAL_INT(r.velMinus.y, r.velSlider.y);
  TEST_ASSERT_EQUAL_INT(r.velSlider.y, r.velPlus.y);
  TEST_ASSERT_EQUAL_INT(r.velMinus.h, r.velSlider.h);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_play_drawer_three_column_velocity_row);
  return UNITY_END();
}
