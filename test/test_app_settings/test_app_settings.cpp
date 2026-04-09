#include <unity.h>

#include "AppSettings.h"

void test_settings_row_count_matches_ui() {
  TEST_ASSERT_EQUAL_UINT8(17, AppSettings::kRowCount);
}

void test_normalize_clamps_channels() {
  AppSettings s;
  s.midiOutChannel = 0;
  s.midiInChannel = 20;
  s.midiTransportSend = 9;
  s.midiTransportReceive = 9;
  s.midiClockSource = 9;
  s.brightnessPercent = 3;
  s.outputVelocity = 200;
  s.normalize();
  TEST_ASSERT_EQUAL_UINT8(1, s.midiOutChannel);
  TEST_ASSERT_EQUAL_UINT8(16, s.midiInChannel);
  TEST_ASSERT_EQUAL_UINT8(0, s.midiTransportSend);
  TEST_ASSERT_EQUAL_UINT8(0, s.midiTransportReceive);
  TEST_ASSERT_EQUAL_UINT8(0, s.midiClockSource);
  TEST_ASSERT_EQUAL_UINT8(10, s.brightnessPercent);
  TEST_ASSERT_EQUAL_UINT8(127, s.outputVelocity);
}

void test_cycle_midi_out_wraps() {
  AppSettings s;
  s.midiOutChannel = 16;
  s.cycleMidiOut();
  TEST_ASSERT_EQUAL_UINT8(1, s.midiOutChannel);
}

void test_cycle_midi_in_includes_omni() {
  AppSettings s;
  s.midiInChannel = 16;
  s.cycleMidiIn();
  TEST_ASSERT_EQUAL_UINT8(0, s.midiInChannel);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_settings_row_count_matches_ui);
  RUN_TEST(test_normalize_clamps_channels);
  RUN_TEST(test_cycle_midi_out_wraps);
  RUN_TEST(test_cycle_midi_in_includes_omni);
  return UNITY_END();
}
