#pragma once

#include <stddef.h>
#include <stdint.h>

struct ChordModel;

/// Raw MIDI bytes on **USB CDC `Serial`** (ESP32 Arduino). Host sees a virtual COM port; use a
/// serial→MIDI bridge (e.g. Hairless) or future USB MIDI class stack for class-compliant devices.
void midiSendControlChange(uint8_t channel0_15, uint8_t cc, uint8_t value);
/// Raw SysEx (`F0` … `F7` inclusive). Broadcast to USB/BLE/DIN when wired.
void midiSendSysEx(const uint8_t* bytes, size_t len);
void midiSendProgramChange(uint8_t channel0_15, uint8_t program);
void midiSendNoteOn(uint8_t channel0_15, uint8_t note, uint8_t velocity);
void midiSendNoteOff(uint8_t channel0_15, uint8_t note);
void midiSendNoteOnTracked(uint8_t channel0_15, uint8_t note, uint8_t velocity);
void midiSendAllNotesOff(uint8_t channel0_15);
uint32_t midiOutTxBytesTotal();
uint32_t midiOutTxMessageTotal();
uint32_t midiOutTxNoteOnTotal();
uint32_t midiOutTxNoteOffTotal();
uint32_t midiOutTxControlChangeTotal();
void midiOutResetTxStats();

/// Stop notes from the last `midiPlaySurroundPad` / `midiPlaySurprisePad` / tonic play on this channel.
void midiSilenceLastChord(uint8_t channel0_15);

/// Play surround pad `0..7` using voicing depth `1..4`, global arpeggiator mode (`ArpeggiatorMode`),
/// and **transpose** (semitones, typically from `AppSettings::transposeSemitones`).
void midiPlaySurroundPad(const ChordModel& model, int surroundIdx, uint8_t channel0_15, uint8_t velocity,
                         uint8_t voicingMax, uint8_t arpeggiatorMode, int8_t transposeSemi);

/// Surprise pool index `0..kSurprisePoolSize-1` (use `surprisePeekIndex()` before `nextSurprise()`).
void midiPlaySurprisePad(const ChordModel& model, int poolIdx, uint8_t channel0_15, uint8_t velocity,
                         uint8_t voicingMax, uint8_t arpeggiatorMode, int8_t transposeSemi);

/// Tonic / center tap: same as **I** chord (surround index 0).
void midiPlayTonicChord(const ChordModel& model, uint8_t channel0_15, uint8_t velocity, uint8_t voicingMax,
                        uint8_t arpeggiatorMode, int8_t transposeSemi);
