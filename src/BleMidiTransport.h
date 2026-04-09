#pragma once

#include <stddef.h>
#include <stdint.h>

void bleMidiInit();
void bleMidiPoll();
size_t bleMidiWrite(const uint8_t* bytes, size_t len);
void bleMidiFlush();
bool bleMidiConnected();
uint32_t bleMidiRxBytesTotal();
void bleMidiResetRxBytes();
uint32_t bleMidiDecodeDropTotal();
uint32_t bleMidiDecodeDropTimestampTotal();
uint32_t bleMidiDecodeDropInvalidDataTotal();
uint32_t bleMidiDecodeDropTruncatedTotal();
