#pragma once

#include <stddef.h>
#include <stdint.h>

enum class ChordRole : uint8_t {
  Principal,  // I, IV, V — green
  Standard,   // ii, iii, vi — blue/cyan
  Tension,    // vii°, etc. — red
  Surprise,   // borrowed / color
};

enum class KeyMode : uint8_t {
  Major = 0,
  NaturalMinor,
  HarmonicMinor,
  Dorian,
  Mixolydian,
  kCount,
};

struct ChordInfo {
  char name[8];
  ChordRole role;
};

struct ChordModel {
  static constexpr int kKeyCount = 12;
  static constexpr int kSurroundCount = 8;
  static constexpr int kSurprisePoolSize = 6;
  static constexpr int kPlaysForHeart = 5;

  int keyIndex = 0;
  KeyMode mode = KeyMode::Major;
  int playCount = 0;
  bool heartAvailable = false;

  ChordInfo surround[kSurroundCount];
  ChordInfo surprisePool[kSurprisePoolSize];
  int surpriseNext = 0;

  void setTonicAndMode(int tonicIndex, KeyMode newMode);
  void rebuildChords();

  const char* keyName() const;
  static const char* modeLabel(KeyMode m);
  static const char* modeShortLabel(KeyMode m);
  void formatKeyCenterLine2(char* out, size_t len) const;

  void registerPlay();
  void consumeHeart();
  /// Pool index used for the next `nextSurprise()` (before it advances).
  int surprisePeekIndex() const { return surpriseNext; }
  const ChordInfo& nextSurprise();

  /// Chord root pitch class 0–11 for surround pad (for MIDI / arpeggiator).
  int8_t surroundChordRootPc(int idx) const;
  /// Intervals from chord root for this pad (see ArpeggiatorMode).
  void getSurroundSpell(int idx, int8_t out[6], uint8_t* n) const;

  int8_t surpriseChordRootPc(int poolIdx) const;
  void getSurpriseSpell(int poolIdx, int8_t out[6], uint8_t* n) const;

  static const char* kKeyNames[kKeyCount];

private:
  void buildDiatonic();
  void buildSurprisePool();
  void setOneSurround(int idx, int semitoneFromTonic, const char* quality, ChordRole role);
  void fillEighthChord();
  static void chordNameFromRoot(int rootSemitone, const char* quality, char* out, size_t len);

  int8_t m_rootPc[kSurroundCount]{};
  int8_t m_spellRel[kSurroundCount][6]{};
  uint8_t m_spellCount[kSurroundCount]{};

  int8_t m_surpriseRootPc[kSurprisePoolSize]{};
  int8_t m_spellSurprise[kSurprisePoolSize][6]{};
  uint8_t m_spellCountSurprise[kSurprisePoolSize]{};
};
