#pragma once

#include <stdint.h>

/// MIDI Machine Control — **Real Time** single-command SysEx (RP-013 style):
/// `F0 7F <deviceId> 06 <command> F7` (6 bytes). Longer MMC messages are ignored here.
///
/// Returns true when `p`/`len` match that frame; `*outCommand` is the MMC command byte
/// (e.g. 0x01 Stop, 0x02 Play, 0x03 Deferred Play, 0x09 Pause).

bool midiMmcParseRealtime(const uint8_t* p, uint16_t len, uint8_t* outCommand);
