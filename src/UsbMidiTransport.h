#pragma once

#include <stddef.h>
#include <stdint.h>

void usbMidiInit();
void usbMidiPoll();
bool usbMidiReady();
bool usbMidiHostConnected();
size_t usbMidiWrite(const uint8_t* bytes, size_t len);
size_t usbMidiRead(uint8_t* dst, size_t cap);
