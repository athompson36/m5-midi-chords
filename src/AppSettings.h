#pragma once

#include <stdint.h>

struct AppSettings {
  uint8_t midiOutChannel = 1;
  /// Legacy single IN channel (0=OMNI). Kept for migration compatibility.
  uint8_t midiInChannel = 0;
  uint8_t midiInChannelUsb = 0;
  uint8_t midiInChannelBle = 0;
  uint8_t midiInChannelDin = 0;
  /// Where MIDI clock / start / stop / SPP are sent (0=Off, 1=USB, 2=Bluetooth, 3=DIN).
  uint8_t midiTransportSend = 1;
  /// Where external transport is received (0=Off — use internal clock only).
  uint8_t midiTransportReceive = 0;
  /// MIDI thru source mask: bit0=USB, bit1=BLE, bit2=DIN.
  uint8_t midiThruMask = 0;
  /// Master clock source: 0=Internal BPM, 1=USB, 2=Bluetooth, 3=DIN.
  uint8_t midiClockSource = 0;
  /// 0=ignore external MIDI clock, 1=follow selected source.
  uint8_t clkFollow = 1;
  /// 0=quarter-note flash, 1=eighth-note flash for clock indicator.
  uint8_t clkFlashEdge = 0;
  /// Settings/UI gate for MIDI debug screen.
  uint8_t midiDebugEnabled = 1;
  /// Show compact MIDI-IN activity row on Play screen.
  uint8_t playInMonitor = 0;
  /// Play IN monitor clock hold preset: 0=short, 1=medium, 2=long.
  uint8_t playInClockHold = 1;
  /// Play IN monitor verbosity: 0=compact, 1=detailed.
  uint8_t playInMonitorMode = 0;
  /// BLE debug peak auto-decay: 0=off, 1=5s, 2=10s, 3=30s.
  uint8_t bleDebugPeakDecay = 2;
  /// Panic debug rotation speed: 0=off, 1=1s, 2=2s, 3=4s.
  uint8_t panicDebugRotate = 2;
  /// Suggestion stability window preset: 0=0.6s, 1=1.0s, 2=1.4s, 3=2.0s, 4=3.0s.
  uint8_t suggestStableWindow = 2;
  /// Suggestion hysteresis score-gap preset: 0=8, 1=12, 2=18, 3=24, 4=32.
  uint8_t suggestStableGap = 2;
  /// Suggestion profile: 0=A (balanced), 1=B (sticky).
  uint8_t suggestProfile = 0;
  /// Lock manual window/gap edits while profiling: 0=off, 1=on.
  uint8_t suggestProfileLock = 0;
  uint8_t brightnessPercent = 80;
  uint8_t outputVelocity = 100;
  /// Output velocity curve: 0=linear, 1=soft, 2=hard.
  uint8_t velocityCurve = 0;
  /// Global swing amount (0..100) applied by transport timing.
  uint8_t globalSwingPct = 0;
  /// Global timing humanize amount (0..100) applied by transport timing.
  uint8_t globalHumanizePct = 0;
  /// X-Y release policy: 0=hold last value, 1=return center on release.
  uint8_t xyReturnToCenter = 0;
  /// Settings-entry (BACK+FWD) long-hold preset.
  uint8_t settingsEntryHoldPreset = 2;
  /// Sequencer step clear long-press preset.
  uint8_t seqLongPressPreset = 2;
  /// Transport BPM tap area long-press preset (tap-tempo entry).
  uint8_t bpmHoldPreset = 1;
  /// 0 = chord tones, 1 = voicing-limited arp (see Arpeggio.h).
  uint8_t arpeggiatorMode = 0;
  /// Global MIDI transpose in semitones (−24…+24).
  int8_t transposeSemitones = 0;

  void normalize();
  void cycleMidiOut();
  void cycleMidiInUsb();
  void cycleMidiTransportSend();
  void cycleMidiTransportReceive();
  void cycleMidiClockSource();
  void cycleBrightness();
  void cycleVelocity();
  void cycleArpeggiatorMode();
};

/// Labels for 0–3: Off/USB/BT/DIN (transport routing).
const char* midiTransportRouteLabel(uint8_t v);
/// Labels for 0–3: Internal/USB/BT/DIN (clock master).
const char* midiClockSourceLabel(uint8_t v);
