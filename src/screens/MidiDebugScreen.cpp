#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

using Rect = ::ui::Rect;

extern MidiInState s_midiInState;
extern MidiEventHistory s_midiEventHistory;
extern uint32_t s_playCategoryTouchStartMs;
extern char s_midiSuggestTop3[3][12];
extern int16_t s_midiSuggestTop3Score[3];
extern uint32_t s_midiSuggestTop3UntilMs;
extern uint32_t s_midiSuggestStableUntilMs;
extern uint32_t s_panicTriggerCounts[static_cast<size_t>(PanicTrigger::Count)];

static int8_t s_midiDbgSourceFilter = -1;  // -1 all, 0 usb, 1 ble, 2 din
static int8_t s_midiDbgTypeFilter = -1;    // -1 all, else MidiEventType value
static uint32_t s_bleDbgRatePrevMs = 0;
static uint32_t s_bleDbgRatePrevRx = 0;
static uint16_t s_bleDbgRateBps = 0;
static uint16_t s_bleDbgRatePeakBps = 0;
static uint32_t s_bleDbgLastActivityMs = 0;
static bool s_bleDbgPeakHeld = true;
static uint32_t s_dinDbgRatePrevMs = 0;
static uint32_t s_dinDbgRatePrevRx = 0;
static uint16_t s_dinDbgRateBps = 0;
static uint16_t s_dinDbgRatePeakBps = 0;
static uint32_t s_dinDbgLastActivityMs = 0;
static bool s_dinDbgPeakHeld = true;
static uint32_t s_outDbgRatePrevMs = 0;
static uint32_t s_outDbgRatePrevTx = 0;
static uint16_t s_outDbgRateBps = 0;
static uint16_t s_outDbgRatePeakBps = 0;
static uint32_t s_bleDbgPrevDrop = 0;
static uint32_t s_bleDbgWarnUntilMs = 0;
static Rect g_midiDbgSourceChip{};
static Rect g_midiDbgTypeChip{};
static Rect g_midiDbgResetChip{};

void midiDebugResetUiDiagnosticsState() {
  s_bleDbgRatePrevMs = 0;
  s_bleDbgRatePrevRx = 0;
  s_bleDbgRateBps = 0;
  s_bleDbgRatePeakBps = 0;
  s_bleDbgLastActivityMs = 0;
  s_bleDbgPeakHeld = true;
  s_dinDbgRatePrevMs = 0;
  s_dinDbgRatePrevRx = 0;
  s_dinDbgRateBps = 0;
  s_dinDbgRatePeakBps = 0;
  s_dinDbgLastActivityMs = 0;
  s_dinDbgPeakHeld = true;
  s_outDbgRatePrevMs = 0;
  s_outDbgRatePrevTx = 0;
  s_outDbgRateBps = 0;
  s_outDbgRatePeakBps = 0;
  s_bleDbgPrevDrop = 0;
  s_bleDbgWarnUntilMs = 0;
}

static const char* midiSrcShortLabel(MidiSource src) {
  switch (src) {
    case MidiSource::Usb:
      return "USB";
    case MidiSource::Ble:
      return "BLE";
    case MidiSource::Din:
      return "DIN";
    default:
      return "?";
  }
}

static const char* midiTypeShortLabel(MidiEventType t) {
  switch (t) {
    case MidiEventType::NoteOn:
      return "On";
    case MidiEventType::NoteOff:
      return "Off";
    case MidiEventType::ControlChange:
      return "CC";
    case MidiEventType::ProgramChange:
      return "PC";
    case MidiEventType::PitchBend:
      return "PB";
    case MidiEventType::Clock:
      return "Clk";
    case MidiEventType::Start:
      return "Start";
    case MidiEventType::Continue:
      return "Cont";
    case MidiEventType::Stop:
      return "Stop";
    case MidiEventType::SongPosition:
      return "SPP";
    case MidiEventType::SysEx:
      return "SX";
    default:
      return "?";
  }
}

static const char* midiDbgSourceFilterLabel() {
  switch (s_midiDbgSourceFilter) {
    case -1:
      return "Src:All";
    case 0:
      return "Src:USB";
    case 1:
      return "Src:BLE";
    case 2:
      return "Src:DIN";
    default:
      return "Src:?";
  }
}

static const char* midiDbgTypeFilterLabel() {
  if (s_midiDbgTypeFilter < 0) return "Typ:All";
  return midiTypeShortLabel(static_cast<MidiEventType>(s_midiDbgTypeFilter));
}

static bool midiDbgEventMatchesFilters(const MidiEventHistoryItem& ev) {
  if (s_midiDbgSourceFilter >= 0) {
    if (static_cast<int>(ev.source) != s_midiDbgSourceFilter) return false;
  }
  if (s_midiDbgTypeFilter >= 0) {
    if (static_cast<int>(ev.type) != s_midiDbgTypeFilter) return false;
  }
  return true;
}

static void formatPanicDebugLine(uint32_t nowMs, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  auto panicDebugRotateMs = []() -> uint32_t {
    switch (g_settings.panicDebugRotate) {
      case 1:
        return 1000U;
      case 2:
        return 2000U;
      case 3:
        return 4000U;
      default:
        return 0U;
    }
  };
  constexpr size_t kShowPerPage = 4;
  const size_t total = static_cast<size_t>(PanicTrigger::Count);
  const size_t pageCount = (total + kShowPerPage - 1U) / kShowPerPage;
  const uint32_t pageMs = panicDebugRotateMs();
  const size_t page = pageCount > 0
                          ? (pageMs == 0U ? 0U : static_cast<size_t>((nowMs / pageMs) % pageCount))
                          : 0U;
  const size_t start = page * kShowPerPage;
  const size_t i0 = start + 0U;
  const size_t i1 = start + 1U;
  const size_t i2 = start + 2U;
  const size_t i3 = start + 3U;
  snprintf(out, outSize, "Panic %s:%lu %s:%lu %s:%lu %s:%lu",
           i0 < total ? panicTriggerShortLabel(static_cast<PanicTrigger>(i0)) : "-",
           i0 < total ? static_cast<unsigned long>(s_panicTriggerCounts[i0]) : 0UL,
           i1 < total ? panicTriggerShortLabel(static_cast<PanicTrigger>(i1)) : "-",
           i1 < total ? static_cast<unsigned long>(s_panicTriggerCounts[i1]) : 0UL,
           i2 < total ? panicTriggerShortLabel(static_cast<PanicTrigger>(i2)) : "-",
           i2 < total ? static_cast<unsigned long>(s_panicTriggerCounts[i2]) : 0UL,
           i3 < total ? panicTriggerShortLabel(static_cast<PanicTrigger>(i3)) : "-",
           i3 < total ? static_cast<unsigned long>(s_panicTriggerCounts[i3]) : 0UL);
}

static void formatPanicDebugPage(uint32_t nowMs, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  auto panicDebugRotateMs = []() -> uint32_t {
    switch (g_settings.panicDebugRotate) {
      case 1:
        return 1000U;
      case 2:
        return 2000U;
      case 3:
        return 4000U;
      default:
        return 0U;
    }
  };
  constexpr size_t kShowPerPage = 4;
  const size_t total = static_cast<size_t>(PanicTrigger::Count);
  const size_t pageCount = (total + kShowPerPage - 1U) / kShowPerPage;
  const uint32_t pageMs = panicDebugRotateMs();
  const size_t page = pageCount > 0
                          ? (pageMs == 0U ? 0U : static_cast<size_t>((nowMs / pageMs) % pageCount))
                          : 0U;
  snprintf(out, outSize, "P%u/%u", static_cast<unsigned>(page + 1U), static_cast<unsigned>(pageCount));
}

void drawMidiDebugSurface(bool fullClear) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  if (fullClear) M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString("MIDI Debug", w / 2, 2);
  M5.Display.setTextSize(1);

  char stat[64];
  char bpmTxt[8];
  const uint16_t extBpm = transportExternalClockBpm();
  if (extBpm >= 40 && extBpm <= 300) {
    snprintf(bpmTxt, sizeof(bpmTxt), "%u", static_cast<unsigned>(extBpm));
  } else {
    snprintf(bpmTxt, sizeof(bpmTxt), "--");
  }
  snprintf(stat, sizeof(stat), "Clk:%s F:%s E:%s BPM:%s", transportExternalClockActive() ? "ext" : "int",
           g_settings.clkFollow ? "on" : "off", g_settings.clkFlashEdge ? "1/8" : "1/4", bpmTxt);
  M5.Display.drawString(stat, w / 2, 32);

  uint16_t aUsb = 0;
  uint16_t aBle = 0;
  uint16_t aDin = 0;
  for (uint8_t ch = 0; ch < 16; ++ch) {
    aUsb = static_cast<uint16_t>(aUsb + s_midiInState.activeNotesOnChannel(MidiSource::Usb, ch));
    aBle = static_cast<uint16_t>(aBle + s_midiInState.activeNotesOnChannel(MidiSource::Ble, ch));
    aDin = static_cast<uint16_t>(aDin + s_midiInState.activeNotesOnChannel(MidiSource::Din, ch));
  }
  char act[64];
  snprintf(act, sizeof(act), "A USB:%u BLE:%u DIN:%u", (unsigned)aUsb, (unsigned)aBle, (unsigned)aDin);
  M5.Display.drawString(act, w / 2, 44);
  const uint32_t nowMs = millis();
  const uint32_t rxNow = bleMidiRxBytesTotal();
  if (s_bleDbgRatePrevMs == 0) {
    s_bleDbgRatePrevMs = nowMs;
    s_bleDbgRatePrevRx = rxNow;
    s_bleDbgRateBps = 0;
  } else {
    const uint32_t dt = nowMs - s_bleDbgRatePrevMs;
    if (dt >= 1000U) {
      const uint32_t dr = rxNow - s_bleDbgRatePrevRx;
      s_bleDbgRateBps = static_cast<uint16_t>((dr * 1000U) / dt);
      if (dr > 0) {
        s_bleDbgLastActivityMs = nowMs;
        s_bleDbgPeakHeld = true;
      }
      if (s_bleDbgRateBps > s_bleDbgRatePeakBps) {
        s_bleDbgRatePeakBps = s_bleDbgRateBps;
      }
      s_bleDbgRatePrevMs = nowMs;
      s_bleDbgRatePrevRx = rxNow;
    }
  }
  uint32_t peakDecayMs = 0;
  switch (g_settings.bleDebugPeakDecay) {
    case 1:
      peakDecayMs = 5000U;
      break;
    case 2:
      peakDecayMs = 10000U;
      break;
    case 3:
      peakDecayMs = 30000U;
      break;
    default:
      peakDecayMs = 0U;
      break;
  }
  if (peakDecayMs > 0 && s_bleDbgLastActivityMs > 0 &&
      (nowMs - s_bleDbgLastActivityMs) > peakDecayMs && s_bleDbgRatePeakBps > s_bleDbgRateBps) {
    s_bleDbgRatePeakBps = s_bleDbgRateBps;
    s_bleDbgPeakHeld = false;
  }
  const uint32_t dinRxNow = dinMidiRxBytesTotal();
  if (s_dinDbgRatePrevMs == 0) {
    s_dinDbgRatePrevMs = nowMs;
    s_dinDbgRatePrevRx = dinRxNow;
    s_dinDbgRateBps = 0;
  } else {
    const uint32_t dt = nowMs - s_dinDbgRatePrevMs;
    if (dt >= 1000U) {
      const uint32_t dr = dinRxNow - s_dinDbgRatePrevRx;
      s_dinDbgRateBps = static_cast<uint16_t>((dr * 1000U) / dt);
      if (dr > 0) {
        s_dinDbgLastActivityMs = nowMs;
        s_dinDbgPeakHeld = true;
      }
      if (s_dinDbgRateBps > s_dinDbgRatePeakBps) {
        s_dinDbgRatePeakBps = s_dinDbgRateBps;
      }
      s_dinDbgRatePrevMs = nowMs;
      s_dinDbgRatePrevRx = dinRxNow;
    }
  }
  if (peakDecayMs > 0 && s_dinDbgLastActivityMs > 0 &&
      (nowMs - s_dinDbgLastActivityMs) > peakDecayMs && s_dinDbgRatePeakBps > s_dinDbgRateBps) {
    s_dinDbgRatePeakBps = s_dinDbgRateBps;
    s_dinDbgPeakHeld = false;
  }
  const uint32_t outTxNow = midiOutTxBytesTotal();
  if (s_outDbgRatePrevMs == 0) {
    s_outDbgRatePrevMs = nowMs;
    s_outDbgRatePrevTx = outTxNow;
    s_outDbgRateBps = 0;
  } else {
    const uint32_t dt = nowMs - s_outDbgRatePrevMs;
    if (dt >= 1000U) {
      const uint32_t db = outTxNow - s_outDbgRatePrevTx;
      s_outDbgRateBps = static_cast<uint16_t>((db * 1000U) / dt);
      if (s_outDbgRateBps > s_outDbgRatePeakBps) {
        s_outDbgRatePeakBps = s_outDbgRateBps;
      }
      s_outDbgRatePrevMs = nowMs;
      s_outDbgRatePrevTx = outTxNow;
    }
  }
  const uint32_t bleDrop = bleMidiDecodeDropTotal();
  if (bleDrop > s_bleDbgPrevDrop && bleMidiConnected()) {
    s_bleDbgWarnUntilMs = nowMs + 1500U;
  }
  s_bleDbgPrevDrop = bleDrop;
  const bool bleWarn = nowMs < s_bleDbgWarnUntilMs;
  M5.Display.setTextColor(bleWarn ? g_uiPalette.accentPress : g_uiPalette.hintText, g_uiPalette.bg);
  char bleStat[72];
  snprintf(bleStat, sizeof(bleStat), "BLE:%s RX:%lu %u/s P:%u%c D:%lu Q:%lu",
           bleMidiConnected() ? "Conn" : "Adv",
           static_cast<unsigned long>(rxNow), static_cast<unsigned>(s_bleDbgRateBps),
           static_cast<unsigned>(s_bleDbgRatePeakBps), s_bleDbgPeakHeld ? 'h' : 'a',
           static_cast<unsigned long>(bleDrop),
           static_cast<unsigned long>(midiIngressBleQueueDropTotal()));
  M5.Display.drawString(bleStat, w / 2, 54);
  char bleDropStat[72];
  snprintf(bleDropStat, sizeof(bleDropStat), "Drop ts:%lu dat:%lu tr:%lu",
           static_cast<unsigned long>(bleMidiDecodeDropTimestampTotal()),
           static_cast<unsigned long>(bleMidiDecodeDropInvalidDataTotal()),
           static_cast<unsigned long>(bleMidiDecodeDropTruncatedTotal()));
  M5.Display.drawString(bleDropStat, w / 2, 64);
  char dinStat[64];
  snprintf(dinStat, sizeof(dinStat), "DIN:%s RX:%lu %u/s P:%u%c D:%lu", dinMidiReady() ? "Ready" : "Off",
           static_cast<unsigned long>(dinRxNow), static_cast<unsigned>(s_dinDbgRateBps),
           static_cast<unsigned>(s_dinDbgRatePeakBps), s_dinDbgPeakHeld ? 'h' : 'a',
           static_cast<unsigned long>(midiIngressDinQueueDropTotal()));
  M5.Display.drawString(dinStat, w / 2, 74);
  char panicStat[96];
  formatPanicDebugLine(nowMs, panicStat, sizeof(panicStat));
  M5.Display.drawString(panicStat, w / 2, 84);
  char panicPage[12];
  formatPanicDebugPage(nowMs, panicPage, sizeof(panicPage));
  M5.Display.setTextDatum(top_right);
  M5.Display.drawString(panicPage, w - 6, 84);
  M5.Display.setTextDatum(top_center);
  char outStat[96];
  snprintf(outStat, sizeof(outStat), "OUT TX:%lu %u/s P:%u NOn:%lu NOff:%lu CC:%lu",
           static_cast<unsigned long>(outTxNow), static_cast<unsigned>(s_outDbgRateBps),
           static_cast<unsigned>(s_outDbgRatePeakBps),
           static_cast<unsigned long>(midiOutTxNoteOnTotal()),
           static_cast<unsigned long>(midiOutTxNoteOffTotal()),
           static_cast<unsigned long>(midiOutTxControlChangeTotal()));
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString(outStat, w / 2, 94);
  const char* sugWnd = g_settings.suggestStableWindow == 0
                           ? "0.6s"
                           : (g_settings.suggestStableWindow == 1
                                  ? "1.0s"
                                  : (g_settings.suggestStableWindow == 2
                                         ? "1.4s"
                                         : (g_settings.suggestStableWindow == 3 ? "2.0s" : "3.0s")));
  const char* sugGap = g_settings.suggestStableGap == 0
                           ? "8"
                           : (g_settings.suggestStableGap == 1
                                  ? "12"
                                  : (g_settings.suggestStableGap == 2
                                         ? "18"
                                         : (g_settings.suggestStableGap == 3 ? "24" : "32")));
  char sugTune[40];
  snprintf(sugTune, sizeof(sugTune), "Prf:%c Lk:%c Wnd:%s Gap:%s",
           g_settings.suggestProfile == 0 ? 'A' : 'B', g_settings.suggestProfileLock ? 'Y' : 'N', sugWnd,
           sugGap);
  M5.Display.drawString(sugTune, w / 2, 104);
  if (s_midiSuggestTop3UntilMs > nowMs && s_midiSuggestTop3[0][0] != '\0') {
    char sugStat[96];
    snprintf(sugStat, sizeof(sugStat), "SUG%c %s(%d) %s(%d) %s(%d)",
             (s_midiSuggestStableUntilMs > nowMs) ? 'S' : '-', s_midiSuggestTop3[0],
             static_cast<int>(s_midiSuggestTop3Score[0]), s_midiSuggestTop3[1],
             static_cast<int>(s_midiSuggestTop3Score[1]), s_midiSuggestTop3[2],
             static_cast<int>(s_midiSuggestTop3Score[2]));
    M5.Display.drawString(sugStat, w / 2, 114);
  }
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);

  const int chipY = 126;
  const int chipH = 14;
  const int chipGap = 8;
  const int chipW = (w - 12 - 2 * chipGap) / 3;
  g_midiDbgSourceChip = {6, chipY, chipW, chipH};
  g_midiDbgTypeChip = {6 + chipW + chipGap, chipY, chipW, chipH};
  g_midiDbgResetChip = {6 + 2 * (chipW + chipGap), chipY, chipW, chipH};
  drawRoundedButton(g_midiDbgSourceChip, g_uiPalette.panelMuted, midiDbgSourceFilterLabel(), 1);
  drawRoundedButton(g_midiDbgTypeChip, g_uiPalette.panelMuted, midiDbgTypeFilterLabel(), 1);
  drawRoundedButton(g_midiDbgResetChip, g_uiPalette.panelMuted, "Filters:Reset", 1);

  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
  M5.Display.drawString("Recent events:", 6, chipY + chipH + 3);
  int y = chipY + chipH + 15;
  const int maxRows = max(1, (h - kBezelBarH - y - 4) / 12);
  int shown = 0;
  for (int i = 0; i < 64 && shown < maxRows; ++i) {
    MidiEventHistoryItem ev{};
    if (!s_midiEventHistory.getNewestFirst(static_cast<size_t>(i), &ev)) break;
    if (!midiDbgEventMatchesFilters(ev)) continue;
    char line[80];
    const uint32_t lineNowMs = millis();
    const uint32_t ageMs = lineNowMs >= ev.atMs ? (lineNowMs - ev.atMs) : 0;
    if (ev.type == MidiEventType::SongPosition) {
      snprintf(line, sizeof(line), "%4lums %s %-5s %u", (unsigned long)ageMs,
               midiSrcShortLabel(ev.source), midiTypeShortLabel(ev.type), (unsigned)ev.value14);
    } else {
      snprintf(line, sizeof(line), "%4lums %s %-5s ch%u %3u %3u", (unsigned long)ageMs,
               midiSrcShortLabel(ev.source), midiTypeShortLabel(ev.type), (unsigned)(ev.channel + 1),
               (unsigned)ev.data1, (unsigned)ev.data2);
    }
    M5.Display.drawString(line, 6, y);
    y += 12;
    shown++;
  }
  drawTopSystemStatus(w, 2, nullptr, 0);
  drawBezelBarStrip();
  M5.Display.endWrite();
}

void processMidiDebugTouch(uint8_t touchCount, int w, int h) {
  if (touchCount > 0) {
    if (!wasTouchActive) {
      s_playCategoryTouchStartMs = millis();
    }
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
    wasTouchActive = true;
    return;
  }
  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;
  layoutBottomBezels(w, h);
  if (pointInRect(hx, hy, g_bezelSelect)) {
    panicForTrigger(PanicTrigger::EnterSettings);
    g_screen = Screen::Settings;
    drawSettingsUi();
    return;
  }
  if (pointInRect(hx, hy, g_bezelBack)) {
    navigateMainRing(-1);
    return;
  }
  if (pointInRect(hx, hy, g_bezelFwd)) {
    navigateMainRing(1);
    return;
  }
  if (pointInRect(hx, hy, g_midiDbgSourceChip)) {
    s_midiDbgSourceFilter++;
    if (s_midiDbgSourceFilter > 2) s_midiDbgSourceFilter = -1;
    drawMidiDebugSurface(false);
    return;
  }
  if (pointInRect(hx, hy, g_midiDbgTypeChip)) {
    static const MidiEventType kTypeCycle[] = {
        MidiEventType::NoteOn, MidiEventType::NoteOff, MidiEventType::ControlChange, MidiEventType::Clock,
        MidiEventType::Start,  MidiEventType::Stop,    MidiEventType::SongPosition};
    if (s_midiDbgTypeFilter < 0) {
      s_midiDbgTypeFilter = static_cast<int8_t>(kTypeCycle[0]);
    } else {
      int idx = -1;
      for (int i = 0; i < static_cast<int>(sizeof(kTypeCycle) / sizeof(kTypeCycle[0])); ++i) {
        if (s_midiDbgTypeFilter == static_cast<int8_t>(kTypeCycle[i])) {
          idx = i;
          break;
        }
      }
      if (idx < 0 || idx + 1 >= static_cast<int>(sizeof(kTypeCycle) / sizeof(kTypeCycle[0]))) {
        s_midiDbgTypeFilter = -1;
      } else {
        s_midiDbgTypeFilter = static_cast<int8_t>(kTypeCycle[idx + 1]);
      }
    }
    drawMidiDebugSurface(false);
    return;
  }
  if (pointInRect(hx, hy, g_midiDbgResetChip)) {
    s_midiDbgSourceFilter = -1;
    s_midiDbgTypeFilter = -1;
    drawMidiDebugSurface(false);
    return;
  }
  drawMidiDebugSurface(false);
}

}  // namespace m5chords_app
