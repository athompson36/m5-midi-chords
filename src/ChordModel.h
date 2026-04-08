#pragma once

#include <stddef.h>
#include <stdint.h>

enum class ChordRole : uint8_t {
  Principal,  // IV, V — green
  Standard,   // ii, iii, vi — blue/cyan
  Tension,    // vii° — red
  Surprise,   // modal interchange, secondary dominants, etc.
};

struct ChordInfo {
  char name[8];
  ChordRole role;
};

struct ChordModel {
  static constexpr int kKeyCount = 12;
  static constexpr int kSurroundCount = 6;
  static constexpr int kSurprisePoolSize = 6;
  static constexpr int kPlaysForHeart = 5;

  int keyIndex = 0;
  int playCount = 0;
  bool heartAvailable = false;

  ChordInfo surround[kSurroundCount];
  ChordInfo surprisePool[kSurprisePoolSize];
  int surpriseNext = 0;

  void cycleKeyForward();
  void cycleKeyBackward();
  const char* keyName() const;
  void rebuildChords();

  void registerPlay();
  void consumeHeart();
  const ChordInfo& nextSurprise();

  static const char* kKeyNames[kKeyCount];

private:
  void buildDiatonic();
  void buildSurprisePool();
  static void chordNameFromRoot(int rootSemitone, const char* quality, char* out, size_t len);
};
