#pragma once

#include <stdint.h>

struct AppSettings {
  static constexpr uint8_t kRowCount = 5;

  uint8_t midiOutChannel = 1;
  uint8_t midiInChannel = 0;
  uint8_t brightnessPercent = 80;
  uint8_t outputVelocity = 100;

  void normalize();
  void cycleMidiOut();
  void cycleMidiIn();
  void cycleBrightness();
  void cycleVelocity();
};
