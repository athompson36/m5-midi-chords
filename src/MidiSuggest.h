#pragma once

#include <stddef.h>
#include <stdint.h>

struct ChordModel;

struct MidiSuggestionItem {
  char name[8];
  int16_t score;
};

/// Rank current-key chord suggestions from a detected incoming chord label.
/// Returns number of items written to `out`.
size_t midiRankSuggestions(const ChordModel& model, const char* detectedChord, MidiSuggestionItem* out,
                           size_t outCap);
