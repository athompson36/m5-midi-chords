#pragma once

#include <stdint.h>

struct AppSettings {
  uint8_t midiOutChannel = 1;
  uint8_t midiInChannel = 0;
  /// Where MIDI clock / start / stop / SPP are sent (0=Off, 1=USB, 2=Bluetooth, 3=DIN).
  uint8_t midiTransportSend = 1;
  /// Where external transport is received (0=Off — use internal clock only).
  uint8_t midiTransportReceive = 0;
  /// Master clock source: 0=Internal BPM, 1=USB, 2=Bluetooth, 3=DIN.
  uint8_t midiClockSource = 0;
  uint8_t brightnessPercent = 80;
  uint8_t outputVelocity = 100;
  /// 0 = chord tones, 1 = voicing-limited arp (see Arpeggio.h).
  uint8_t arpeggiatorMode = 0;

  void normalize();
  void cycleMidiOut();
  void cycleMidiIn();
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
