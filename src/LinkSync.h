#pragma once

#include <stdint.h>

/// Placeholder for Ableton Link on ESP32 (requires WiFi, peer sync, and a ported Link core).
void linkSyncInit();
void linkSyncPoll(uint32_t nowMs);
