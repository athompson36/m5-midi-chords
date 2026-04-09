#include "SettingsStore.h"

#include "ChordModel.h"
#include "SeqExtras.h"
#include "UiTheme.h"
#include <Preferences.h>
#include <Arduino.h>
#include <string.h>

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
  s.arpeggiatorMode = p.getUChar("arpMode", s.arpeggiatorMode);
  s.midiTransportSend = p.getUChar("mTx", s.midiTransportSend);
  s.midiTransportReceive = p.getUChar("mRx", s.midiTransportReceive);
  s.midiClockSource = p.getUChar("mClk", s.midiClockSource);
  p.end();
  s.normalize();
}

void settingsSave(const AppSettings& s) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("outCh", s.midiOutChannel);
  p.putUChar("inCh", s.midiInChannel);
  p.putUChar("mTx", s.midiTransportSend);
  p.putUChar("mRx", s.midiTransportReceive);
  p.putUChar("mClk", s.midiClockSource);
  p.putUChar("bright", s.brightnessPercent);
  p.putUChar("vel", s.outputVelocity);
  p.putUChar("arpMode", s.arpeggiatorMode);
  p.end();
}

void chordStateLoad(ChordModel& m) {
  Preferences p;
  if (!p.begin(kNs, true)) return;
  int tonic = p.getUChar("tonic", static_cast<uint8_t>(m.keyIndex));
  int km = p.getUChar("kmode", static_cast<uint8_t>(m.mode));
  p.end();
  if (tonic < 0 || tonic >= ChordModel::kKeyCount) tonic = 0;
  if (km < 0 || km >= static_cast<int>(KeyMode::kCount)) {
    km = 0;
  }
  m.setTonicAndMode(tonic, static_cast<KeyMode>(km));
}

void chordStateSave(const ChordModel& m) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("tonic", static_cast<uint8_t>(m.keyIndex));
  p.putUChar("kmode", static_cast<uint8_t>(m.mode));
  p.end();
}

void seqPatternLoad(uint8_t out[3][16]) {
  constexpr uint8_t kRest = 0xFE;
  for (int L = 0; L < 3; ++L) {
    for (int i = 0; i < 16; ++i) {
      out[L][i] = kRest;
    }
  }
  Preferences p;
  if (!p.begin(kNs, true)) return;
  const size_t n = p.getBytesLength("seq");
  if (n == 48) {
    p.getBytes("seq", out, 48);
  } else if (n == 16) {
    p.getBytes("seq", out[0], 16);
  }
  p.end();
}

void seqPatternSave(const uint8_t in[3][16]) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putBytes("seq", in, 48);
  p.end();
}

void seqLaneChannelsLoad(uint8_t out[3]) {
  out[0] = 1;
  out[1] = 2;
  out[2] = 3;
  Preferences p;
  if (!p.begin(kNs, true)) return;
  const size_t n = p.getBytesLength("seqCh");
  if (n >= 3) {
    p.getBytes("seqCh", out, 3);
  }
  p.end();
  for (int i = 0; i < 3; ++i) {
    if (out[i] < 1 || out[i] > 16) {
      out[i] = static_cast<uint8_t>(1 + (i % 16));
    }
  }
}

void seqLaneChannelsSave(const uint8_t in[3]) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putBytes("seqCh", in, 3);
  p.end();
}

void chordVoicingLoad(uint8_t* out) {
  *out = 4;
  Preferences p;
  if (!p.begin(kNs, true)) return;
  *out = p.getUChar("voicing", *out);
  p.end();
  if (*out < 1 || *out > 4) {
    *out = 4;
  }
}

void chordVoicingSave(uint8_t v) {
  if (v < 1) v = 1;
  if (v > 4) v = 4;
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("voicing", v);
  p.end();
}

void uiThemeLoad(uint8_t* out) {
  *out = 0;
  Preferences p;
  if (!p.begin(kNs, true)) return;
  *out = p.getUChar("uiTheme", 0);
  p.end();
  if (*out >= kUiThemeCount) {
    *out = 0;
  }
}

void uiThemeSave(uint8_t v) {
  if (v >= kUiThemeCount) v = 0;
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("uiTheme", v);
  p.end();
}

void xyMappingLoad(uint8_t* ccA, uint8_t* ccB) {
  uint8_t a = 74;
  uint8_t b = 71;
  Preferences p;
  if (!p.begin(kNs, true)) {
    *ccA = a;
    *ccB = b;
    return;
  }
  a = p.getUChar("xyCCA", a);
  b = p.getUChar("xyCCB", b);
  p.end();
  if (a > 127) a = 74;
  if (b > 127) b = 71;
  *ccA = a;
  *ccB = b;
}

void xyMappingSave(uint8_t ccA, uint8_t ccB) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("xyCCA", ccA);
  p.putUChar("xyCCB", ccB);
  p.end();
}

void transportPrefsLoadFromStore(uint8_t* clickVol, bool* metro, bool* countIn, bool* xyArm) {
  *clickVol = 70;
  *metro = true;
  *countIn = true;
  *xyArm = false;
  Preferences p;
  if (!p.begin(kNs, true)) return;
  *clickVol = p.getUChar("clkVol", 70);
  *metro = p.getUChar("metro", 1) != 0;
  *countIn = p.getUChar("cntIn", 1) != 0;
  *xyArm = p.getUChar("xyArm", 0) != 0;
  p.end();
  if (*clickVol > 100) *clickVol = 100;
}

void transportPrefsSaveToStore(uint8_t clickVol, bool metro, bool countIn, bool xyArm) {
  if (clickVol > 100) clickVol = 100;
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("clkVol", clickVol);
  p.putUChar("metro", metro ? 1 : 0);
  p.putUChar("cntIn", countIn ? 1 : 0);
  p.putUChar("xyArm", xyArm ? 1 : 0);
  p.end();
}

void seqExtrasLoad(SeqExtras* out) {
  seqExtrasInitDefaults(out);
  Preferences p;
  if (!p.begin(kNs, true)) return;
  const size_t n = p.getBytesLength("seqEx");
  if (n >= 58) {
    uint8_t buf[64];
    p.getBytes("seqEx", buf, 58);
    if (buf[0] == 1) {
      for (int L = 0; L < 3; ++L) {
        out->quantizePct[L] = buf[1 + L];
        out->swingPct[L] = buf[4 + L];
        out->chordRandPct[L] = buf[7 + L];
        if (out->quantizePct[L] > 100) out->quantizePct[L] = 100;
        if (out->swingPct[L] > 100) out->swingPct[L] = 100;
        if (out->chordRandPct[L] > 100) out->chordRandPct[L] = 100;
      }
      memcpy(out->stepProb, buf + 10, 48);
      for (int L = 0; L < 3; ++L) {
        for (int i = 0; i < 16; ++i) {
          if (out->stepProb[L][i] > 100) out->stepProb[L][i] = 100;
        }
      }
    }
  }
  p.end();
}

void seqExtrasSave(const SeqExtras* in) {
  uint8_t buf[58];
  buf[0] = 1;
  for (int L = 0; L < 3; ++L) {
    buf[1 + L] = in->quantizePct[L] > 100 ? 100 : in->quantizePct[L];
    buf[4 + L] = in->swingPct[L] > 100 ? 100 : in->swingPct[L];
    buf[7 + L] = in->chordRandPct[L] > 100 ? 100 : in->chordRandPct[L];
  }
  memcpy(buf + 10, in->stepProb, 48);
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putBytes("seqEx", buf, 58);
  p.end();
}

void xyAutoPatternLoad(uint8_t a[16], uint8_t b[16]) {
  for (int i = 0; i < 16; ++i) {
    a[i] = 255;
    b[i] = 255;
  }
  Preferences p;
  if (!p.begin(kNs, true)) return;
  const size_t n = p.getBytesLength("xyAu2");
  if (n == 32) {
    uint8_t buf[32];
    p.getBytes("xyAu2", buf, 32);
    memcpy(a, buf, 16);
    memcpy(b, buf + 16, 16);
  }
  p.end();
}

void xyAutoPatternSave(const uint8_t a[16], const uint8_t b[16]) {
  uint8_t buf[32];
  memcpy(buf, a, 16);
  memcpy(buf + 16, b, 16);
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putBytes("xyAu2", buf, 32);
  p.end();
}

void xyAutoWriteStep(uint8_t step, uint8_t x, uint8_t y, uint8_t a[16], uint8_t b[16]) {
  if (step > 15) return;
  a[step] = x;
  b[step] = y;
  xyAutoPatternSave(a, b);
}

void projectBpmLoad(uint16_t* bpm) {
  *bpm = 120;
  Preferences p;
  if (!p.begin(kNs, true)) return;
  uint32_t v = p.getUInt("projBpm", 120);
  p.end();
  if (v < 40 || v > 300) v = 120;
  *bpm = static_cast<uint16_t>(v);
}

void projectBpmSave(uint16_t bpm) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUInt("projBpm", bpm);
  p.end();
}

void projectCustomNameLoad(char out[48]) {
  out[0] = '\0';
  Preferences p;
  if (!p.begin(kNs, true)) return;
  String s = p.getString("projNm", "");
  strncpy(out, s.c_str(), 47);
  out[47] = '\0';
  p.end();
}

void projectCustomNameSave(const char* name) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("projNm", name ? name : "");
  p.end();
}

void lastProjectFolderLoad(char out[48]) {
  out[0] = '\0';
  Preferences p;
  if (!p.begin(kNs, true)) return;
  String s = p.getString("lastProj", "");
  strncpy(out, s.c_str(), 47);
  out[47] = '\0';
  p.end();
}

void lastProjectFolderSave(const char* folderBasename) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putString("lastProj", folderBasename ? folderBasename : "");
  p.end();
}

void factoryResetAll(AppSettings& s, ChordModel& m) {
  Preferences p;
  if (p.begin(kNs, false)) {
    p.clear();
    p.end();
  }
  s = AppSettings{};
  s.normalize();
  m = ChordModel{};
  m.setTonicAndMode(0, KeyMode::Major);
  uint8_t rest[3][16];
  memset(rest, 0xFE, sizeof(rest));
  uint8_t lanes[3] = {1, 2, 3};
  settingsSave(s);
  chordStateSave(m);
  seqPatternSave(rest);
  seqLaneChannelsSave(lanes);
  chordVoicingSave(4);
  xyMappingSave(74, 71);
  projectBpmSave(120);
  projectCustomNameSave("");
  lastProjectFolderSave("");
  uiThemeSave(0);
  transportPrefsSaveToStore(70, true, true, false);
  uint8_t xa[16];
  uint8_t xb[16];
  memset(xa, 255, 16);
  memset(xb, 255, 16);
  xyAutoPatternSave(xa, xb);
  SeqExtras se{};
  seqExtrasInitDefaults(&se);
  seqExtrasSave(&se);
}
