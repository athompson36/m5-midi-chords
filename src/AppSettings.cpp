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

uint32_t AppSettings::displayAutoDimIdleTimeoutMs() const {
  switch (displayAutoDimPreset) {
    case 1:
      return 30000;
    case 2:
      return 60000;
    case 3:
      return 300000;
    default:
      return 0;
  }
}

void AppSettings::normalize() {
  if (midiOutChannel < 1) midiOutChannel = 1;
  if (midiOutChannel > 16) midiOutChannel = 16;
  if (midiInChannel > 16) midiInChannel = 16;
  if (midiInChannelUsb > 16) midiInChannelUsb = 16;
  if (midiInChannelBle > 16) midiInChannelBle = 16;
  if (midiInChannelDin > 16) midiInChannelDin = 16;
  // Keep legacy field coherent for older code paths/backups.
  midiInChannel = midiInChannelUsb;
  if (midiTransportSend >= kMidiRouteCount) midiTransportSend = 0;
  if (midiTransportReceive >= kMidiRouteCount) midiTransportReceive = 0;
  if (midiThruMask > 7) midiThruMask = 0;
  if (midiClockSource >= kMidiRouteCount) midiClockSource = 0;
  if (clkFollow > 1) clkFollow = 1;
  if (midiClockRateIndex > 4) midiClockRateIndex = 2;
  if (mackieControlPort > 3) mackieControlPort = 0;
  if (abletonLinkEnabled > 1) abletonLinkEnabled = 0;
  if (clkFlashEdge > 1) clkFlashEdge = 0;
  if (midiDebugEnabled > 1) midiDebugEnabled = 1;
  if (playInMonitor > 1) playInMonitor = 0;
  if (playInClockHold > 2) playInClockHold = 1;
  if (playInMonitorMode > 1) playInMonitorMode = 0;
  if (bleDebugPeakDecay > 3) bleDebugPeakDecay = 2;
  if (panicDebugRotate > 3) panicDebugRotate = 2;
  if (suggestStableWindow > 4) suggestStableWindow = 2;
  if (suggestStableGap > 4) suggestStableGap = 2;
  if (suggestProfile > 1) suggestProfile = 0;
  if (suggestProfileLock > 1) suggestProfileLock = 0;
  if (brightnessPercent < 10) brightnessPercent = 10;
  if (brightnessPercent > 100) brightnessPercent = 100;
  if (displayAutoDimPreset > 3) displayAutoDimPreset = 0;
  if (outputVelocity < 40) outputVelocity = 40;
  if (outputVelocity > 127) outputVelocity = 127;
  if (velocityCurve > 2) velocityCurve = 0;
  if (globalSwingPct > 100) globalSwingPct = 0;
  if (globalHumanizePct > 100) globalHumanizePct = 0;
  if (xyReturnToCenter > 1) xyReturnToCenter = 0;
  if (settingsEntryHoldPreset > 4) settingsEntryHoldPreset = 2;
  if (seqLongPressPreset > 4) seqLongPressPreset = 2;
  if (bpmHoldPreset > 3) bpmHoldPreset = 1;
  if (arpeggiatorMode >= kArpeggiatorModeCount) arpeggiatorMode = 0;
  if (transposeSemitones < -24) transposeSemitones = -24;
  if (transposeSemitones > 24) transposeSemitones = 24;
}

void AppSettings::cycleMidiOut() {
  midiOutChannel = (midiOutChannel % 16) + 1;
}

void AppSettings::cycleMidiInUsb() {
  midiInChannelUsb = (midiInChannelUsb + 1) % 17;
  midiInChannel = midiInChannelUsb;
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
