#include <unity.h>

#include "MidiChordDetect.h"

void test_detects_major_triad() {
  const uint8_t notes[] = {60, 64, 67};
  char out[12];
  TEST_ASSERT_TRUE(midiDetectChordFromNotes(notes, 3, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("C", out);
}

void test_detects_minor_triad() {
  const uint8_t notes[] = {69, 72, 76};  // A C E
  char out[12];
  TEST_ASSERT_TRUE(midiDetectChordFromNotes(notes, 3, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("Am", out);
}

void test_detects_diminished_triad() {
  const uint8_t notes[] = {71, 74, 77};  // B D F
  char out[12];
  TEST_ASSERT_TRUE(midiDetectChordFromNotes(notes, 3, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("Bdim", out);
}

void test_detects_sus2_triad() {
  const uint8_t notes[] = {62, 64, 69};  // D E A
  char out[12];
  TEST_ASSERT_TRUE(midiDetectChordFromNotes(notes, 3, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("Dsus2", out);
}

void test_detects_sus4_triad() {
  const uint8_t notes[] = {64, 69, 71};  // E A B
  char out[12];
  TEST_ASSERT_TRUE(midiDetectChordFromNotes(notes, 3, out, sizeof(out)));
  TEST_ASSERT_EQUAL_STRING("Esus4", out);
}

void test_requires_triad() {
  const uint8_t notes[] = {60, 67};
  char out[12];
  TEST_ASSERT_FALSE(midiDetectChordFromNotes(notes, 2, out, sizeof(out)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_detects_major_triad);
  RUN_TEST(test_detects_minor_triad);
  RUN_TEST(test_detects_diminished_triad);
  RUN_TEST(test_detects_sus2_triad);
  RUN_TEST(test_detects_sus4_triad);
  RUN_TEST(test_requires_triad);
  return UNITY_END();
}
