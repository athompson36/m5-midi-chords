#include "AppSettings.h"

#include "Arpeggio.h"

namespace {

constexpr uint8_t kMidiRouteCount = 4;

}  // namespace

const char* midiTransportRouteLabel(uint8_t v) {
  switch (v) {
    case 0:
      return "Off";
    case 1:
      return "USB";
    case 2:
      return "Bluetooth";
    case 3:
      return "DIN";
    default:
      return "?";
  }
}

const char* midiClockSourceLabel(uint8_t v) {
  switch (v) {
    case 0:
      return "Internal";
    case 1:
      return "USB";
    case 2:
      return "Bluetooth";
    case 3:
      return "DIN";
    default:
      return "?";
  }
}

void AppSettings::normalize() {
  if (midiOutChannel < 1) midiOutChannel = 1;
  if (midiOutChannel > 16) midiOutChannel = 16;
  if (midiInChannel > 16) midiInChannel = 16;
  if (midiTransportSend >= kMidiRouteCount) midiTransportSend = 0;
  if (midiTransportReceive >= kMidiRouteCount) midiTransportReceive = 0;
  if (midiClockSource >= kMidiRouteCount) midiClockSource = 0;
  if (brightnessPercent < 10) brightnessPercent = 10;
  if (brightnessPercent > 100) brightnessPercent = 100;
  if (outputVelocity < 40) outputVelocity = 40;
  if (outputVelocity > 127) outputVelocity = 127;
  if (arpeggiatorMode >= kArpeggiatorModeCount) arpeggiatorMode = 0;
}

void AppSettings::cycleMidiOut() {
  midiOutChannel = (midiOutChannel % 16) + 1;
}

void AppSettings::cycleMidiIn() {
  midiInChannel = (midiInChannel + 1) % 17;
}

void AppSettings::cycleMidiTransportSend() {
  midiTransportSend = static_cast<uint8_t>((midiTransportSend + 1U) % kMidiRouteCount);
}

void AppSettings::cycleMidiTransportReceive() {
  midiTransportReceive = static_cast<uint8_t>((midiTransportReceive + 1U) % kMidiRouteCount);
}

void AppSettings::cycleMidiClockSource() {
  midiClockSource = static_cast<uint8_t>((midiClockSource + 1U) % kMidiRouteCount);
}

void AppSettings::cycleBrightness() {
  brightnessPercent += 10;
  if (brightnessPercent > 100) brightnessPercent = 10;
}

void AppSettings::cycleVelocity() {
  outputVelocity += 10;
  if (outputVelocity > 120) outputVelocity = 40;
}

void AppSettings::cycleArpeggiatorMode() {
  arpeggiatorMode = static_cast<uint8_t>((arpeggiatorMode + 1U) % kArpeggiatorModeCount);
}
