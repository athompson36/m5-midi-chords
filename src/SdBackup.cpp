#include "SdBackup.h"

#include "AppSettings.h"
#include "ChordModel.h"
#include "SeqExtras.h"

#include <SD.h>
#include <SPI.h>
#include <cctype>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace {

constexpr int kSdCsPin = 4;
constexpr int kSdSckPin = 36;
constexpr int kSdMisoPin = 35;
constexpr int kSdMosiPin = 37;

constexpr uint32_t kSdFreq = 25000000;

const char kGlobalDir[] = "/fs-chord-seq/_global";
const char kGlobalFile[] = "/fs-chord-seq/_global/global_settings.txt";

const char kMagicGlobal[] = "FSCHORD_GLOBAL_V1\n";
const char kMagicProject[] = "FSCHORD_PROJECT_V1\n";

bool g_sdMounted = false;

bool writeAll(File& f, const char* s) {
  size_t n = strlen(s);
  return f.write((const uint8_t*)s, n) == n;
}

/// Parses 16 (legacy one-lane) or 48 (three lanes) hex bytes. Returns 16, 48, or -1 on error.
int parseSeqHexLine(const char* p, uint8_t outFlat[48]) {
  int n = 0;
  while (n < 48) {
    while (*p == ',' || *p == ' ') p++;
    if (*p == '\0' || *p == '\r' || *p == '\n') break;
    if (!std::isxdigit(static_cast<unsigned char>(p[0])) ||
        !std::isxdigit(static_cast<unsigned char>(p[1]))) {
      return -1;
    }
    char hex[3] = {p[0], p[1], '\0'};
    outFlat[n++] = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
    p += 2;
  }
  if (n != 16 && n != 48) return -1;
  return n;
}

bool mkdirOne(const char* path) {
  if (SD.exists(path)) {
    File f = SD.open(path);
    if (!f) return false;
    bool isDir = f.isDirectory();
    f.close();
    return isDir;
  }
  return SD.mkdir(path);
}

bool ensureGlobalTree() {
  if (!g_sdMounted) return false;
  if (!mkdirOne("/fs-chord-seq")) return false;
  return mkdirOne(kGlobalDir);
}

bool ensureProjectTree(const char* folderBasename) {
  if (!folderBasename || !folderBasename[0]) return false;
  char buf[96];
  if (!mkdirOne("/fs-chord-seq")) return false;
  snprintf(buf, sizeof(buf), "/fs-chord-seq/%s", folderBasename);
  if (!mkdirOne(buf)) return false;
  snprintf(buf, sizeof(buf), "/fs-chord-seq/%s/settings", folderBasename);
  return mkdirOne(buf);
}

void buildProjectFilePath(char* out, size_t cap, const char* folderBasename) {
  snprintf(out, cap, "/fs-chord-seq/%s/settings/project.txt", folderBasename);
}

bool readFileToBuf(const char* path, char* buf, size_t bufCap, size_t* outLen) {
  if (!SD.exists(path)) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  size_t sz = f.size();
  if (sz == 0 || sz >= bufCap) {
    f.close();
    return false;
  }
  size_t n = f.read((uint8_t*)buf, sz);
  f.close();
  buf[n] = '\0';
  if (outLen) *outLen = n;
  return true;
}

bool parseGlobalBuf(char* buf, AppSettings& s) {
  if (strncmp(buf, "FSCHORD_GLOBAL_V1", 17) != 0) {
    return false;
  }
  char* save = nullptr;
  for (char* line = strtok_r(buf, "\n", &save); line; line = strtok_r(nullptr, "\n", &save)) {
    while (*line == ' ' || *line == '\r') ++line;
    if (!*line || *line == '#') continue;

    unsigned v;
    if (sscanf(line, "outCh:%u", &v) == 1) {
      s.midiOutChannel = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "inCh:%u", &v) == 1) {
      s.midiInChannel = static_cast<uint8_t>(v);
      s.midiInChannelUsb = s.midiInChannel;
      s.midiInChannelBle = s.midiInChannel;
      s.midiInChannelDin = s.midiInChannel;
      continue;
    }
    if (sscanf(line, "inUsb:%u", &v) == 1) {
      s.midiInChannelUsb = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "inBle:%u", &v) == 1) {
      s.midiInChannelBle = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "inDin:%u", &v) == 1) {
      s.midiInChannelDin = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "bright:%u", &v) == 1) {
      s.brightnessPercent = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "vel:%u", &v) == 1) {
      s.outputVelocity = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "vCur:%u", &v) == 1) {
      if (v < 3) s.velocityCurve = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "gSw:%u", &v) == 1) {
      if (v <= 100) s.globalSwingPct = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "gHu:%u", &v) == 1) {
      if (v <= 100) s.globalHumanizePct = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "xyCtr:%u", &v) == 1) {
      if (v < 2) s.xyReturnToCenter = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "eHld:%u", &v) == 1) {
      if (v < 5) s.settingsEntryHoldPreset = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "sHld:%u", &v) == 1) {
      if (v < 5) s.seqLongPressPreset = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "bHld:%u", &v) == 1) {
      if (v < 4) s.bpmHoldPreset = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "arpMode:%u", &v) == 1) {
      if (v < 2) s.arpeggiatorMode = static_cast<uint8_t>(v);
      continue;
    }
    int tx;
    if (sscanf(line, "xpose:%d", &tx) == 1) {
      s.transposeSemitones = static_cast<int8_t>(tx);
      continue;
    }
    if (sscanf(line, "mTx:%u", &v) == 1) {
      if (v < 4) s.midiTransportSend = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "mRx:%u", &v) == 1) {
      if (v < 4) s.midiTransportReceive = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "mThM:%u", &v) == 1) {
      if (v < 8) s.midiThruMask = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "mThr:%u", &v) == 1) {
      if (v < 2) s.midiThruMask = (v != 0U) ? 0x07 : 0x00;
      continue;
    }
    if (sscanf(line, "mClk:%u", &v) == 1) {
      if (v < 4) s.midiClockSource = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "clkF:%u", &v) == 1) {
      if (v < 2) s.clkFollow = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "clkFe:%u", &v) == 1) {
      if (v < 2) s.clkFlashEdge = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "mDbg:%u", &v) == 1) {
      if (v < 2) s.midiDebugEnabled = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "pInMon:%u", &v) == 1) {
      if (v < 2) s.playInMonitor = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "pInClkH:%u", &v) == 1) {
      if (v < 3) s.playInClockHold = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "pInMode:%u", &v) == 1) {
      if (v < 2) s.playInMonitorMode = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "bPkDec:%u", &v) == 1) {
      if (v < 4) s.bleDebugPeakDecay = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "pRot:%u", &v) == 1) {
      if (v < 4) s.panicDebugRotate = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "sWin:%u", &v) == 1) {
      if (v < 5) s.suggestStableWindow = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "sGap:%u", &v) == 1) {
      if (v < 5) s.suggestStableGap = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "sPrf:%u", &v) == 1) {
      if (v < 2) s.suggestProfile = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "sLck:%u", &v) == 1) {
      if (v < 2) s.suggestProfileLock = static_cast<uint8_t>(v);
      continue;
    }
  }
  s.normalize();
  return true;
}

bool parseHex48(const char* p, uint8_t out[48]) {
  int n = 0;
  while (n < 48) {
    while (*p == ',' || *p == ' ' || *p == '\t') p++;
    if (*p == '\0' || *p == '\r' || *p == '\n') break;
    if (!std::isxdigit(static_cast<unsigned char>(p[0])) ||
        !std::isxdigit(static_cast<unsigned char>(p[1]))) {
      return false;
    }
    char hex[3] = {p[0], p[1], '\0'};
    out[n++] = static_cast<uint8_t>(strtoul(hex, nullptr, 16));
    p += 2;
  }
  return n == 48;
}

bool parseProjectBuf(char* buf, ChordModel& m, uint8_t seqPattern[3][16], uint8_t seqCh[3], uint8_t* xyCcA,
                     uint8_t* xyCcB, uint8_t* xyChannel, uint8_t* xyCurveA, uint8_t* xyCurveB,
                     uint16_t* bpm, uint8_t* chordVoicing, SeqExtras* seqExtras) {
  if (strncmp(buf, "FSCHORD_PROJECT_V1", 18) != 0) {
    return false;
  }

  int tonic = m.keyIndex;
  int km = static_cast<int>(m.mode);
  bool haveSeq = false;
  uint8_t xa = *xyCcA;
  uint8_t xb = *xyCcB;
  uint8_t xch = *xyChannel;
  uint8_t xca = *xyCurveA;
  uint8_t xcb = *xyCurveB;
  uint16_t bp = *bpm;
  uint8_t vo = *chordVoicing;
  uint8_t sch[3] = {seqCh[0], seqCh[1], seqCh[2]};
  SeqExtras sx{};
  if (seqExtras) {
    seqExtrasInitDefaults(&sx);
  }

  char* save = nullptr;
  for (char* line = strtok_r(buf, "\n", &save); line; line = strtok_r(nullptr, "\n", &save)) {
    while (*line == ' ' || *line == '\r') ++line;
    if (!*line || *line == '#') continue;

    unsigned v;
    int ti;
    if (sscanf(line, "tonic:%d", &ti) == 1) {
      if (ti >= 0 && ti < ChordModel::kKeyCount) tonic = ti;
      continue;
    }
    if (sscanf(line, "kmode:%d", &km) == 1) {
      continue;
    }
    unsigned bpv;
    if (sscanf(line, "bpm:%u", &bpv) == 1) {
      if (bpv <= 400) bp = static_cast<uint16_t>(bpv);
      continue;
    }
    unsigned voi;
    if (sscanf(line, "voicing:%u", &voi) == 1) {
      if (voi >= 1 && voi <= 4) vo = static_cast<uint8_t>(voi);
      continue;
    }
    unsigned c0, c1, c2;
    if (sscanf(line, "seqCh:%u,%u,%u", &c0, &c1, &c2) == 3) {
      if (c0 >= 1 && c0 <= 16) sch[0] = static_cast<uint8_t>(c0);
      if (c1 >= 1 && c1 <= 16) sch[1] = static_cast<uint8_t>(c1);
      if (c2 >= 1 && c2 <= 16) sch[2] = static_cast<uint8_t>(c2);
      continue;
    }
    if (strncmp(line, "seq:", 4) == 0) {
      uint8_t flat[48];
      const int n = parseSeqHexLine(line + 4, flat);
      if (n == 48) {
        memcpy(seqPattern[0], flat, 16);
        memcpy(seqPattern[1], flat + 16, 16);
        memcpy(seqPattern[2], flat + 32, 16);
        haveSeq = true;
      } else if (n == 16) {
        memcpy(seqPattern[0], flat, 16);
        memset(seqPattern[1], 0xFE, 16);
        memset(seqPattern[2], 0xFE, 16);
        haveSeq = true;
      }
      continue;
    }
    if (sscanf(line, "xyCCA:%u", &v) == 1) {
      if (v <= 127) xa = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "xyCCB:%u", &v) == 1) {
      if (v <= 127) xb = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "xyCh:%u", &v) == 1) {
      if (v >= 1 && v <= 16) xch = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "xyCvA:%u", &v) == 1) {
      if (v <= 2) xca = static_cast<uint8_t>(v);
      continue;
    }
    if (sscanf(line, "xyCvB:%u", &v) == 1) {
      if (v <= 2) xcb = static_cast<uint8_t>(v);
      continue;
    }
    unsigned q0, q1, q2;
    if (seqExtras && sscanf(line, "seqQ:%u,%u,%u", &q0, &q1, &q2) == 3) {
      if (q0 <= 100) sx.quantizePct[0] = static_cast<uint8_t>(q0);
      if (q1 <= 100) sx.quantizePct[1] = static_cast<uint8_t>(q1);
      if (q2 <= 100) sx.quantizePct[2] = static_cast<uint8_t>(q2);
      continue;
    }
    unsigned s0, s1, s2;
    if (seqExtras && sscanf(line, "seqS:%u,%u,%u", &s0, &s1, &s2) == 3) {
      if (s0 <= 100) sx.swingPct[0] = static_cast<uint8_t>(s0);
      if (s1 <= 100) sx.swingPct[1] = static_cast<uint8_t>(s1);
      if (s2 <= 100) sx.swingPct[2] = static_cast<uint8_t>(s2);
      continue;
    }
    unsigned r0, r1, r2;
    if (seqExtras && sscanf(line, "seqR:%u,%u,%u", &r0, &r1, &r2) == 3) {
      if (r0 <= 100) sx.chordRandPct[0] = static_cast<uint8_t>(r0);
      if (r1 <= 100) sx.chordRandPct[1] = static_cast<uint8_t>(r1);
      if (r2 <= 100) sx.chordRandPct[2] = static_cast<uint8_t>(r2);
      continue;
    }
    if (seqExtras && strncmp(line, "seqP:", 5) == 0) {
      uint8_t flat[48];
      if (parseHex48(line + 5, flat)) {
        memcpy(sx.stepProb, flat, 48);
        for (int L = 0; L < 3; ++L) {
          for (int i = 0; i < 16; ++i) {
            if (sx.stepProb[L][i] > 100) sx.stepProb[L][i] = 100;
          }
        }
      }
      continue;
    }
    if (seqExtras && strncmp(line, "seqD:", 5) == 0) {
      uint8_t flat[48];
      if (parseHex48(line + 5, flat)) {
        memcpy(sx.stepClockDiv, flat, 48);
        for (int L = 0; L < 3; ++L) {
          for (int i = 0; i < 16; ++i) {
            sx.stepClockDiv[L][i] &= 0x03U;
          }
        }
      }
      continue;
    }
    if (seqExtras && strncmp(line, "seqAe:", 6) == 0) {
      uint8_t flat[48];
      if (parseHex48(line + 6, flat)) {
        memcpy(sx.arpEnabled, flat, 48);
        for (int L = 0; L < 3; ++L) {
          for (int i = 0; i < 16; ++i) {
            sx.arpEnabled[L][i] = sx.arpEnabled[L][i] ? 1U : 0U;
          }
        }
      }
      continue;
    }
    if (seqExtras && strncmp(line, "seqAp:", 6) == 0) {
      uint8_t flat[48];
      if (parseHex48(line + 6, flat)) {
        memcpy(sx.arpPattern, flat, 48);
        for (int L = 0; L < 3; ++L) {
          for (int i = 0; i < 16; ++i) {
            sx.arpPattern[L][i] &= 0x03U;
          }
        }
      }
      continue;
    }
    if (seqExtras && strncmp(line, "seqAd:", 6) == 0) {
      uint8_t flat[48];
      if (parseHex48(line + 6, flat)) {
        memcpy(sx.arpClockDiv, flat, 48);
        for (int L = 0; L < 3; ++L) {
          for (int i = 0; i < 16; ++i) {
            sx.arpClockDiv[L][i] &= 0x03U;
          }
        }
      }
      continue;
    }
  }

  if (km < 0 || km >= static_cast<int>(KeyMode::kCount)) {
    km = 0;
  }
  if (!haveSeq) {
    memset(seqPattern, 0xFE, 48);
  }
  seqCh[0] = sch[0];
  seqCh[1] = sch[1];
  seqCh[2] = sch[2];
  m.setTonicAndMode(tonic, static_cast<KeyMode>(km));
  *xyCcA = xa;
  *xyCcB = xb;
  *xyChannel = xch;
  *xyCurveA = xca;
  *xyCurveB = xcb;
  *bpm = bp;
  *chordVoicing = vo;
  if (seqExtras) {
    *seqExtras = sx;
  }
  return true;
}

const char* baseNameOnly(const char* pathOrName) {
  const char* p = strrchr(pathOrName, '/');
  return p ? (p + 1) : pathOrName;
}

}  // namespace

void sdBackupSanitizeFolderName(char* s) {
  for (; *s; ++s) {
    unsigned char c = static_cast<unsigned char>(*s);
    if (c < 32 || c > 126 || strchr("<>:\"/\\|?*", *s)) {
      *s = '_';
    }
  }
}

void sdBackupFormatDefaultProjectFolder(char* out, size_t cap, const ChordModel& m, uint16_t bpm) {
  const char* key = m.keyName();
  const char* mode = ChordModel::modeShortLabel(m.mode);
  time_t now = time(nullptr);
  if (now > 1700000000) {
    struct tm* t = localtime(&now);
    if (t) {
      snprintf(out, cap, "%04d-%02d-%02d_%s_%s_%u", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, key,
               mode, (unsigned)bpm);
    } else {
      snprintf(out, cap, "%s_%s_%u", key, mode, (unsigned)bpm);
    }
  } else {
    snprintf(out, cap, "%s_%s_%u", key, mode, (unsigned)bpm);
  }
  sdBackupSanitizeFolderName(out);
}

bool sdBackupInit() {
  SPI.begin(kSdSckPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  g_sdMounted = SD.begin(kSdCsPin, SPI, kSdFreq);
  return g_sdMounted;
}

bool sdBackupIsMounted() { return g_sdMounted; }

bool sdBackupWriteGlobal(const AppSettings& s) {
  if (!g_sdMounted) return false;
  if (!ensureGlobalTree()) return false;

  File f = SD.open(kGlobalFile, FILE_WRITE, true);
  if (!f) return false;

  char line[96];
  bool ok = writeAll(f, kMagicGlobal);
  snprintf(line, sizeof(line), "outCh:%u\n", (unsigned)s.midiOutChannel);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "inCh:%u\n", (unsigned)s.midiInChannelUsb);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "inUsb:%u\n", (unsigned)s.midiInChannelUsb);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "inBle:%u\n", (unsigned)s.midiInChannelBle);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "inDin:%u\n", (unsigned)s.midiInChannelDin);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "bright:%u\n", (unsigned)s.brightnessPercent);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "vel:%u\n", (unsigned)s.outputVelocity);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "vCur:%u\n", (unsigned)s.velocityCurve);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "gSw:%u\n", (unsigned)s.globalSwingPct);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "gHu:%u\n", (unsigned)s.globalHumanizePct);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "xyCtr:%u\n", (unsigned)s.xyReturnToCenter);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "eHld:%u\n", (unsigned)s.settingsEntryHoldPreset);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "sHld:%u\n", (unsigned)s.seqLongPressPreset);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "bHld:%u\n", (unsigned)s.bpmHoldPreset);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "arpMode:%u\n", (unsigned)s.arpeggiatorMode);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "xpose:%d\n", static_cast<int>(s.transposeSemitones));
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "mTx:%u\n", (unsigned)s.midiTransportSend);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "mRx:%u\n", (unsigned)s.midiTransportReceive);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "mThM:%u\n", (unsigned)s.midiThruMask);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "mThr:%u\n", (unsigned)(s.midiThruMask ? 1U : 0U));
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "mClk:%u\n", (unsigned)s.midiClockSource);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "clkF:%u\n", (unsigned)s.clkFollow);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "clkFe:%u\n", (unsigned)s.clkFlashEdge);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "mDbg:%u\n", (unsigned)s.midiDebugEnabled);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "pInMon:%u\n", (unsigned)s.playInMonitor);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "pInClkH:%u\n", (unsigned)s.playInClockHold);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "pInMode:%u\n", (unsigned)s.playInMonitorMode);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "bPkDec:%u\n", (unsigned)s.bleDebugPeakDecay);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "pRot:%u\n", (unsigned)s.panicDebugRotate);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "sWin:%u\n", (unsigned)s.suggestStableWindow);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "sGap:%u\n", (unsigned)s.suggestStableGap);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "sPrf:%u\n", (unsigned)s.suggestProfile);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "sLck:%u\n", (unsigned)s.suggestProfileLock);
  ok = ok && writeAll(f, line);
  f.close();
  return ok;
}

bool sdBackupReadGlobal(AppSettings& s) {
  if (!g_sdMounted) return false;
  char buf[512];
  if (!readFileToBuf(kGlobalFile, buf, sizeof(buf), nullptr)) return false;
  return parseGlobalBuf(buf, s);
}

bool sdBackupWriteProject(const ChordModel& m, const uint8_t seqPattern[3][16], const uint8_t seqCh[3],
                          uint8_t xyChannel, uint8_t xyCcA, uint8_t xyCcB, uint8_t xyCurveA,
                          uint8_t xyCurveB, uint16_t bpm, uint8_t chordVoicing,
                          const SeqExtras* seqExtras, const char* projectFolderBasename) {
  if (!g_sdMounted || !projectFolderBasename || !projectFolderBasename[0]) return false;
  if (!ensureProjectTree(projectFolderBasename)) return false;

  char path[96];
  buildProjectFilePath(path, sizeof(path), projectFolderBasename);

  File f = SD.open(path, FILE_WRITE, true);
  if (!f) return false;

  char line[160];
  bool ok = writeAll(f, kMagicProject);
  snprintf(line, sizeof(line), "tonic:%d\n", m.keyIndex);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "kmode:%d\n", static_cast<int>(m.mode));
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "bpm:%u\n", (unsigned)bpm);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "voicing:%u\n", (unsigned)chordVoicing);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "seqCh:%u,%u,%u\n", (unsigned)seqCh[0], (unsigned)seqCh[1],
           (unsigned)seqCh[2]);
  ok = ok && writeAll(f, line);
  ok = ok && writeAll(f, "seq:");
  for (int k = 0, L = 0; L < 3 && ok; ++L) {
    for (int i = 0; i < 16 && ok; ++i) {
      snprintf(line, sizeof(line), "%02X", seqPattern[L][i]);
      ok = writeAll(f, line);
      if (k < 47) ok = ok && writeAll(f, ",");
      ++k;
    }
  }
  ok = ok && writeAll(f, "\n");
  snprintf(line, sizeof(line), "xyCCA:%u\n", (unsigned)xyCcA);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "xyCCB:%u\n", (unsigned)xyCcB);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "xyCh:%u\n", (unsigned)xyChannel);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "xyCvA:%u\n", (unsigned)xyCurveA);
  ok = ok && writeAll(f, line);
  snprintf(line, sizeof(line), "xyCvB:%u\n", (unsigned)xyCurveB);
  ok = ok && writeAll(f, line);
  if (seqExtras) {
    snprintf(line, sizeof(line), "seqQ:%u,%u,%u\n", (unsigned)seqExtras->quantizePct[0],
             (unsigned)seqExtras->quantizePct[1], (unsigned)seqExtras->quantizePct[2]);
    ok = ok && writeAll(f, line);
    snprintf(line, sizeof(line), "seqS:%u,%u,%u\n", (unsigned)seqExtras->swingPct[0],
             (unsigned)seqExtras->swingPct[1], (unsigned)seqExtras->swingPct[2]);
    ok = ok && writeAll(f, line);
    snprintf(line, sizeof(line), "seqR:%u,%u,%u\n", (unsigned)seqExtras->chordRandPct[0],
             (unsigned)seqExtras->chordRandPct[1], (unsigned)seqExtras->chordRandPct[2]);
    ok = ok && writeAll(f, line);
    ok = ok && writeAll(f, "seqP:");
    for (int k = 0, L = 0; L < 3 && ok; ++L) {
      for (int i = 0; i < 16 && ok; ++i) {
        snprintf(line, sizeof(line), "%02X", seqExtras->stepProb[L][i]);
        ok = writeAll(f, line);
        if (k < 47) ok = ok && writeAll(f, ",");
        ++k;
      }
    }
    ok = ok && writeAll(f, "\n");
    ok = ok && writeAll(f, "seqD:");
    for (int k = 0, L = 0; L < 3 && ok; ++L) {
      for (int i = 0; i < 16 && ok; ++i) {
        snprintf(line, sizeof(line), "%02X", seqExtras->stepClockDiv[L][i] & 0x03U);
        ok = writeAll(f, line);
        if (k < 47) ok = ok && writeAll(f, ",");
        ++k;
      }
    }
    ok = ok && writeAll(f, "\n");
    ok = ok && writeAll(f, "seqAe:");
    for (int k = 0, L = 0; L < 3 && ok; ++L) {
      for (int i = 0; i < 16 && ok; ++i) {
        snprintf(line, sizeof(line), "%02X", seqExtras->arpEnabled[L][i] ? 1U : 0U);
        ok = writeAll(f, line);
        if (k < 47) ok = ok && writeAll(f, ",");
        ++k;
      }
    }
    ok = ok && writeAll(f, "\n");
    ok = ok && writeAll(f, "seqAp:");
    for (int k = 0, L = 0; L < 3 && ok; ++L) {
      for (int i = 0; i < 16 && ok; ++i) {
        snprintf(line, sizeof(line), "%02X", seqExtras->arpPattern[L][i] & 0x03U);
        ok = writeAll(f, line);
        if (k < 47) ok = ok && writeAll(f, ",");
        ++k;
      }
    }
    ok = ok && writeAll(f, "\n");
    ok = ok && writeAll(f, "seqAd:");
    for (int k = 0, L = 0; L < 3 && ok; ++L) {
      for (int i = 0; i < 16 && ok; ++i) {
        snprintf(line, sizeof(line), "%02X", seqExtras->arpClockDiv[L][i] & 0x03U);
        ok = writeAll(f, line);
        if (k < 47) ok = ok && writeAll(f, ",");
        ++k;
      }
    }
    ok = ok && writeAll(f, "\n");
  }
  f.close();
  return ok;
}

bool sdBackupReadProject(ChordModel& m, uint8_t seqPattern[3][16], uint8_t seqCh[3], uint8_t* xyCcA,
                         uint8_t* xyCcB, uint8_t* xyChannel, uint8_t* xyCurveA, uint8_t* xyCurveB,
                         uint16_t* bpm, uint8_t* chordVoicing, SeqExtras* seqExtras,
                         const char* projectFolderBasename) {
  if (!g_sdMounted || !projectFolderBasename || !projectFolderBasename[0]) return false;
  char path[96];
  buildProjectFilePath(path, sizeof(path), projectFolderBasename);
  char buf[4096];
  if (!readFileToBuf(path, buf, sizeof(buf), nullptr)) return false;
  return parseProjectBuf(buf, m, seqPattern, seqCh, xyCcA, xyCcB, xyChannel, xyCurveA, xyCurveB, bpm,
                         chordVoicing, seqExtras);
}

bool sdBackupListProjects(char out[][48], int maxProjects, int* outCount) {
  *outCount = 0;
  if (!g_sdMounted || maxProjects <= 0) return false;
  File root = SD.open("/fs-chord-seq");
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return false;
  }
  for (;;) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) {
      const char* raw = entry.name();
      const char* base = baseNameOnly(raw);
      if (base[0] && strcmp(base, "_global") != 0 && base[0] != '.') {
        strncpy(out[*outCount], base, 47);
        out[*outCount][47] = '\0';
        (*outCount)++;
        if (*outCount >= maxProjects) {
          entry.close();
          break;
        }
      }
    }
    entry.close();
  }
  root.close();
  return true;
}

bool sdBackupWriteAll(const AppSettings& s, const ChordModel& m, const uint8_t seqPattern[3][16],
                      const uint8_t seqCh[3], uint8_t xyChannel, uint8_t xyCcA, uint8_t xyCcB,
                      uint8_t xyCurveA, uint8_t xyCurveB, uint16_t bpm, uint8_t chordVoicing,
                      const SeqExtras* seqExtras,
                      const char* projectFolderBasename) {
  if (!sdBackupWriteGlobal(s)) return false;
  if (!sdBackupWriteProject(m, seqPattern, seqCh, xyChannel, xyCcA, xyCcB, xyCurveA, xyCurveB, bpm,
                            chordVoicing, seqExtras, projectFolderBasename)) {
    return false;
  }
  return true;
}

bool sdBackupReadAll(AppSettings& s, ChordModel& m, uint8_t seqPattern[3][16], uint8_t seqCh[3],
                     uint8_t* xyCcA, uint8_t* xyCcB, uint8_t* xyChannel, uint8_t* xyCurveA,
                     uint8_t* xyCurveB, uint16_t* bpm, uint8_t* chordVoicing, SeqExtras* seqExtras,
                     const char* projectFolderBasename) {
  if (!sdBackupReadGlobal(s)) return false;
  if (!sdBackupReadProject(m, seqPattern, seqCh, xyCcA, xyCcB, xyChannel, xyCurveA, xyCurveB, bpm,
                           chordVoicing, seqExtras, projectFolderBasename)) {
    return false;
  }
  return true;
}
