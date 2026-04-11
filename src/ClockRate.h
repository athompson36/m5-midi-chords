#pragma once

#include <stdint.h>

/// Maps `AppSettings::midiClockRateIndex` to a rational multiplier (num/den) applied to internal
/// timing and outgoing MIDI clock. External MIDI clock follow stays unscaled.
inline void midiClockRateToRatio(uint8_t idx, uint16_t* num, uint16_t* den) {
  if (idx > 4) idx = 2;
  switch (idx) {
    case 0:
      *num = 1;
      *den = 4;
      break;  // x0.25
    case 1:
      *num = 1;
      *den = 2;
      break;  // x0.5
    case 2:
      *num = 1;
      *den = 1;
      break;  // x1
    case 3:
      *num = 2;
      *den = 1;
      break;  // x2
    case 4:
      *num = 4;
      *den = 1;
      break;  // x4
    default:
      *num = 1;
      *den = 1;
      break;
  }
}
