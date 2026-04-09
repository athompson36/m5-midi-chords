#pragma once

#include <stdint.h>

/// How the arpeggiator orders notes when MIDI note output is active.
/// ChordTones: full chord (triad or 7th from quality) in close position.
/// VoicingArp: only the lowest N chord tones where N = voicing depth (1–4).
enum class ArpeggiatorMode : uint8_t {
  ChordTones = 0,
  VoicingArp = 1,
  kCount = 2,
};

constexpr uint8_t kArpeggiatorModeCount = 2;

const char* arpeggiatorModeLabel(uint8_t mode);

/// Intervals in semitones above chord root (ascending). Uses quality from ChordModel ("", "m", "dim", "aug", "7", …).
void chordSpellFromQuality(const char* quality, int8_t outRel[6], uint8_t* outCount);

/// Fills `orderedOut` with the intervals to arpeggiate in order (low → high in one octave).
/// `voicing1_4` is only used when mode == VoicingArp (number of chord tones from the bass).
void arpeggiatorOrderedIntervals(uint8_t mode, const int8_t* spellRel, uint8_t spellN,
                                 uint8_t voicing1_4, int8_t orderedOut[6], uint8_t* orderedCount);
