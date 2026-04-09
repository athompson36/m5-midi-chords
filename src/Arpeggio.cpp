#include "Arpeggio.h"

#include <string.h>

const char* arpeggiatorModeLabel(uint8_t mode) {
  switch (mode) {
    case static_cast<uint8_t>(ArpeggiatorMode::ChordTones):
      return "Chord notes";
    case static_cast<uint8_t>(ArpeggiatorMode::VoicingArp):
      return "Voicing arp";
    default:
      return "?";
  }
}

void chordSpellFromQuality(const char* quality, int8_t outRel[6], uint8_t* outCount) {
  if (!outCount) return;
  *outCount = 0;
  if (!quality) quality = "";

  const bool has7 = strchr(quality, '7') != nullptr;
  if (has7) {
    const bool minorish = quality[0] == 'm';
    if (minorish) {
      *outCount = 4;
      outRel[0] = 0;
      outRel[1] = 3;
      outRel[2] = 7;
      outRel[3] = 10;
      return;
    }
    *outCount = 4;
    outRel[0] = 0;
    outRel[1] = 4;
    outRel[2] = 7;
    outRel[3] = 10;
    return;
  }

  if (strcmp(quality, "dim") == 0) {
    *outCount = 3;
    outRel[0] = 0;
    outRel[1] = 3;
    outRel[2] = 6;
    return;
  }
  if (strcmp(quality, "aug") == 0) {
    *outCount = 3;
    outRel[0] = 0;
    outRel[1] = 4;
    outRel[2] = 8;
    return;
  }
  if (strcmp(quality, "m") == 0) {
    *outCount = 3;
    outRel[0] = 0;
    outRel[1] = 3;
    outRel[2] = 7;
    return;
  }

  *outCount = 3;
  outRel[0] = 0;
  outRel[1] = 4;
  outRel[2] = 7;
}

void arpeggiatorOrderedIntervals(uint8_t mode, const int8_t* spellRel, uint8_t spellN,
                                 uint8_t voicing1_4, int8_t orderedOut[6], uint8_t* orderedCount) {
  if (!orderedCount || !orderedOut) return;
  *orderedCount = 0;
  if (!spellRel || spellN == 0) return;

  if (mode == static_cast<uint8_t>(ArpeggiatorMode::ChordTones)) {
    if (spellN > 6) spellN = 6;
    *orderedCount = spellN;
    for (uint8_t i = 0; i < spellN; ++i) {
      orderedOut[i] = spellRel[i];
    }
    return;
  }

  uint8_t v = voicing1_4;
  if (v < 1) v = 1;
  if (v > 4) v = 4;
  uint8_t take = v;
  if (take > spellN) take = spellN;
  if (take > 6) take = 6;
  *orderedCount = take;
  for (uint8_t i = 0; i < take; ++i) {
    orderedOut[i] = spellRel[i];
  }
}
