#pragma once

#include <stddef.h>
#include <stdint.h>

#include "MidiIngress.h"

class MidiInState {
 public:
  void reset();
  void onEvent(const MidiEvent& ev);

  uint8_t activeNotesOnChannel(MidiSource source, uint8_t channel0_15) const;
  bool isNoteActive(MidiSource source, uint8_t channel0_15, uint8_t note0_127) const;
  void collectActiveNotes(MidiSource source, uint8_t channel0_15, uint8_t* out, size_t cap,
                          size_t* outCount) const;

 private:
  static int sourceIndex(MidiSource source);
  void clearChannel(int srcIdx, uint8_t ch);

  bool noteOn_[3][16][128] = {};
  uint8_t activeCount_[3][16] = {};
};
