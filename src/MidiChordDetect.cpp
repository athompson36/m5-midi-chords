#include "MidiChordDetect.h"

#include "ChordModel.h"

#include <stdio.h>
#include <string.h>

namespace {

struct Triad {
  uint8_t i1;
  uint8_t i2;
  const char* suffix;
};

constexpr Triad kTriads[] = {
    {4, 7, ""},     // major
    {3, 7, "m"},    // minor
    {2, 7, "sus2"}, // suspended 2
    {5, 7, "sus4"}, // suspended 4
    {3, 6, "dim"},  // diminished
    {4, 8, "aug"},  // augmented
};

}  // namespace

bool midiDetectChordFromNotes(const uint8_t* notes, size_t count, char* out, size_t outLen) {
  if (!out || outLen == 0) return false;
  out[0] = '\0';
  if (!notes || count < 3) return false;

  bool pcs[12] = {};
  for (size_t i = 0; i < count; ++i) {
    pcs[notes[i] % 12] = true;
  }

  for (uint8_t root = 0; root < 12; ++root) {
    if (!pcs[root]) continue;
    for (size_t t = 0; t < sizeof(kTriads) / sizeof(kTriads[0]); ++t) {
      const uint8_t p1 = static_cast<uint8_t>((root + kTriads[t].i1) % 12);
      const uint8_t p2 = static_cast<uint8_t>((root + kTriads[t].i2) % 12);
      if (pcs[p1] && pcs[p2]) {
        snprintf(out, outLen, "%s%s", ChordModel::kKeyNames[root], kTriads[t].suffix);
        return true;
      }
    }
  }
  return false;
}
