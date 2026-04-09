#include "DinMidiTransport.h"

#include "MidiIngress.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <HardwareSerial.h>
#endif

namespace {

constexpr uint32_t kDinMidiBaud = 31250U;
volatile bool g_dinReady = false;
volatile uint32_t g_dinRxBytes = 0;

#if defined(ARDUINO)
#ifndef M5CHORD_DIN_RX_PIN
#define M5CHORD_DIN_RX_PIN RX1
#endif
#ifndef M5CHORD_DIN_TX_PIN
#define M5CHORD_DIN_TX_PIN TX1
#endif
#endif

}  // namespace

void dinMidiInit() {
#if defined(ARDUINO)
  Serial1.begin(kDinMidiBaud, SERIAL_8N1, M5CHORD_DIN_RX_PIN, M5CHORD_DIN_TX_PIN);
  g_dinReady = true;
#else
  g_dinReady = false;
#endif
}

void dinMidiPoll() {}

bool dinMidiReady() { return g_dinReady; }

size_t dinMidiWrite(const uint8_t* bytes, size_t len) {
#if defined(ARDUINO)
  if (!bytes || len == 0 || !g_dinReady) return 0;
  return Serial1.write(bytes, len);
#else
  (void)bytes;
  (void)len;
  return 0;
#endif
}

uint32_t dinMidiRxBytesTotal() { return g_dinRxBytes; }

void dinMidiResetRxBytes() { g_dinRxBytes = 0; }

extern "C" size_t __attribute__((weak)) m5ChordDinMidiRead(uint8_t* dst, size_t cap) {
#if defined(ARDUINO)
  if (!dst || cap == 0 || !g_dinReady) return 0;
  size_t n = 0;
  while (n < cap && Serial1.available() > 0) {
    const int rb = Serial1.read();
    if (rb < 0) break;
    dst[n++] = static_cast<uint8_t>(rb);
  }
  g_dinRxBytes += static_cast<uint32_t>(n);
  return n;
#else
  (void)dst;
  (void)cap;
  return 0;
#endif
}
