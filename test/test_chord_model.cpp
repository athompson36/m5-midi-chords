#include <unity.h>

#include "ChordModel.h"

void test_move_down_wraps_to_start() {
  ChordModel model;
  model.selectedIndex = ChordModel::kChordCount - 1;
  model.moveDown();
  TEST_ASSERT_EQUAL(0, model.selectedIndex);
}

void test_move_up_wraps_to_end() {
  ChordModel model;
  model.selectedIndex = 0;
  model.moveUp();
  TEST_ASSERT_EQUAL(ChordModel::kChordCount - 1, model.selectedIndex);
}

void test_selected_chord_returns_expected_name() {
  ChordModel model;
  model.selectedIndex = 3;
  TEST_ASSERT_EQUAL_STRING("G7", model.selectedChord());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_move_down_wraps_to_start);
  RUN_TEST(test_move_up_wraps_to_end);
  RUN_TEST(test_selected_chord_returns_expected_name);
  return UNITY_END();
}
