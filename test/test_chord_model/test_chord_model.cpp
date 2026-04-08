#include <unity.h>

#include "ChordModel.h"
#include <string.h>

void test_initial_key_is_c() {
  ChordModel m;
  m.rebuildChords();
  TEST_ASSERT_EQUAL_STRING("C", m.keyName());
}

void test_cycle_key_forward_wraps() {
  ChordModel m;
  m.keyIndex = ChordModel::kKeyCount - 1;
  m.cycleKeyForward();
  TEST_ASSERT_EQUAL(0, m.keyIndex);
  TEST_ASSERT_EQUAL_STRING("C", m.keyName());
}

void test_cycle_key_backward_wraps() {
  ChordModel m;
  m.keyIndex = 0;
  m.cycleKeyBackward();
  TEST_ASSERT_EQUAL(ChordModel::kKeyCount - 1, m.keyIndex);
  TEST_ASSERT_EQUAL_STRING("B", m.keyName());
}

void test_rebuild_fills_six_surround_chords() {
  ChordModel m;
  m.rebuildChords();
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    TEST_ASSERT_TRUE(strlen(m.surround[i].name) > 0);
  }
}

void test_key_of_c_diatonic_names() {
  ChordModel m;
  m.keyIndex = 0;  // C
  m.rebuildChords();
  TEST_ASSERT_EQUAL_STRING("Dm", m.surround[0].name);   // ii
  TEST_ASSERT_EQUAL_STRING("Em", m.surround[1].name);   // iii
  TEST_ASSERT_EQUAL_STRING("F", m.surround[2].name);    // IV
  TEST_ASSERT_EQUAL_STRING("G", m.surround[3].name);    // V
  TEST_ASSERT_EQUAL_STRING("Am", m.surround[4].name);   // vi
  TEST_ASSERT_EQUAL_STRING("Bdim", m.surround[5].name); // vii°
}

void test_surround_roles() {
  ChordModel m;
  m.rebuildChords();
  TEST_ASSERT_EQUAL((int)ChordRole::Standard,  (int)m.surround[0].role);  // ii
  TEST_ASSERT_EQUAL((int)ChordRole::Standard,  (int)m.surround[1].role);  // iii
  TEST_ASSERT_EQUAL((int)ChordRole::Principal, (int)m.surround[2].role);  // IV
  TEST_ASSERT_EQUAL((int)ChordRole::Principal, (int)m.surround[3].role);  // V
  TEST_ASSERT_EQUAL((int)ChordRole::Standard,  (int)m.surround[4].role);  // vi
  TEST_ASSERT_EQUAL((int)ChordRole::Tension,   (int)m.surround[5].role);  // vii°
}

void test_heart_after_five_plays() {
  ChordModel m;
  m.rebuildChords();
  TEST_ASSERT_FALSE(m.heartAvailable);
  for (int i = 0; i < ChordModel::kPlaysForHeart; ++i) {
    m.registerPlay();
  }
  TEST_ASSERT_TRUE(m.heartAvailable);
}

void test_consume_heart_resets() {
  ChordModel m;
  m.rebuildChords();
  for (int i = 0; i < ChordModel::kPlaysForHeart; ++i) m.registerPlay();
  TEST_ASSERT_TRUE(m.heartAvailable);
  m.consumeHeart();
  TEST_ASSERT_FALSE(m.heartAvailable);
  TEST_ASSERT_EQUAL(0, m.playCount);
}

void test_surprise_pool_filled() {
  ChordModel m;
  m.rebuildChords();
  for (int i = 0; i < ChordModel::kSurprisePoolSize; ++i) {
    TEST_ASSERT_TRUE(strlen(m.surprisePool[i].name) > 0);
    TEST_ASSERT_EQUAL((int)ChordRole::Surprise, (int)m.surprisePool[i].role);
  }
}

void test_surprise_round_robin() {
  ChordModel m;
  m.rebuildChords();
  const char* first = m.nextSurprise().name;
  const char* second = m.nextSurprise().name;
  TEST_ASSERT_TRUE(strlen(first) > 0);
  TEST_ASSERT_TRUE(strlen(second) > 0);
}

void test_key_change_rebuilds() {
  ChordModel m;
  m.keyIndex = 0;
  m.rebuildChords();
  const char* cIV = m.surround[2].name;  // F in C
  TEST_ASSERT_EQUAL_STRING("F", cIV);

  m.cycleKeyForward();  // now Db
  TEST_ASSERT_EQUAL_STRING("Db", m.keyName());
  TEST_ASSERT_EQUAL_STRING("F#", m.surround[2].name);  // IV of Db (enharmonic Gb = F#)
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_initial_key_is_c);
  RUN_TEST(test_cycle_key_forward_wraps);
  RUN_TEST(test_cycle_key_backward_wraps);
  RUN_TEST(test_rebuild_fills_six_surround_chords);
  RUN_TEST(test_key_of_c_diatonic_names);
  RUN_TEST(test_surround_roles);
  RUN_TEST(test_heart_after_five_plays);
  RUN_TEST(test_consume_heart_resets);
  RUN_TEST(test_surprise_pool_filled);
  RUN_TEST(test_surprise_round_robin);
  RUN_TEST(test_key_change_rebuilds);
  return UNITY_END();
}
