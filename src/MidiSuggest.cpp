#include "MidiSuggest.h"

#include "ChordModel.h"

#include <string.h>

namespace {

enum class QualityClass : uint8_t {
  Major,
  Minor,
  Diminished,
  Augmented,
  Suspended,
  Dominant,
  Other,
};

bool parseChordLabel(const char* chord, int* rootPc, QualityClass* qc) {
  if (!chord || !chord[0] || !rootPc || !qc) return false;
  const char c0 = chord[0];
  int base = -1;
  switch (c0) {
    case 'C':
      base = 0;
      break;
    case 'D':
      base = 2;
      break;
    case 'E':
      base = 4;
      break;
    case 'F':
      base = 5;
      break;
    case 'G':
      base = 7;
      break;
    case 'A':
      base = 9;
      break;
    case 'B':
      base = 11;
      break;
    default:
      return false;
  }
  const char* p = chord + 1;
  if (*p == '#') {
    base = (base + 1) % 12;
    ++p;
  } else if (*p == 'b') {
    base = (base + 11) % 12;
    ++p;
  }
  *rootPc = base;
  if (strcmp(p, "m") == 0) {
    *qc = QualityClass::Minor;
  } else if (strcmp(p, "dim") == 0) {
    *qc = QualityClass::Diminished;
  } else if (strcmp(p, "aug") == 0) {
    *qc = QualityClass::Augmented;
  } else if (strcmp(p, "sus2") == 0 || strcmp(p, "sus4") == 0) {
    *qc = QualityClass::Suspended;
  } else if (strcmp(p, "7") == 0 || strcmp(p, "maj7") == 0 || strcmp(p, "m7") == 0) {
    *qc = QualityClass::Dominant;
  } else if (p[0] == '\0') {
    *qc = QualityClass::Major;
  } else {
    *qc = QualityClass::Other;
  }
  return true;
}

int semitoneDistance(int a, int b) {
  int d = a - b;
  if (d < 0) d = -d;
  if (d > 6) d = 12 - d;
  return d;
}

int roleBonus(ChordRole role) {
  switch (role) {
    case ChordRole::Principal:
      return 16;
    case ChordRole::Standard:
      return 10;
    case ChordRole::Tension:
      return 6;
    case ChordRole::Surprise:
      return 4;
    default:
      return 0;
  }
}

}  // namespace

size_t midiRankSuggestions(const ChordModel& model, const char* detectedChord, MidiSuggestionItem* out,
                           size_t outCap) {
  if (!out || outCap == 0) return 0;

  int detRoot = 0;
  QualityClass detQ = QualityClass::Other;
  const bool haveDetected = parseChordLabel(detectedChord, &detRoot, &detQ);

  size_t written = 0;
  for (int i = 0; i < ChordModel::kSurroundCount && written < outCap; ++i) {
    const ChordInfo& c = model.surround[i];
    int candRoot = 0;
    QualityClass candQ = QualityClass::Other;
    const bool parsedCand = parseChordLabel(c.name, &candRoot, &candQ);

    int score = 20 + roleBonus(c.role);
    if (haveDetected) {
      if (strcmp(c.name, detectedChord) == 0) {
        score += 120;
      }
      if (parsedCand) {
        const int dist = semitoneDistance(detRoot, candRoot);
        score += (12 - dist) * 3;
        if (candRoot == detRoot) score += 30;
        if (((detRoot + 7) % 12) == candRoot || ((detRoot + 5) % 12) == candRoot) {
          score += 12;
        }
        if (candQ == detQ) score += 22;
      }
    }

    strncpy(out[written].name, c.name, sizeof(out[written].name) - 1);
    out[written].name[sizeof(out[written].name) - 1] = '\0';
    out[written].score = static_cast<int16_t>(score);
    ++written;
  }

  // Small fixed-size sort (descending score).
  for (size_t i = 0; i < written; ++i) {
    size_t best = i;
    for (size_t j = i + 1; j < written; ++j) {
      if (out[j].score > out[best].score) best = j;
    }
    if (best != i) {
      const MidiSuggestionItem t = out[i];
      out[i] = out[best];
      out[best] = t;
    }
  }
  return written;
}
