#include "SettingsStore.h"

#include "ChordModel.h"
#include "SeqExtras.h"
#include "UiTheme.h"
#include <Preferences.h>
#include <Arduino.h>
#include <string.h>

namespace {
constexpr const char* kNs = "m5chord";
constexpr uint8_t kPrefsSchemaVersion = 2;
}  // namespace

void prefsMigrateOnBoot() {
  Preferences p;
  if (!p.begin(kNs, false)) {
    return;
  }
  const uint8_t ver = p.getUChar("schema", 0);
  if (ver < kPrefsSchemaVersion) {
    if (ver < 2) {
      const uint8_t legacyIn = p.getUChar("inCh", 0);
      p.putUChar("inUsb", legacyIn);
      p.putUChar("inBle", legacyIn);
      p.putUChar("inDin", legacyIn);
    }
    p.putUChar("schema", kPrefsSchemaVersion);
  }
  p.end();
}

void settingsLoad(AppSettings& s) {
  Preferences p;
  if (!p.begin(kNs, true)) {
    s.normalize();
    return;
  }
  s.midiOutChannel = p.getUChar("outCh", s.midiOutChannel);
  const uint8_t legacyIn = p.getUChar("inCh", s.midiInChannel);
  s.midiInChannel = legacyIn;
  s.midiInChannelUsb = p.getUChar("inUsb", legacyIn);
  s.midiInChannelBle = p.getUChar("inBle", legacyIn);
  s.midiInChannelDin = p.getUChar("inDin", legacyIn);
  s.brightnessPercent = p.getUChar("bright", s.brightnessPercent);
  s.outputVelocity = p.getUChar("vel", s.outputVelocity);
  s.velocityCurve = p.getUChar("vCur", s.velocityCurve);
  s.globalSwingPct = p.getUChar("gSw", s.globalSwingPct);
  s.globalHumanizePct = p.getUChar("gHu", s.globalHumanizePct);
  s.xyReturnToCenter = p.getUChar("xyCtr", s.xyReturnToCenter);
  s.settingsEntryHoldPreset = p.getUChar("eHld", s.settingsEntryHoldPreset);
  s.seqLongPressPreset = p.getUChar("sHld", s.seqLongPressPreset);
  s.bpmHoldPreset = p.getUChar("bHld", s.bpmHoldPreset);
  s.arpeggiatorMode = p.getUChar("arpMode", s.arpeggiatorMode);
  {
    const uint8_t raw = p.getUChar("xpose", 24);
    const uint8_t enc = raw > 48 ? 24 : raw;
    s.transposeSemitones = static_cast<int8_t>(enc) - 24;
  }
  s.midiTransportSend = p.getUChar("mTx", s.midiTransportSend);
  s.midiTransportReceive = p.getUChar("mRx", s.midiTransportReceive);
  const uint8_t thruMaskStored = p.getUChar("mThM", 255);
  if (thruMaskStored != 255) {
    s.midiThruMask = thruMaskStored;
  } else {
    const uint8_t legacyThru = p.getUChar("mThr", 0);
    s.midiThruMask = legacyThru ? 0x07 : 0x00;
  }
  s.midiClockSource = p.getUChar("mClk", s.midiClockSource);
  s.clkFollow = p.getUChar("clkF", s.clkFollow);
  s.clkFlashEdge = p.getUChar("clkFe", s.clkFlashEdge);
  s.midiDebugEnabled = p.getUChar("mDbg", s.midiDebugEnabled);
  s.playInMonitor = p.getUChar("pInMon", s.playInMonitor);
  s.playInClockHold = p.getUChar("pInClkH", s.playInClockHold);
  s.playInMonitorMode = p.getUChar("pInMode", s.playInMonitorMode);
  s.bleDebugPeakDecay = p.getUChar("bPkDec", s.bleDebugPeakDecay);
  s.panicDebugRotate = p.getUChar("pRot", s.panicDebugRotate);
  s.suggestStableWindow = p.getUChar("sWin", s.suggestStableWindow);
  s.suggestStableGap = p.getUChar("sGap", s.suggestStableGap);
  s.suggestProfile = p.getUChar("sPrf", s.suggestProfile);
  s.suggestProfileLock = p.getUChar("sLck", s.suggestProfileLock);
  p.end();
  s.normalize();
}

void settingsSave(const AppSettings& s) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("schema", kPrefsSchemaVersion);
  p.putUChar("outCh", s.midiOutChannel);
  p.putUChar("inCh", s.midiInChannelUsb);  // legacy key for backward compatibility
  p.putUChar("inUsb", s.midiInChannelUsb);
  p.putUChar("inBle", s.midiInChannelBle);
  p.putUChar("inDin", s.midiInChannelDin);
  p.putUChar("mTx", s.midiTransportSend);
  p.putUChar("mRx", s.midiTransportReceive);
  p.putUChar("mThM", s.midiThruMask);
  p.putUChar("mThr", s.midiThruMask ? 1 : 0);  // legacy compatibility
  p.putUChar("mClk", s.midiClockSource);
  p.putUChar("clkF", s.clkFollow);
  p.putUChar("clkFe", s.clkFlashEdge);
  p.putUChar("mDbg", s.midiDebugEnabled);
  p.putUChar("pInMon", s.playInMonitor);
  p.putUChar("pInClkH", s.playInClockHold);
  p.putUChar("pInMode", s.playInMonitorMode);
  p.putUChar("bPkDec", s.bleDebugPeakDecay);
  p.putUChar("pRot", s.panicDebugRotate);
  p.putUChar("sWin", s.suggestStableWindow);
  p.putUChar("sGap", s.suggestStableGap);
  p.putUChar("sPrf", s.suggestProfile);
  p.putUChar("sLck", s.suggestProfileLock);
  p.putUChar("bright", s.brightnessPercent);
  p.putUChar("vel", s.outputVelocity);
  p.putUChar("vCur", s.velocityCurve);
  p.putUChar("gSw", s.globalSwingPct);
  p.putUChar("gHu", s.globalHumanizePct);
  p.putUChar("xyCtr", s.xyReturnToCenter);
  p.putUChar("eHld", s.settingsEntryHoldPreset);
  p.putUChar("sHld", s.seqLongPressPreset);
  p.putUChar("bHld", s.bpmHoldPreset);
  p.putUChar("arpMode", s.arpeggiatorMode);
  {
    const int16_t enc = static_cast<int16_t>(s.transposeSemitones) + 24;
    const uint8_t u = enc < 0 ? 24 : (enc > 48 ? 48 : static_cast<uint8_t>(enc));
    p.putUChar("xpose", u);
  }
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

void xyMappingLoad(uint8_t* channel, uint8_t* ccA, uint8_t* ccB, uint8_t* curveA, uint8_t* curveB) {
  uint8_t ch = 1;
  uint8_t a = 74;
  uint8_t b = 71;
  uint8_t ca = 0;
  uint8_t cb = 0;
  Preferences p;
  if (!p.begin(kNs, true)) {
    *channel = ch;
    *ccA = a;
    *ccB = b;
    *curveA = ca;
    *curveB = cb;
    return;
  }
  ch = p.getUChar("xyCh", ch);
  a = p.getUChar("xyCCA", a);
  b = p.getUChar("xyCCB", b);
  ca = p.getUChar("xyCvA", ca);
  cb = p.getUChar("xyCvB", cb);
  p.end();
  if (ch < 1 || ch > 16) ch = 1;
  if (a > 127) a = 74;
  if (b > 127) b = 71;
  if (ca > 2) ca = 0;
  if (cb > 2) cb = 0;
  *channel = ch;
  *ccA = a;
  *ccB = b;
  *curveA = ca;
  *curveB = cb;
}

void xyMappingSave(uint8_t channel, uint8_t ccA, uint8_t ccB, uint8_t curveA, uint8_t curveB) {
  if (channel < 1 || channel > 16) channel = 1;
  if (curveA > 2) curveA = 0;
  if (curveB > 2) curveB = 0;
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("xyCh", channel);
  p.putUChar("xyCCA", ccA);
  p.putUChar("xyCCB", ccB);
  p.putUChar("xyCvA", curveA);
  p.putUChar("xyCvB", curveB);
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
    uint8_t buf[394];
    const size_t readN = n > sizeof(buf) ? sizeof(buf) : n;
    p.getBytes("seqEx", buf, readN);
    if (buf[0] == 1 && readN >= 58) {
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
    } else if (buf[0] == 2 && readN >= 250) {
      for (int L = 0; L < 3; ++L) {
        out->quantizePct[L] = buf[1 + L];
        out->swingPct[L] = buf[4 + L];
        out->chordRandPct[L] = buf[7 + L];
        if (out->quantizePct[L] > 100) out->quantizePct[L] = 100;
        if (out->swingPct[L] > 100) out->swingPct[L] = 100;
        if (out->chordRandPct[L] > 100) out->chordRandPct[L] = 100;
      }
      memcpy(out->stepProb, buf + 10, 48);
      memcpy(out->stepClockDiv, buf + 58, 48);
      memcpy(out->arpEnabled, buf + 106, 48);
      memcpy(out->arpPattern, buf + 154, 48);
      memcpy(out->arpClockDiv, buf + 202, 48);
      for (int L = 0; L < 3; ++L) {
        for (int i = 0; i < 16; ++i) {
          if (out->stepProb[L][i] > 100) out->stepProb[L][i] = 100;
          out->stepClockDiv[L][i] &= 0x03U;
          out->arpEnabled[L][i] = out->arpEnabled[L][i] ? 1U : 0U;
          out->arpPattern[L][i] &= 0x03U;
          out->arpClockDiv[L][i] &= 0x03U;
        }
      }
    } else if (buf[0] == 3 && readN >= 346) {
      for (int L = 0; L < 3; ++L) {
        out->quantizePct[L] = buf[1 + L];
        out->swingPct[L] = buf[4 + L];
        out->chordRandPct[L] = buf[7 + L];
        if (out->quantizePct[L] > 100) out->quantizePct[L] = 100;
        if (out->swingPct[L] > 100) out->swingPct[L] = 100;
        if (out->chordRandPct[L] > 100) out->chordRandPct[L] = 100;
      }
      memcpy(out->stepProb, buf + 10, 48);
      memcpy(out->stepClockDiv, buf + 58, 48);
      memcpy(out->arpEnabled, buf + 106, 48);
      memcpy(out->arpPattern, buf + 154, 48);
      memcpy(out->arpClockDiv, buf + 202, 48);
      memcpy(out->arpOctRange, buf + 250, 48);
      memcpy(out->arpGatePct, buf + 298, 48);
      for (int L = 0; L < 3; ++L) {
        for (int i = 0; i < 16; ++i) {
          if (out->stepProb[L][i] > 100) out->stepProb[L][i] = 100;
          out->stepClockDiv[L][i] &= 0x03U;
          out->arpEnabled[L][i] = out->arpEnabled[L][i] ? 1U : 0U;
          out->arpPattern[L][i] &= 0x03U;
          out->arpClockDiv[L][i] &= 0x03U;
          out->arpOctRange[L][i] = out->arpOctRange[L][i] > 2 ? 2 : out->arpOctRange[L][i];
          if (out->arpGatePct[L][i] < 10) out->arpGatePct[L][i] = 10;
          if (out->arpGatePct[L][i] > 100) out->arpGatePct[L][i] = 100;
          out->stepVoicing[L][i] = 4;
        }
      }
    } else if (buf[0] == 4 && readN >= 394) {
      for (int L = 0; L < 3; ++L) {
        out->quantizePct[L] = buf[1 + L];
        out->swingPct[L] = buf[4 + L];
        out->chordRandPct[L] = buf[7 + L];
        if (out->quantizePct[L] > 100) out->quantizePct[L] = 100;
        if (out->swingPct[L] > 100) out->swingPct[L] = 100;
        if (out->chordRandPct[L] > 100) out->chordRandPct[L] = 100;
      }
      memcpy(out->stepProb, buf + 10, 48);
      memcpy(out->stepClockDiv, buf + 58, 48);
      memcpy(out->arpEnabled, buf + 106, 48);
      memcpy(out->arpPattern, buf + 154, 48);
      memcpy(out->arpClockDiv, buf + 202, 48);
      memcpy(out->arpOctRange, buf + 250, 48);
      memcpy(out->arpGatePct, buf + 298, 48);
      memcpy(out->stepVoicing, buf + 346, 48);
      for (int L = 0; L < 3; ++L) {
        for (int i = 0; i < 16; ++i) {
          if (out->stepProb[L][i] > 100) out->stepProb[L][i] = 100;
          out->stepClockDiv[L][i] &= 0x03U;
          out->arpEnabled[L][i] = out->arpEnabled[L][i] ? 1U : 0U;
          out->arpPattern[L][i] &= 0x03U;
          out->arpClockDiv[L][i] &= 0x03U;
          out->arpOctRange[L][i] = out->arpOctRange[L][i] > 2 ? 2 : out->arpOctRange[L][i];
          if (out->arpGatePct[L][i] < 10) out->arpGatePct[L][i] = 10;
          if (out->arpGatePct[L][i] > 100) out->arpGatePct[L][i] = 100;
          if (out->stepVoicing[L][i] < 1) out->stepVoicing[L][i] = 1;
          if (out->stepVoicing[L][i] > 4) out->stepVoicing[L][i] = 4;
        }
      }
    }
  }
  p.end();
}

void seqExtrasSave(const SeqExtras* in) {
  uint8_t buf[394];
  buf[0] = 4;
  for (int L = 0; L < 3; ++L) {
    buf[1 + L] = in->quantizePct[L] > 100 ? 100 : in->quantizePct[L];
    buf[4 + L] = in->swingPct[L] > 100 ? 100 : in->swingPct[L];
    buf[7 + L] = in->chordRandPct[L] > 100 ? 100 : in->chordRandPct[L];
  }
  for (int L = 0; L < 3; ++L) {
    for (int i = 0; i < 16; ++i) {
      const int idx = L * 16 + i;
      buf[10 + idx] = in->stepProb[L][i] > 100 ? 100 : in->stepProb[L][i];
      buf[58 + idx] = in->stepClockDiv[L][i] & 0x03U;
      buf[106 + idx] = in->arpEnabled[L][i] ? 1U : 0U;
      buf[154 + idx] = in->arpPattern[L][i] & 0x03U;
      buf[202 + idx] = in->arpClockDiv[L][i] & 0x03U;
      buf[250 + idx] = in->arpOctRange[L][i] > 2 ? 2 : in->arpOctRange[L][i];
      uint8_t gate = in->arpGatePct[L][i];
      if (gate < 10) gate = 10;
      if (gate > 100) gate = 100;
      buf[298 + idx] = gate;
      uint8_t vo = in->stepVoicing[L][i];
      if (vo < 1) vo = 1;
      if (vo > 4) vo = 4;
      buf[346 + idx] = vo;
    }
  }
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putBytes("seqEx", buf, 394);
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
  xyMappingSave(1, 74, 71, 0, 0);
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
