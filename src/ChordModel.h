#pragma once

#include <stddef.h>

struct ChordModel {
  static constexpr int kChordCount = 8;
  const char* chords[kChordCount] = {"Cmaj7", "Am7", "Dm7", "G7", "Fmaj7", "Em7", "A7", "D7"};
  int selectedIndex = 0;

  void moveUp();
  void moveDown();
  const char* selectedChord() const;
};
