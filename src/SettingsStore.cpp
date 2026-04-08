#include "SettingsStore.h"

#include <Preferences.h>

namespace {
constexpr const char* kNs = "m5chord";
}

void settingsLoad(AppSettings& s) {
  Preferences p;
  if (!p.begin(kNs, true)) {
    s.normalize();
    return;
  }
  s.midiOutChannel = p.getUChar("outCh", s.midiOutChannel);
  s.midiInChannel = p.getUChar("inCh", s.midiInChannel);
  s.brightnessPercent = p.getUChar("bright", s.brightnessPercent);
  s.outputVelocity = p.getUChar("vel", s.outputVelocity);
  p.end();
  s.normalize();
}

void settingsSave(const AppSettings& s) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("outCh", s.midiOutChannel);
  p.putUChar("inCh", s.midiInChannel);
  p.putUChar("bright", s.brightnessPercent);
  p.putUChar("vel", s.outputVelocity);
  p.end();
}
