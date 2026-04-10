#pragma once

#include <stddef.h>
#include <stdint.h>

void dinMidiInit();
void dinMidiPoll();
bool dinMidiReady();
bool dinMidiRecentTraffic(uint32_t maxAgeMs);
size_t dinMidiWrite(const uint8_t* bytes, size_t len);
uint32_t dinMidiRxBytesTotal();
void dinMidiResetRxBytes();
