#pragma once

#include <stdint.h>

/// Per-lane sequencer timing / performance parameters (NVS + SD project).
struct SeqExtras {
  /// Grid snap strength for future MIDI record quantize (0 = off, 100 = full step).
  uint8_t quantizePct[3];
  /// Swing 0–100; stretches odd/even step intervals (see seqSwingIntervalAfterStep).
  uint8_t swingPct[3];
  /// Random diatonic chord pick: 0 = off, 100 = up to ±4 octave spread around base chord.
  uint8_t chordRandPct[3];
  /// Per-step trigger probability 0–100 (100 = always).
  uint8_t stepProb[3][16];
};

void seqExtrasInitDefaults(SeqExtras* e);

/// Interval from after playing `stepJustPlayed` (0–15) until the next transport tick.
uint32_t seqSwingIntervalAfterStep(uint32_t baseBeatMs, uint8_t swingPct, uint8_t stepJustPlayed);

/// Bernoulli trial: true = step should fire (chord/MIDI).
bool seqRollStepFires(uint8_t probPct);

/// Random octave offset in [-maxOct..maxOct] from chordRandPct (max spread scales with pct).
int8_t seqChordOctaveJitter(uint8_t chordRandPct);
