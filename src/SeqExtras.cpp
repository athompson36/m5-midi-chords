#include "SeqExtras.h"

#include <stdlib.h>

void seqExtrasInitDefaults(SeqExtras* e) {
  for (int L = 0; L < 3; ++L) {
    e->quantizePct[L] = 0;
    e->swingPct[L] = 0;
    e->chordRandPct[L] = 0;
    for (int i = 0; i < 16; ++i) {
      e->stepProb[L][i] = 100;
      e->stepClockDiv[L][i] = 0;
      e->arpEnabled[L][i] = 0;
      e->arpPattern[L][i] = 0;
      e->arpClockDiv[L][i] = 0;
    }
  }
}

uint32_t seqSwingIntervalAfterStep(uint32_t baseBeatMs, uint8_t swingPct, uint8_t stepJustPlayed) {
  if (swingPct == 0 || baseBeatMs == 0) return baseBeatMs;
  const float k = (static_cast<float>(swingPct) / 100.0f) * 0.28f;
  if ((stepJustPlayed & 1u) == 0u) {
    return static_cast<uint32_t>(static_cast<float>(baseBeatMs) * (1.0f - k));
  }
  return static_cast<uint32_t>(static_cast<float>(baseBeatMs) * (1.0f + k));
}

bool seqRollStepFires(uint8_t probPct) {
  if (probPct >= 100) return true;
  if (probPct == 0) return false;
  return (rand() % 100) < static_cast<int>(probPct);
}

int8_t seqChordOctaveJitter(uint8_t chordRandPct) {
  if (chordRandPct == 0) return 0;
  uint8_t maxOct = static_cast<uint8_t>((static_cast<unsigned>(chordRandPct) * 4U + 99U) / 100U);
  if (maxOct > 4) maxOct = 4;
  if (maxOct < 1) maxOct = 1;
  const int spread = 2 * static_cast<int>(maxOct) + 1;
  return static_cast<int8_t>(rand() % spread - static_cast<int>(maxOct));
}
