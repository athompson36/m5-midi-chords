#include "ChordModel.h"

#include "Arpeggio.h"

#include <stdio.h>
#include <string.h>

const char* ChordModel::kKeyNames[kKeyCount] = {
    "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

static const char* kModeLabels[] = {"Major",
                                    "Natural minor",
                                    "Harmonic minor",
                                    "Dorian",
                                    "Mixolydian"};

static const char* kModeShort[] = {"maj", "min", "harm", "dor", "mix"};

struct SurpriseTemplate {
  int semitonesFromRoot;
  const char* quality;
};

static constexpr SurpriseTemplate kSurpriseTemplates[6] = {
    {1, ""}, {3, ""}, {5, "m"}, {8, ""}, {10, ""}, {2, "7"}};

void ChordModel::chordNameFromRoot(int rootSemitone, const char* quality, char* out,
                                   size_t len) {
  int idx = ((rootSemitone % kKeyCount) + kKeyCount) % kKeyCount;
  snprintf(out, len, "%s%s", kKeyNames[idx], quality);
}

void ChordModel::setOneSurround(int idx, int semitoneFromTonic, const char* quality,
                                ChordRole role) {
  const int r = (keyIndex + semitoneFromTonic + kKeyCount) % kKeyCount;
  chordNameFromRoot(r, quality, surround[idx].name, sizeof(surround[idx].name));
  surround[idx].role = role;
  m_rootPc[idx] = static_cast<int8_t>(r);
  chordSpellFromQuality(quality, m_spellRel[idx], &m_spellCount[idx]);
}

void ChordModel::fillEighthChord() {
  const int b7 = (keyIndex + 10) % kKeyCount;
  if (mode == KeyMode::Mixolydian) {
    const int r = (keyIndex + 1) % kKeyCount;
    chordNameFromRoot(r, "", surround[7].name, sizeof(surround[7].name));
    m_rootPc[7] = static_cast<int8_t>(r);
  } else {
    chordNameFromRoot(b7, "", surround[7].name, sizeof(surround[7].name));
    m_rootPc[7] = static_cast<int8_t>(b7 % kKeyCount);
  }
  surround[7].role = ChordRole::Surprise;
  chordSpellFromQuality("", m_spellRel[7], &m_spellCount[7]);
}

void ChordModel::buildDiatonic() {
  switch (mode) {
    case KeyMode::Major: {
      static constexpr int iv[7] = {0, 2, 4, 5, 7, 9, 11};
      static const char* q[7] = {"", "m", "m", "", "", "m", "dim"};
      static constexpr ChordRole rr[7] = {
          ChordRole::Principal, ChordRole::Standard, ChordRole::Standard,
          ChordRole::Principal, ChordRole::Principal, ChordRole::Standard,
          ChordRole::Tension};
      for (int i = 0; i < 7; ++i) {
        setOneSurround(i, iv[i], q[i], rr[i]);
      }
      fillEighthChord();
      break;
    }
    case KeyMode::NaturalMinor: {
      static constexpr int iv[7] = {0, 2, 3, 5, 7, 8, 10};
      static const char* q[7] = {"m", "dim", "", "m", "m", "", ""};
      static constexpr ChordRole rr[7] = {
          ChordRole::Principal, ChordRole::Tension, ChordRole::Standard,
          ChordRole::Standard,  ChordRole::Standard, ChordRole::Principal,
          ChordRole::Principal};
      for (int i = 0; i < 7; ++i) {
        setOneSurround(i, iv[i], q[i], rr[i]);
      }
      fillEighthChord();
      break;
    }
    case KeyMode::HarmonicMinor: {
      static constexpr int iv[7] = {0, 2, 3, 5, 7, 8, 11};
      static const char* q[7] = {"m", "dim", "aug", "m", "", "", "dim"};
      static constexpr ChordRole rr[7] = {
          ChordRole::Principal, ChordRole::Tension, ChordRole::Surprise,
          ChordRole::Standard,  ChordRole::Principal, ChordRole::Principal,
          ChordRole::Tension};
      for (int i = 0; i < 7; ++i) {
        setOneSurround(i, iv[i], q[i], rr[i]);
      }
      fillEighthChord();
      break;
    }
    case KeyMode::Dorian: {
      static constexpr int iv[7] = {0, 2, 3, 5, 7, 9, 10};
      static const char* q[7] = {"m", "m", "", "", "m", "dim", ""};
      static constexpr ChordRole rr[7] = {
          ChordRole::Principal, ChordRole::Standard, ChordRole::Principal,
          ChordRole::Principal, ChordRole::Standard, ChordRole::Tension,
          ChordRole::Principal};
      for (int i = 0; i < 7; ++i) {
        setOneSurround(i, iv[i], q[i], rr[i]);
      }
      fillEighthChord();
      break;
    }
    case KeyMode::Mixolydian: {
      static constexpr int iv[7] = {0, 2, 4, 5, 7, 9, 10};
      static const char* q[7] = {"", "m", "dim", "", "m", "m", ""};
      static constexpr ChordRole rr[7] = {
          ChordRole::Principal, ChordRole::Standard, ChordRole::Tension,
          ChordRole::Principal, ChordRole::Standard, ChordRole::Standard,
          ChordRole::Principal};
      for (int i = 0; i < 7; ++i) {
        setOneSurround(i, iv[i], q[i], rr[i]);
      }
      fillEighthChord();
      break;
    }
    default:
      mode = KeyMode::Major;
      buildDiatonic();
      return;
  }
}

void ChordModel::buildSurprisePool() {
  for (int i = 0; i < kSurprisePoolSize; ++i) {
    int semitone =
        (keyIndex + kSurpriseTemplates[i].semitonesFromRoot + kKeyCount) % kKeyCount;
    chordNameFromRoot(semitone, kSurpriseTemplates[i].quality,
                      surprisePool[i].name, sizeof(surprisePool[i].name));
    surprisePool[i].role = ChordRole::Surprise;
    m_surpriseRootPc[i] = static_cast<int8_t>(semitone % kKeyCount);
    chordSpellFromQuality(kSurpriseTemplates[i].quality, m_spellSurprise[i],
                          &m_spellCountSurprise[i]);
  }
  surpriseNext = 0;
}

void ChordModel::rebuildChords() {
  buildDiatonic();
  buildSurprisePool();
  playCount = 0;
  heartAvailable = false;
}

void ChordModel::setTonicAndMode(int tonicIndex, KeyMode newMode) {
  keyIndex = (tonicIndex % kKeyCount + kKeyCount) % kKeyCount;
  if (newMode >= KeyMode::kCount) newMode = KeyMode::Major;
  mode = newMode;
  rebuildChords();
}

const char* ChordModel::keyName() const { return kKeyNames[keyIndex]; }

const char* ChordModel::modeLabel(KeyMode m) {
  const int i = static_cast<int>(m);
  if (i < 0 || i >= static_cast<int>(KeyMode::kCount)) return "?";
  return kModeLabels[i];
}

const char* ChordModel::modeShortLabel(KeyMode m) {
  const int i = static_cast<int>(m);
  if (i < 0 || i >= static_cast<int>(KeyMode::kCount)) return "?";
  return kModeShort[i];
}

void ChordModel::formatKeyCenterLine2(char* out, size_t len) const {
  snprintf(out, len, "%s", modeShortLabel(mode));
}

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

int8_t ChordModel::surroundChordRootPc(int idx) const {
  if (idx < 0 || idx >= kSurroundCount) return 0;
  return m_rootPc[idx];
}

void ChordModel::getSurroundSpell(int idx, int8_t out[6], uint8_t* n) const {
  if (!n || !out) return;
  if (idx < 0 || idx >= kSurroundCount) {
    *n = 0;
    return;
  }
  *n = m_spellCount[idx];
  for (uint8_t i = 0; i < m_spellCount[idx] && i < 6; ++i) {
    out[i] = m_spellRel[idx][i];
  }
}

int8_t ChordModel::surpriseChordRootPc(int poolIdx) const {
  if (poolIdx < 0 || poolIdx >= kSurprisePoolSize) return 0;
  return m_surpriseRootPc[poolIdx];
}

void ChordModel::getSurpriseSpell(int poolIdx, int8_t out[6], uint8_t* n) const {
  if (!n || !out) return;
  if (poolIdx < 0 || poolIdx >= kSurprisePoolSize) {
    *n = 0;
    return;
  }
  *n = m_spellCountSurprise[poolIdx];
  for (uint8_t i = 0; i < m_spellCountSurprise[poolIdx] && i < 6; ++i) {
    out[i] = m_spellSurprise[poolIdx][i];
  }
}
