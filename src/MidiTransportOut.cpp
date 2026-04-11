#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

static bool s_transportTxPrevPlaying = false;
static bool s_transportTxPrevRunning = false;
static uint32_t s_transportTxNextClockMs = 0;

static void midiRouteWriteBytes(uint8_t route, const uint8_t* bytes, size_t len) {
  if (!bytes || len == 0) return;
  // route: 1=USB, 2=BLE, 3=DIN (matches Transport dropdown; 0 = off).
  switch (route) {
    case 1:
      (void)usbMidiWrite(bytes, len);
      break;
    case 2:
      (void)bleMidiWrite(bytes, len);
      break;
    case 3:
      (void)dinMidiWrite(bytes, len);
      break;
    default:
      break;
  }
}

static void midiRouteWriteRealtime(uint8_t route, uint8_t status) {
  const uint8_t b = status;
  midiRouteWriteBytes(route, &b, 1);
}

void transportSendTickIfNeeded(uint32_t nowMs) {
  const uint8_t route = g_settings.midiTransportSend;
  if (route == 0 || route > 3) return;
  // While following external clock, do not echo generated clock back out.
  if (transportExternalClockActive()) return;

  const bool playing = transportIsPlaying();
  const bool running = transportIsRunning();
  if (running && !s_transportTxPrevRunning) {
    s_transportTxNextClockMs = nowMs;
  }
  if (playing && !s_transportTxPrevPlaying) {
    seqArpStopAll(true);
    // Start from step-zero, Continue otherwise.
    const uint8_t st = (transportPlayhead() == 0) ? 0xFAU : 0xFBU;
    midiRouteWriteRealtime(route, st);
    s_transportTxNextClockMs = nowMs;
  } else if (!playing && s_transportTxPrevPlaying) {
    midiRouteWriteRealtime(route, 0xFCU);
  }
  s_transportTxPrevPlaying = playing;
  s_transportTxPrevRunning = running;

  if (!playing) return;
  uint16_t bpm = g_projectBpm;
  if (bpm < 40U) bpm = 40U;
  if (bpm > 300U) bpm = 300U;
  uint16_t crNum = 1;
  uint16_t crDen = 1;
  midiClockRateToRatio(g_settings.midiClockRateIndex, &crNum, &crDen);
  uint32_t tickMs = 60000UL / (static_cast<uint32_t>(bpm) * 24UL);
  tickMs = tickMs * crDen / crNum;
  if (s_transportTxNextClockMs == 0) s_transportTxNextClockMs = nowMs;
  while (static_cast<int32_t>(nowMs - s_transportTxNextClockMs) >= 0) {
    midiRouteWriteRealtime(route, 0xF8U);
    s_transportTxNextClockMs += (tickMs == 0 ? 1U : tickMs);
  }
}

}  // namespace m5chords_app
