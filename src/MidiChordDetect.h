#pragma once

#include <stddef.h>
#include <stdint.h>

/// Detect a simple triad class from active MIDI notes.
/// Returns true when a supported chord is detected and writes a short label into `out`.
/// Supported qualities: major, minor, diminished, augmented.
bool midiDetectChordFromNotes(const uint8_t* notes, size_t count, char* out, size_t outLen);
