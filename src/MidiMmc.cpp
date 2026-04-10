#include "MidiMmc.h"

bool midiMmcParseRealtime(const uint8_t* p, uint16_t len, uint8_t* outCommand) {
  if (!p || !outCommand) return false;
  // Only the short real-time form (no extra data bytes before F7).
  if (len != 6) return false;
  if (p[0] != 0xF0 || p[1] != 0x7F || p[3] != 0x06 || p[5] != 0xF7) return false;
  // Device ID 0x00–0x7F (0x7F = all devices).
  *outCommand = p[4];
  return true;
}
