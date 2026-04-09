#pragma once

#include <stdint.h>

/// Send MIDI Control Change on the given channel (0–15 = MIDI ch 1–16).
/// Implementation pending USB MIDI device stack; stub is a no-op for now.
void midiSendControlChange(uint8_t channel0_15, uint8_t cc, uint8_t value);
