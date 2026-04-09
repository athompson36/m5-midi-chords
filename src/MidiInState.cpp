#include "MidiInState.h"

#include <string.h>

int MidiInState::sourceIndex(MidiSource source) {
  switch (source) {
    case MidiSource::Usb:
      return 0;
    case MidiSource::Ble:
      return 1;
    case MidiSource::Din:
      return 2;
    default:
      return -1;
  }
}

void MidiInState::reset() {
  memset(noteOn_, 0, sizeof(noteOn_));
  memset(activeCount_, 0, sizeof(activeCount_));
}

void MidiInState::clearChannel(int srcIdx, uint8_t ch) {
  if (srcIdx < 0 || srcIdx > 2 || ch > 15) return;
  memset(noteOn_[srcIdx][ch], 0, sizeof(noteOn_[srcIdx][ch]));
  activeCount_[srcIdx][ch] = 0;
}

void MidiInState::onEvent(const MidiEvent& ev) {
  const int s = sourceIndex(ev.source);
  if (s < 0 || ev.channel > 15) return;

  if (ev.type == MidiEventType::ControlChange) {
    // CC120/123 panic behavior: clear channel active notes map.
    if (ev.data1 == 120 || ev.data1 == 123) {
      clearChannel(s, ev.channel);
    }
    return;
  }

  if (ev.type != MidiEventType::NoteOn && ev.type != MidiEventType::NoteOff) {
    return;
  }
  if (ev.data1 > 127) return;

  bool on = (ev.type == MidiEventType::NoteOn) && (ev.data2 != 0);
  bool& slot = noteOn_[s][ev.channel][ev.data1];
  if (on) {
    if (!slot && activeCount_[s][ev.channel] < 127) {
      activeCount_[s][ev.channel]++;
    }
    slot = true;
    return;
  }

  if (slot && activeCount_[s][ev.channel] > 0) {
    activeCount_[s][ev.channel]--;
  }
  slot = false;
}

uint8_t MidiInState::activeNotesOnChannel(MidiSource source, uint8_t channel0_15) const {
  const int s = sourceIndex(source);
  if (s < 0 || channel0_15 > 15) return 0;
  return activeCount_[s][channel0_15];
}

bool MidiInState::isNoteActive(MidiSource source, uint8_t channel0_15, uint8_t note0_127) const {
  const int s = sourceIndex(source);
  if (s < 0 || channel0_15 > 15 || note0_127 > 127) return false;
  return noteOn_[s][channel0_15][note0_127];
}

void MidiInState::collectActiveNotes(MidiSource source, uint8_t channel0_15, uint8_t* out, size_t cap,
                                     size_t* outCount) const {
  size_t n = 0;
  if (outCount) *outCount = 0;
  if (!out || cap == 0) return;
  const int s = sourceIndex(source);
  if (s < 0 || channel0_15 > 15) return;
  for (uint16_t note = 0; note <= 127 && n < cap; ++note) {
    if (noteOn_[s][channel0_15][note]) {
      out[n++] = static_cast<uint8_t>(note);
    }
  }
  if (outCount) *outCount = n;
}
