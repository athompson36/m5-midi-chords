#include "MidiOut.h"

#include "Arpeggio.h"
#include "BleMidiTransport.h"
#include "ChordModel.h"
#include "DinMidiTransport.h"
#include "UsbMidiTransport.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace {

extern "C" size_t __attribute__((weak)) m5ChordMidiBroadcastWrite(const uint8_t* bytes, size_t len) {
  size_t wrote = 0;
  wrote += usbMidiWrite(bytes, len);
  wrote += bleMidiWrite(bytes, len);
  wrote += dinMidiWrite(bytes, len);
  return wrote;
}

void writeBytes(const uint8_t* bytes, size_t len) {
  if (!bytes || len == 0) return;
  (void)m5ChordMidiBroadcastWrite(bytes, len);
}

void write3(uint8_t a, uint8_t b, uint8_t c) {
  const uint8_t msg[3] = {a, b, c};
  writeBytes(msg, sizeof(msg));
}

void write2(uint8_t a, uint8_t b) {
  const uint8_t msg[2] = {a, b};
  writeBytes(msg, sizeof(msg));
}

uint8_t s_lastNotes[16][6]{};
uint8_t s_lastCount[16]{};
uint32_t s_txBytesTotal = 0;
uint32_t s_txMsgTotal = 0;
uint32_t s_txNoteOnTotal = 0;
uint32_t s_txNoteOffTotal = 0;
uint32_t s_txCcTotal = 0;

void silenceTracked(uint8_t ch) {
  const uint8_t c = ch & 0x0F;
  for (uint8_t i = 0; i < s_lastCount[c]; ++i) {
    const uint8_t note = static_cast<uint8_t>(s_lastNotes[c][i] & 0x7F);
    // Use NoteOn velocity=0 as release form for broad BLE synth compatibility.
    write3(static_cast<uint8_t>(0x90 | c), note, 0);
  }
  s_lastCount[c] = 0;
}

void trackNote(uint8_t ch, uint8_t note) {
  const uint8_t c = ch & 0x0F;
  if (s_lastCount[c] < 6) {
    s_lastNotes[c][s_lastCount[c]++] = note;
  }
}

}  // namespace

void midiSendControlChange(uint8_t channel0_15, uint8_t cc, uint8_t value) {
  ++s_txMsgTotal;
  s_txBytesTotal += 3U;
  ++s_txCcTotal;
  write3(static_cast<uint8_t>(0xB0 | (channel0_15 & 0x0F)), cc & 0x7F, value & 0x7F);
}

void midiSendProgramChange(uint8_t channel0_15, uint8_t program) {
  ++s_txMsgTotal;
  s_txBytesTotal += 2U;
  write2(static_cast<uint8_t>(0xC0 | (channel0_15 & 0x0F)), program & 0x7F);
}

void midiSendNoteOn(uint8_t channel0_15, uint8_t note, uint8_t velocity) {
  ++s_txMsgTotal;
  s_txBytesTotal += 3U;
  ++s_txNoteOnTotal;
  write3(static_cast<uint8_t>(0x90 | (channel0_15 & 0x0F)), note & 0x7F, velocity & 0x7F);
}

void midiSendNoteOnTracked(uint8_t channel0_15, uint8_t note, uint8_t velocity) {
  const uint8_t c = channel0_15 & 0x0F;
  midiSendNoteOn(c, note, velocity);
  trackNote(c, static_cast<uint8_t>(note & 0x7F));
}

void midiSendNoteOff(uint8_t channel0_15, uint8_t note) {
  ++s_txMsgTotal;
  s_txBytesTotal += 3U;
  ++s_txNoteOffTotal;
  write3(static_cast<uint8_t>(0x80 | (channel0_15 & 0x0F)), note & 0x7F, 0);
}

void midiSendAllNotesOff(uint8_t channel0_15) {
  midiSendControlChange(channel0_15, 123, 0);
}

void midiSilenceLastChord(uint8_t channel0_15) {
  silenceTracked(channel0_15);
}

static void playSpell(int rootMidi, const int8_t* rel, uint8_t n, uint8_t ch, uint8_t vel, uint8_t voicingMax,
                      uint8_t arpeggiatorMode, int8_t transposeSemi) {
  int8_t ordered[6]{};
  uint8_t oc = 0;
  arpeggiatorOrderedIntervals(arpeggiatorMode, rel, n, voicingMax, ordered, &oc);
  if (oc == 0) {
    return;
  }
  const uint8_t c = ch & 0x0F;
  for (uint8_t i = 0; i < oc; ++i) {
    int note = rootMidi + static_cast<int>(ordered[i]) + static_cast<int>(transposeSemi);
    if (note < 0) {
      note = 0;
    }
    if (note > 127) {
      note = 127;
    }
    const uint8_t nn = static_cast<uint8_t>(note);
    midiSendNoteOn(c, nn, vel);
    trackNote(c, nn);
  }
}

void midiPlaySurroundPad(const ChordModel& model, int surroundIdx, uint8_t channel0_15, uint8_t velocity,
                         uint8_t voicingMax, uint8_t arpeggiatorMode, int8_t transposeSemi) {
  silenceTracked(channel0_15);
  if (surroundIdx < 0 || surroundIdx >= ChordModel::kSurroundCount) {
    return;
  }
  int8_t rel[6]{};
  uint8_t n = 0;
  model.getSurroundSpell(surroundIdx, rel, &n);
  if (n == 0) {
    return;
  }
  const int rpc = model.surroundChordRootPc(surroundIdx);
  const int rootMidi = 48 + static_cast<int>(rpc);
  playSpell(rootMidi, rel, n, channel0_15, velocity, voicingMax, arpeggiatorMode, transposeSemi);
}

void midiPlaySurprisePad(const ChordModel& model, int poolIdx, uint8_t channel0_15, uint8_t velocity,
                         uint8_t voicingMax, uint8_t arpeggiatorMode, int8_t transposeSemi) {
  silenceTracked(channel0_15);
  if (poolIdx < 0 || poolIdx >= ChordModel::kSurprisePoolSize) {
    return;
  }
  int8_t rel[6]{};
  uint8_t n = 0;
  model.getSurpriseSpell(poolIdx, rel, &n);
  if (n == 0) {
    return;
  }
  const int rpc = model.surpriseChordRootPc(poolIdx);
  const int rootMidi = 48 + static_cast<int>(rpc);
  playSpell(rootMidi, rel, n, channel0_15, velocity, voicingMax, arpeggiatorMode, transposeSemi);
}

void midiPlayTonicChord(const ChordModel& model, uint8_t channel0_15, uint8_t velocity, uint8_t voicingMax,
                        uint8_t arpeggiatorMode, int8_t transposeSemi) {
  midiPlaySurroundPad(model, 0, channel0_15, velocity, voicingMax, arpeggiatorMode, transposeSemi);
}

uint32_t midiOutTxBytesTotal() {
  return s_txBytesTotal;
}

uint32_t midiOutTxMessageTotal() {
  return s_txMsgTotal;
}

uint32_t midiOutTxNoteOnTotal() {
  return s_txNoteOnTotal;
}

uint32_t midiOutTxNoteOffTotal() {
  return s_txNoteOffTotal;
}

uint32_t midiOutTxControlChangeTotal() {
  return s_txCcTotal;
}

void midiOutResetTxStats() {
  s_txBytesTotal = 0;
  s_txMsgTotal = 0;
  s_txNoteOnTotal = 0;
  s_txNoteOffTotal = 0;
  s_txCcTotal = 0;
}
