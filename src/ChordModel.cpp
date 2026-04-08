#include "ChordModel.h"

#include <stdio.h>
#include <string.h>

const char* ChordModel::kKeyNames[kKeyCount] = {
    "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

// Semitone intervals from root for major-scale degrees I–VII
static constexpr int kMajorIntervals[7] = {0, 2, 4, 5, 7, 9, 11};

// Qualities for each scale degree in major: maj, min, min, maj, maj, min, dim
static const char* kDiatonicQualities[7] = {
    "", "m", "m", "", "", "m", "dim"};

// Roles for degrees ii(1), iii(2), IV(3), V(4), vi(5), vii°(6)
// (degree I = index 0 is the tonic shown in center, not in surround)
static constexpr ChordRole kSurroundRoles[6] = {
    ChordRole::Standard,   // ii
    ChordRole::Standard,   // iii
    ChordRole::Principal,  // IV
    ChordRole::Principal,  // V
    ChordRole::Standard,   // vi
    ChordRole::Tension,    // vii°
};

struct SurpriseTemplate {
  int semitonesFromRoot;
  const char* quality;
};

// Surprise chords relative to root (modal interchange, secondary dominants, etc.)
static constexpr SurpriseTemplate kSurpriseTemplates[6] = {
    {1, ""},      // bII (Neapolitan)
    {3, ""},      // bIII (modal interchange from parallel minor)
    {5, "m"},     // iv (modal interchange)
    {8, ""},      // bVI (modal interchange)
    {10, ""},     // bVII (modal interchange)
    {2, "7"},     // V/V (secondary dominant, shown as a dominant 7th)
};

void ChordModel::chordNameFromRoot(int rootSemitone, const char* quality, char* out, size_t len) {
  int idx = ((rootSemitone % kKeyCount) + kKeyCount) % kKeyCount;
  snprintf(out, len, "%s%s", kKeyNames[idx], quality);
}

void ChordModel::buildDiatonic() {
  for (int i = 0; i < kSurroundCount; ++i) {
    int degree = i + 1;  // skip degree 0 (tonic)
    int semitone = (keyIndex + kMajorIntervals[degree]) % kKeyCount;
    chordNameFromRoot(semitone, kDiatonicQualities[degree], surround[i].name,
                      sizeof(surround[i].name));
    surround[i].role = kSurroundRoles[i];
  }
}

void ChordModel::buildSurprisePool() {
  for (int i = 0; i < kSurprisePoolSize; ++i) {
    int semitone =
        (keyIndex + kSurpriseTemplates[i].semitonesFromRoot) % kKeyCount;
    chordNameFromRoot(semitone, kSurpriseTemplates[i].quality,
                      surprisePool[i].name, sizeof(surprisePool[i].name));
    surprisePool[i].role = ChordRole::Surprise;
  }
  surpriseNext = 0;
}

void ChordModel::rebuildChords() {
  buildDiatonic();
  buildSurprisePool();
  playCount = 0;
  heartAvailable = false;
}

void ChordModel::cycleKeyForward() {
  keyIndex = (keyIndex + 1) % kKeyCount;
  rebuildChords();
}

void ChordModel::cycleKeyBackward() {
  keyIndex = (keyIndex - 1 + kKeyCount) % kKeyCount;
  rebuildChords();
}

const char* ChordModel::keyName() const { return kKeyNames[keyIndex]; }

void ChordModel::registerPlay() {
  ++playCount;
  if (playCount >= kPlaysForHeart) {
    heartAvailable = true;
  }
}

void ChordModel::consumeHeart() {
  heartAvailable = false;
  playCount = 0;
}

const ChordInfo& ChordModel::nextSurprise() {
  const ChordInfo& c = surprisePool[surpriseNext];
  surpriseNext = (surpriseNext + 1) % kSurprisePoolSize;
  return c;
}
