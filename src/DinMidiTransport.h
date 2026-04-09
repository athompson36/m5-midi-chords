#pragma once

#include <stddef.h>
#include <stdint.h>

void dinMidiInit();
void dinMidiPoll();
bool dinMidiReady();
uint32_t dinMidiRxBytesTotal();
void dinMidiResetRxBytes();
