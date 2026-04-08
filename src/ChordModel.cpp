#include "ChordModel.h"

void ChordModel::moveUp() {
  selectedIndex = (selectedIndex - 1 + kChordCount) % kChordCount;
}

void ChordModel::moveDown() {
  selectedIndex = (selectedIndex + 1) % kChordCount;
}

const char* ChordModel::selectedChord() const {
  return chords[selectedIndex];
}
