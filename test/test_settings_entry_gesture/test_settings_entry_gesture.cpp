#include <unity.h>

#include "SettingsEntryGesture.h"

void test_back_fwd_requires_two_distinct_zones() {
  IntRect back{};
  IntRect sel{};
  IntRect fwd{};
  settingsEntryGesture_computeBezelRects(320, 240, 34, &back, &sel, &fwd);

  int xs[2] = {back.x + 2, back.x + 4};
  int ys[2] = {back.y + 2, back.y + 4};
  TEST_ASSERT_FALSE(settingsEntryGesture_touchesCoverBackAndFwd(2, xs, ys, nullptr, back, fwd));

  int xs2[2] = {back.x + 2, fwd.x + 2};
  int ys2[2] = {back.y + 2, fwd.y + 2};
  TEST_ASSERT_TRUE(settingsEntryGesture_touchesCoverBackAndFwd(2, xs2, ys2, nullptr, back, fwd));
}

void test_hold_fires_after_threshold() {
  IntRect back{};
  IntRect sel{};
  IntRect fwd{};
  settingsEntryGesture_computeBezelRects(320, 240, 34, &back, &sel, &fwd);

  SettingsEntryGestureState st{};
  int xs[2] = {back.x + 2, fwd.x + 2};
  int ys[2] = {back.y + 2, fwd.y + 2};
  constexpr uint32_t kHold = 800;

  auto r0 = settingsEntryGesture_update(st, 1000, 2, xs, ys, nullptr, back, fwd, kHold);
  TEST_ASSERT_FALSE(r0.enteredSettings);
  TEST_ASSERT_EQUAL_UINT32(1000u, st.holdStartMs);

  auto r1 = settingsEntryGesture_update(st, 1799, 2, xs, ys, nullptr, back, fwd, kHold);
  TEST_ASSERT_FALSE(r1.enteredSettings);

  auto r2 = settingsEntryGesture_update(st, 1800, 2, xs, ys, nullptr, back, fwd, kHold);
  TEST_ASSERT_TRUE(r2.enteredSettings);
  TEST_ASSERT_TRUE(r2.suppressNextPlayTap);
  TEST_ASSERT_TRUE(st.needRelease);
}

void test_release_before_threshold_resets_timer() {
  IntRect back{};
  IntRect sel{};
  IntRect fwd{};
  settingsEntryGesture_computeBezelRects(320, 240, 34, &back, &sel, &fwd);

  SettingsEntryGestureState st{};
  int xs[2] = {back.x + 2, fwd.x + 2};
  int ys[2] = {back.y + 2, fwd.y + 2};
  constexpr uint32_t kHold = 800;

  settingsEntryGesture_update(st, 0, 2, xs, ys, nullptr, back, fwd, kHold);
  TEST_ASSERT_EQUAL_UINT32(0u, st.holdStartMs);

  settingsEntryGesture_update(st, 100, 2, xs, ys, nullptr, back, fwd, kHold);
  TEST_ASSERT_EQUAL_UINT32(100u, st.holdStartMs);

  settingsEntryGesture_update(st, 400, 0, xs, ys, nullptr, back, fwd, kHold);
  TEST_ASSERT_EQUAL_UINT32(0u, st.holdStartMs);
}

void test_need_release_clears_after_all_fingers_up() {
  IntRect back{};
  IntRect sel{};
  IntRect fwd{};
  settingsEntryGesture_computeBezelRects(320, 240, 34, &back, &sel, &fwd);

  SettingsEntryGestureState st{};
  int xs[2] = {back.x + 2, fwd.x + 2};
  int ys[2] = {back.y + 2, fwd.y + 2};

  settingsEntryGesture_update(st, 100, 2, xs, ys, nullptr, back, fwd, 800);
  auto r = settingsEntryGesture_update(st, 900, 2, xs, ys, nullptr, back, fwd, 800);
  TEST_ASSERT_TRUE(r.enteredSettings);
  TEST_ASSERT_TRUE(st.needRelease);

  settingsEntryGesture_update(st, 801, 2, xs, ys, nullptr, back, fwd, 800);
  TEST_ASSERT_TRUE(st.needRelease);

  settingsEntryGesture_update(st, 900, 0, xs, ys, nullptr, back, fwd, 800);
  TEST_ASSERT_FALSE(st.needRelease);
}

void test_pressed_mask_ignores_lifted_slots() {
  IntRect back{};
  IntRect sel{};
  IntRect fwd{};
  settingsEntryGesture_computeBezelRects(320, 240, 34, &back, &sel, &fwd);

  int xs[2] = {back.x + 2, fwd.x + 2};
  int ys[2] = {back.y + 2, fwd.y + 2};
  bool pressed[2] = {true, false};
  TEST_ASSERT_FALSE(settingsEntryGesture_touchesCoverBackAndFwd(2, xs, ys, pressed, back, fwd));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_back_fwd_requires_two_distinct_zones);
  RUN_TEST(test_hold_fires_after_threshold);
  RUN_TEST(test_release_before_threshold_resets_timer);
  RUN_TEST(test_need_release_clears_after_all_fingers_up);
  RUN_TEST(test_pressed_mask_ignores_lifted_slots);
  return UNITY_END();
}
