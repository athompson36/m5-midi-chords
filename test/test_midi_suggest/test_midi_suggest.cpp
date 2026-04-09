#include <unity.h>

#include "ChordModel.h"
#include "MidiSuggest.h"

void test_exact_detected_chord_ranks_first() {
  ChordModel m;
  m.setTonicAndMode(0, KeyMode::Major);

  MidiSuggestionItem out[ChordModel::kSurroundCount]{};
  const size_t n = midiRankSuggestions(m, "C", out, ChordModel::kSurroundCount);
  TEST_ASSERT_GREATER_THAN_UINT32(0, static_cast<uint32_t>(n));
  TEST_ASSERT_EQUAL_STRING("C", out[0].name);
}

void test_returns_ranked_list_even_for_unknown_detected() {
  ChordModel m;
  m.setTonicAndMode(0, KeyMode::Major);

  MidiSuggestionItem out[ChordModel::kSurroundCount]{};
  const size_t n = midiRankSuggestions(m, "???", out, ChordModel::kSurroundCount);
  TEST_ASSERT_EQUAL_UINT32(ChordModel::kSurroundCount, static_cast<uint32_t>(n));
  TEST_ASSERT_TRUE(out[0].name[0] != '\0');
}

void test_scores_are_descending() {
  ChordModel m;
  m.setTonicAndMode(0, KeyMode::Major);

  MidiSuggestionItem out[ChordModel::kSurroundCount]{};
  const size_t n = midiRankSuggestions(m, "Am", out, ChordModel::kSurroundCount);
  TEST_ASSERT_GREATER_THAN_UINT32(1, static_cast<uint32_t>(n));
  for (size_t i = 1; i < n; ++i) {
    TEST_ASSERT_TRUE(out[i - 1].score >= out[i].score);
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_exact_detected_chord_ranks_first);
  RUN_TEST(test_returns_ranked_list_even_for_unknown_detected);
  RUN_TEST(test_scores_are_descending);
  return UNITY_END();
}
