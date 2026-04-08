#include "AppSettings.h"

void AppSettings::normalize() {
  if (midiOutChannel < 1) midiOutChannel = 1;
  if (midiOutChannel > 16) midiOutChannel = 16;
  if (midiInChannel > 16) midiInChannel = 16;
  if (brightnessPercent < 10) brightnessPercent = 10;
  if (brightnessPercent > 100) brightnessPercent = 100;
  if (outputVelocity < 40) outputVelocity = 40;
  if (outputVelocity > 127) outputVelocity = 127;
}

void AppSettings::cycleMidiOut() {
  midiOutChannel = (midiOutChannel % 16) + 1;
}

void AppSettings::cycleMidiIn() {
  midiInChannel = (midiInChannel + 1) % 17;
}

void AppSettings::cycleBrightness() {
  brightnessPercent += 10;
  if (brightnessPercent > 100) brightnessPercent = 10;
}

void AppSettings::cycleVelocity() {
  outputVelocity += 10;
  if (outputVelocity > 120) outputVelocity = 40;
}
