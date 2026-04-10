static int transportDropOptionCount(int8_t kind) {
  switch (kind) {
    case kTransportDropMidiOut:
      return 16;
    case kTransportDropMidiIn:
      return 17;
    case kTransportDropClock:
      return 4;
    case kTransportDropStrum:
      return kArpeggiatorModeCount;
    default:
      return 0;
  }
}

static int transportDropCurrentIndex(int8_t kind) {
  switch (kind) {
    case kTransportDropMidiOut:
      return static_cast<int>(g_settings.midiOutChannel) - 1;
    case kTransportDropMidiIn:
      return (g_settings.midiInChannelUsb == 0) ? 0 : static_cast<int>(g_settings.midiInChannelUsb);
    case kTransportDropClock:
      return static_cast<int>(g_settings.midiClockSource);
    case kTransportDropStrum:
      return static_cast<int>(g_settings.arpeggiatorMode);
    default:
      return 0;
  }
}

static void transportDropFormatOption(int8_t kind, int opt, char* buf, size_t n) {
  switch (kind) {
    case kTransportDropMidiOut:
      snprintf(buf, n, "Channel %u", static_cast<unsigned>(opt + 1));
      break;
    case kTransportDropMidiIn:
      if (opt == 0) {
        snprintf(buf, n, "OMNI");
      } else {
        snprintf(buf, n, "Channel %u", static_cast<unsigned>(opt));
      }
      break;
    case kTransportDropClock:
      snprintf(buf, n, "%s", midiClockSourceLabel(static_cast<uint8_t>(opt)));
      break;
    case kTransportDropStrum:
      snprintf(buf, n, "%s", arpeggiatorModeLabel(static_cast<uint8_t>(opt)));
      break;
    default:
      buf[0] = '\0';
      break;
  }
}

static void transportDropApply(int8_t kind, int opt) {
  switch (kind) {
    case kTransportDropMidiOut:
      g_settings.midiOutChannel = static_cast<uint8_t>(opt + 1);
      break;
    case kTransportDropMidiIn:
      g_settings.midiInChannelUsb = static_cast<uint8_t>(opt == 0 ? 0 : opt);
      break;
    case kTransportDropClock:
      g_settings.midiClockSource = static_cast<uint8_t>(opt);
      break;
    case kTransportDropStrum:
      g_settings.arpeggiatorMode = static_cast<uint8_t>(opt);
      break;
    default:
      break;
  }
  g_settings.normalize();
  settingsSave(g_settings);
}

static int transportDropVisibleRows(int h) {
  constexpr int kRowH = 38;
  const int listTop = 50;
  const int listBottom = h - kBezelBarH - 22;
  const int maxH = max(kRowH * 3, listBottom - listTop);
  return max(3, maxH / kRowH);
}

static void transportComputeDropLayout(int w, int h, int* outX, int* outY, int* outW, int* rowH,
                                       int* firstOpt, int* visCount, int* totalOpts) {
  *rowH = 38;
  *totalOpts = transportDropOptionCount(s_transportDropKind);
  *visCount = min(*totalOpts, transportDropVisibleRows(h));
  *outW = w - 16;
  *outX = 8;
  *outY = 38;
  *firstOpt = s_transportDropScroll;
  if (*firstOpt + *visCount > *totalOpts) {
    *firstOpt = max(0, *totalOpts - *visCount);
    s_transportDropScroll = *firstOpt;
  }
}

static int transportHitDropdown(int px, int py, int w, int h) {
  int dx, dy, dw, rh, first, vis, tot;
  transportComputeDropLayout(w, h, &dx, &dy, &dw, &rh, &first, &vis, &tot);
  if (px < dx || px >= dx + dw || py < dy || py >= dy + vis * rh) return -1;
  const int row = (py - dy) / rh;
  const int idx = first + row;
  if (idx < 0 || idx >= tot) return -1;
  return idx;
}

static void transportDropScrollInit(int w, int h) {
  if (s_transportDropKind < 0) return;
  const int cnt = transportDropOptionCount(s_transportDropKind);
  const int idx = transportDropCurrentIndex(s_transportDropKind);
  const int vis = transportDropVisibleRows(h);
  int scroll = idx - vis / 2;
  if (scroll < 0) scroll = 0;
  const int maxScroll = max(0, cnt - vis);
  if (scroll > maxScroll) scroll = maxScroll;
  s_transportDropScroll = scroll;
}

static void transportDropDismiss() {
  s_transportDropKind = -1;
  s_transportDropDragLastY = -1;
  s_transportDropFingerScrollCount = 0;
}

static void transportDropOpen(int8_t kind, int w, int h) {
  if (kind < kTransportDropMidiOut || kind > kTransportDropStrum) {
    transportDropDismiss();
    return;
  }
  s_transportDropKind = kind;
  s_transportDropDragLastY = -1;
  s_transportDropFingerScrollCount = 0;
  transportDropScrollInit(w, h);
}

static void layoutTransportRects(int w, int h) {
  constexpr int margin = 8;
  constexpr int row1y = 22;
  constexpr int btnH = 38;
  constexpr int gap = 8;
  const int totalW = w - 2 * margin;
  const int third = (totalW - 2 * gap) / 3;
  g_trPlay = {margin, row1y, third, btnH};
  g_trStop = {margin + third + gap, row1y, third, btnH};
  g_trRec = {margin + 2 * (third + gap), row1y, third, btnH};

  constexpr int subGap = 4;
  const int row2y = row1y + btnH + gap;
  const int smallH = 26;
  const int miniW = (third - subGap) / 2;
  g_trMetro = {margin, row2y, miniW, smallH};
  g_trCntIn = {margin + miniW + subGap, row2y, miniW, smallH};

  const int row3y = row2y + smallH + gap;
  constexpr int midiRowH = 34;
  const int ddw = (totalW - 3 * gap) / 4;
  g_trMidiOutBtn = {margin, row3y, ddw, midiRowH};
  g_trMidiInBtn = {margin + ddw + gap, row3y, ddw, midiRowH};
  g_trSyncBtn = {margin + 2 * (ddw + gap), row3y, ddw, midiRowH};
  g_trStrumBtn = {margin + 3 * (ddw + gap), row3y, ddw, midiRowH};

  const int infoY = row3y + midiRowH + 6;
  const int infoH = h - kBezelBarH - infoY - 4;
  const int halfCol = (totalW - gap) / 2;
  g_trBpmTap = {margin, infoY, halfCol, infoH};
  g_trStepDisplay = {margin + halfCol + gap, infoY, halfCol, infoH};
}

/// Repaint only the step readout (avoids full Transport redraw every clock tick).
static void transportRedrawStepDisplayOnly() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutTransportRects(w, h);
  M5.Display.startWrite();
  const Rect& sd = g_trStepDisplay;
  M5.Display.fillRect(sd.x, sd.y, sd.w, sd.h, g_uiPalette.bg);
  M5.Display.drawRoundRect(sd.x, sd.y, sd.w, sd.h, 6, g_uiPalette.settingsBtnBorder);
  M5.Display.setFont(nullptr);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("Step 1-16", sd.x + sd.w / 2, sd.y + 2);
  char stepMain[8];
  char stepSub[16];
  if (transportIsCountIn()) {
    const uint8_t cn = transportCountInNumber();
    snprintf(stepMain, sizeof(stepMain), "%u", static_cast<unsigned>(cn));
    snprintf(stepSub, sizeof(stepSub), "cnt in");
  } else if (transportIsPlaying() || transportIsPaused()) {
    const uint8_t au = transportAudibleStep();
    const unsigned disp = static_cast<unsigned>(au) + 1U;
    const unsigned beatInBar = static_cast<unsigned>(au) / 4U + 1U;
    const unsigned subInBeat = static_cast<unsigned>(au) % 4U + 1U;
    snprintf(stepMain, sizeof(stepMain), "%u", disp);
    snprintf(stepSub, sizeof(stepSub), "b%u s%u", beatInBar, subInBeat);
  } else {
    snprintf(stepMain, sizeof(stepMain), "--");
    snprintf(stepSub, sizeof(stepSub), " ");
  }
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(stepMain, sd.x + sd.w / 2, sd.y + sd.h / 2 - 2);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
  M5.Display.drawString(stepSub, sd.x + sd.w / 2, sd.y + sd.h / 2 + 22);
  M5.Display.endWrite();
}

void drawTransportSurface(bool fullClear) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutTransportRects(w, h);
  M5.Display.startWrite();
  if (fullClear) M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  layoutBottomBezels(w, h);

  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  if (s_transportDropKind >= 0) {
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
    M5.Display.drawString("Transport", w / 2, 2);
    const char* dropTitle = "MIDI out";
    if (s_transportDropKind == kTransportDropMidiIn) {
      dropTitle = "MIDI in USB";
    } else if (s_transportDropKind == kTransportDropClock) {
      dropTitle = "Clock source";
    } else if (s_transportDropKind == kTransportDropStrum) {
      dropTitle = "Default strum";
    }
    M5.Display.drawString(dropTitle, w / 2, 22);

    int dx, dy, dw, rh, first, vis, tot;
    transportComputeDropLayout(w, h, &dx, &dy, &dw, &rh, &first, &vis, &tot);
    for (int i = 0; i < vis; ++i) {
      const int opt = first + i;
      const Rect r = {dx, dy + i * rh, dw, rh - 2};
      char lab[40];
      transportDropFormatOption(s_transportDropKind, opt, lab, sizeof(lab));
      const bool sel = (opt == transportDropCurrentIndex(s_transportDropKind));
      const uint16_t bg = sel ? g_uiPalette.settingsBtnActive : g_uiPalette.panelMuted;
      drawRoundedButton(r, bg, lab, 2);
    }
    drawTopSystemStatus(w, 2, nullptr, 0);
    drawBezelBarStrip();
    M5.Display.endWrite();
    return;
  }

  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Transport", w / 2, 2);
  M5.Display.setTextSize(1);

  const char* playLab = "Play";
  if (transportIsPaused()) {
    playLab = "Play";
  } else if (transportIsPlaying() || transportIsCountIn()) {
    playLab = "Pause";
  }

  drawRoundedButton(g_trPlay, g_uiPalette.settingsBtnActive, playLab, 2);
  drawRoundedButton(g_trStop, g_uiPalette.settingsBtnIdle, "Stop", 2);
  char rlab[16];
  snprintf(rlab, sizeof(rlab), "Rec %s", transportRecordArmed() ? "ARM" : "");
  drawRoundedButton(g_trRec, transportRecordArmed() ? g_uiPalette.danger : g_uiPalette.panelMuted, rlab,
                    2);

  char ml[20];
  char cl[24];
  snprintf(ml, sizeof(ml), "M %s", g_prefsMetronome ? "on" : "off");
  snprintf(cl, sizeof(cl), "C %s", g_prefsCountIn ? "on" : "off");
  drawRoundedButton(g_trMetro, g_prefsMetronome ? g_uiPalette.seqLaneTab : g_uiPalette.panelMuted, ml,
                    1);
  drawRoundedButton(g_trCntIn, g_prefsCountIn ? g_uiPalette.seqLaneTab : g_uiPalette.panelMuted, cl,
                    1);

  char mo[20];
  char mi[20];
  char sy[24];
  char st[24];
  snprintf(mo, sizeof(mo), "Out %u", (unsigned)g_settings.midiOutChannel);
  if (g_settings.midiInChannelUsb == 0) {
    snprintf(mi, sizeof(mi), "InUSB OMNI");
  } else {
    snprintf(mi, sizeof(mi), "InUSB %u", (unsigned)g_settings.midiInChannelUsb);
  }
  snprintf(sy, sizeof(sy), "%s", midiClockSourceLabel(g_settings.midiClockSource));
  snprintf(st, sizeof(st), "%s", arpeggiatorModeLabel(g_settings.arpeggiatorMode));
  drawRoundedButton(g_trMidiOutBtn, g_uiPalette.panelMuted, mo, 1);
  drawRoundedButton(g_trMidiInBtn, g_uiPalette.panelMuted, mi, 1);
  drawRoundedButton(g_trSyncBtn, g_uiPalette.panelMuted, sy, 1);
  drawRoundedButton(g_trStrumBtn, g_uiPalette.panelMuted, st, 1);

  M5.Display.drawRoundRect(g_trBpmTap.x, g_trBpmTap.y, g_trBpmTap.w, g_trBpmTap.h, 6,
                           g_uiPalette.settingsBtnBorder);
  M5.Display.drawRoundRect(g_trStepDisplay.x, g_trStepDisplay.y, g_trStepDisplay.w, g_trStepDisplay.h, 6,
                           g_uiPalette.settingsBtnBorder);

  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_center);
  M5.Display.drawString("BPM tap / hold", g_trBpmTap.x + g_trBpmTap.w / 2, g_trBpmTap.y + 2);

  char bpmStr[8];
  snprintf(bpmStr, sizeof(bpmStr), "%u", static_cast<unsigned>(g_projectBpm));
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(bpmStr, g_trBpmTap.x + g_trBpmTap.w / 2, g_trBpmTap.y + g_trBpmTap.h / 2 + 4);

  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("Step 1-16", g_trStepDisplay.x + g_trStepDisplay.w / 2, g_trStepDisplay.y + 2);

  char stepMain[8];
  char stepSub[16];
  if (transportIsCountIn()) {
    const uint8_t cn = transportCountInNumber();
    snprintf(stepMain, sizeof(stepMain), "%u", static_cast<unsigned>(cn));
    snprintf(stepSub, sizeof(stepSub), "cnt in");
  } else if (transportIsPlaying() || transportIsPaused()) {
    const uint8_t au = transportAudibleStep();
    const unsigned disp = static_cast<unsigned>(au) + 1U;
    const unsigned beatInBar = static_cast<unsigned>(au) / 4U + 1U;
    const unsigned subInBeat = static_cast<unsigned>(au) % 4U + 1U;
    snprintf(stepMain, sizeof(stepMain), "%u", disp);
    snprintf(stepSub, sizeof(stepSub), "b%u s%u", beatInBar, subInBeat);
  } else {
    snprintf(stepMain, sizeof(stepMain), "--");
    snprintf(stepSub, sizeof(stepSub), " ");
  }
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(stepMain, g_trStepDisplay.x + g_trStepDisplay.w / 2,
                         g_trStepDisplay.y + g_trStepDisplay.h / 2 - 2);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
  M5.Display.drawString(stepSub, g_trStepDisplay.x + g_trStepDisplay.w / 2,
                         g_trStepDisplay.y + g_trStepDisplay.h / 2 + 22);

  drawTopSystemStatus(w, 2, nullptr, 0);
  drawBezelBarStrip();
  M5.Display.endWrite();
}

void drawTransportBpmEdit() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutBottomBezels(w, h);
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString("BPM", w / 2, 8);

  char nb[8];
  snprintf(nb, sizeof(nb), "%u", static_cast<unsigned>(g_bpmEditValue));
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(nb, w / 2, 52);

  constexpr int m = 8;
  const int bw = (w - m * 5) / 4;
  const int rowY = 88;
  g_bpmEditM5 = {m, rowY, bw, 32};
  g_bpmEditM1 = {m + (bw + m), rowY, bw, 32};
  g_bpmEditP1 = {m + 2 * (bw + m), rowY, bw, 32};
  g_bpmEditP5 = {m + 3 * (bw + m), rowY, bw, 32};
  drawRoundedButton(g_bpmEditM5, g_uiPalette.panelMuted, "-5", 2);
  drawRoundedButton(g_bpmEditM1, g_uiPalette.panelMuted, "-1", 2);
  drawRoundedButton(g_bpmEditP1, g_uiPalette.panelMuted, "+1", 2);
  drawRoundedButton(g_bpmEditP5, g_uiPalette.panelMuted, "+5", 2);

  const int btnY = rowY + 42;
  const int btnW = (w - 3 * m) / 2;
  g_bpmEditOk = {m, btnY, btnW, 36};
  g_bpmEditCancel = {m * 2 + btnW, btnY, btnW, 36};
  drawRoundedButton(g_bpmEditOk, g_uiPalette.settingsBtnActive, "OK", 2);
  drawRoundedButton(g_bpmEditCancel, g_uiPalette.keyPickCancel, "Cancel", 2);

  drawTopSystemStatus(w, 2, nullptr, 0);
  drawBezelBarStrip();
  M5.Display.endWrite();
}

void drawTransportTapTempo() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutBottomBezels(w, h);
  M5.Display.startWrite();
  M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString("Tap tempo", w / 2, 8);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("Two or more taps set BPM", w / 2, 30);

  char nb[8];
  snprintf(nb, sizeof(nb), "%u", static_cast<unsigned>(s_tapTempoPreviewBpm));
  M5.Display.setTextSize(4);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString(nb, w / 2, h / 2 - 24);

  g_trTapTempoDone = {8, h - kBezelBarH - 44, w - 16, 36};
  drawRoundedButton(g_trTapTempoDone, g_uiPalette.settingsBtnActive, "Use BPM", 2);

  drawTopSystemStatus(w, 2, nullptr, 0);
  drawBezelBarStrip();
  M5.Display.endWrite();
}

static void bpmEditAdjust(int delta) {
  int v = static_cast<int>(g_bpmEditValue) + delta;
  if (v < 40) v = 40;
  if (v > 300) v = 300;
  g_bpmEditValue = static_cast<uint16_t>(v);
}

void processTransportBpmEditTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);
  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    const auto& d = M5.Touch.getDetail(0);
    if (d.isPressed()) {
      g_lastTouchX = d.x;
      g_lastTouchY = d.y;
    }
    wasTouchActive = true;
    return;
  }

  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;

  if (pointInRect(hx, hy, g_bpmEditOk)) {
    g_projectBpm = g_bpmEditValue;
    projectBpmSave(g_projectBpm);
    g_screen = Screen::Transport;
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_bpmEditCancel)) {
    g_screen = Screen::Transport;
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_bpmEditM5)) {
    bpmEditAdjust(-5);
    drawTransportBpmEdit();
    return;
  }
  if (pointInRect(hx, hy, g_bpmEditM1)) {
    bpmEditAdjust(-1);
    drawTransportBpmEdit();
    return;
  }
  if (pointInRect(hx, hy, g_bpmEditP1)) {
    bpmEditAdjust(1);
    drawTransportBpmEdit();
    return;
  }
  if (pointInRect(hx, hy, g_bpmEditP5)) {
    bpmEditAdjust(5);
    drawTransportBpmEdit();
    return;
  }
  drawTransportBpmEdit();
}

void processTransportTapTempoTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);
  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    const auto& d = M5.Touch.getDetail(0);
    if (d.isPressed()) {
      g_lastTouchX = d.x;
      g_lastTouchY = d.y;
    }
    wasTouchActive = true;
    return;
  }

  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;
  const uint32_t now = millis();

  if (pointInRect(hx, hy, g_trTapTempoDone)) {
    g_projectBpm = s_tapTempoPreviewBpm;
    projectBpmSave(g_projectBpm);
    g_screen = Screen::Transport;
    drawTransportSurface();
    return;
  }
  if (pointInRect(hx, hy, g_bezelBack) || pointInRect(hx, hy, g_bezelFwd) ||
      pointInRect(hx, hy, g_bezelSelect)) {
    g_screen = Screen::Transport;
    drawTransportSurface();
    return;
  }

  if (s_tapTempoPrevTapMs > 0U) {
    const uint32_t dt = now - s_tapTempoPrevTapMs;
    if (dt >= 200U && dt <= 2500U) {
      uint32_t bpm = 60000UL / dt;
      if (bpm < 40U) bpm = 40U;
      if (bpm > 300U) bpm = 300U;
      s_tapTempoPreviewBpm = static_cast<uint16_t>(bpm);
    }
  }
  s_tapTempoPrevTapMs = now;
  drawTransportTapTempo();
}

void processTransportTouch(uint8_t touchCount, int w, int h) {
  if (s_transportIgnoreUntilTouchUp) {
    if (touchCount > 0) {
      wasTouchActive = true;
      return;
    }
    s_transportIgnoreUntilTouchUp = false;
    wasTouchActive = false;
    s_transportTouchDown = false;
    s_transportDropDragLastY = -1;
    s_transportDropFingerScrollCount = 0;
    return;
  }
  layoutBottomBezels(w, h);
  layoutTransportRects(w, h);

  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    const auto& d = M5.Touch.getDetail(0);
    if (d.isPressed()) {
      g_lastTouchX = d.x;
      g_lastTouchY = d.y;
      if (s_transportDropKind >= 0) {
        int ddx, ddy, ddw, drh, dfirst, dvis, dtot;
        transportComputeDropLayout(w, h, &ddx, &ddy, &ddw, &drh, &dfirst, &dvis, &dtot);
        const Rect listHit = {ddx, ddy, ddw, dvis * drh};
        if (pointInRect(d.x, d.y, listHit)) {
          if (s_transportDropDragLastY >= 0) {
            const int delta = d.y - s_transportDropDragLastY;
            if (delta > 22) {
              if (s_transportDropScroll > 0) {
                --s_transportDropScroll;
                ++s_transportDropFingerScrollCount;
                drawTransportSurface(false);
              }
              s_transportDropDragLastY = d.y;
            } else if (delta < -22) {
              const int cnt = transportDropOptionCount(s_transportDropKind);
              if (s_transportDropScroll + dvis < cnt) {
                ++s_transportDropScroll;
                ++s_transportDropFingerScrollCount;
                drawTransportSurface(false);
              }
              s_transportDropDragLastY = d.y;
            }
          } else {
            s_transportDropDragLastY = d.y;
          }
        } else {
          s_transportDropDragLastY = -1;
        }
      }
      if (!s_transportTouchDown) {
        s_transportTouchDown = true;
        s_transportTouchStartMs = millis();
        s_transportTouchStartX = d.x;
        s_transportTouchStartY = d.y;
      }
      if (s_transportDropKind < 0 && pointInRect(s_transportTouchStartX, s_transportTouchStartY, g_trBpmTap)) {
        const uint32_t elapsed = millis() - s_transportTouchStartMs;
        drawHoldProgressStrip(HoldProgressOwner::TransportBpm, elapsed, transportBpmLongMs(),
                              "Hold BPM: tap-tempo");
      } else {
        clearHoldProgressStrip(HoldProgressOwner::TransportBpm);
      }
    }
    wasTouchActive = true;
    return;
  }

  if (!wasTouchActive) return;
  wasTouchActive = false;
  clearHoldProgressStrip(HoldProgressOwner::TransportBpm);
  s_transportTouchDown = false;
  s_transportDropDragLastY = -1;
  const int fingerScrollsThisGesture = s_transportDropFingerScrollCount;
  s_transportDropFingerScrollCount = 0;

  const int hx = s_transportTouchStartX;
  const int hy = s_transportTouchStartY;
  const int relX = g_lastTouchX;
  const int relY = g_lastTouchY;
  const uint32_t dur = millis() - s_transportTouchStartMs;
  const uint32_t now = millis();

  if (s_transportDropKind >= 0) {
    if (pointInRect(hx, hy, g_bezelBack)) {
      if (s_transportDropScroll > 0) {
        --s_transportDropScroll;
        drawTransportSurface(false);
      } else {
        transportDropDismiss();
        drawTransportSurface(false);
      }
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      const int cnt = transportDropOptionCount(s_transportDropKind);
      const int vis = transportDropVisibleRows(h);
      if (s_transportDropScroll + vis < cnt) {
        ++s_transportDropScroll;
        drawTransportSurface(false);
      }
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      transportDropDismiss();
      drawTransportSurface(false);
      return;
    }
    const int pick = transportHitDropdown(relX, relY, w, h);
    if (pick >= 0) {
      const bool movedPastThreshold = touchMovedPastSuppressThreshold(hx, hy, relX, relY);
      if (fingerScrollsThisGesture > 0 || movedPastThreshold) {
        drawTransportSurface(false);
        return;
      }
      transportDropApply(s_transportDropKind, pick);
      transportDropDismiss();
      drawTransportSurface(false);
      return;
    }
    transportDropDismiss();
    drawTransportSurface(false);
    return;
  }

  if (pointInRect(hx, hy, g_bezelBack) || pointInRect(hx, hy, g_bezelFwd) ||
      pointInRect(hx, hy, g_bezelSelect)) {
    closeTransportDrawer();
    return;
  }

  if (pointInRect(hx, hy, g_trBpmTap)) {
    transportDropDismiss();
    if (dur >= transportBpmLongMs()) {
      s_tapTempoPrevTapMs = 0;
      s_tapTempoPreviewBpm = g_projectBpm;
      g_screen = Screen::TransportTapTempo;
      drawTransportTapTempo();
      return;
    }
    g_bpmEditValue = g_projectBpm;
    g_screen = Screen::TransportBpmEdit;
    drawTransportBpmEdit();
    return;
  }

  if (pointInRect(hx, hy, g_trMidiOutBtn)) {
    transportDropOpen(kTransportDropMidiOut, w, h);
    drawTransportSurface(false);
    return;
  }
  if (pointInRect(hx, hy, g_trMidiInBtn)) {
    transportDropOpen(kTransportDropMidiIn, w, h);
    drawTransportSurface(false);
    return;
  }
  if (pointInRect(hx, hy, g_trSyncBtn)) {
    transportDropOpen(kTransportDropClock, w, h);
    drawTransportSurface(false);
    return;
  }
  if (pointInRect(hx, hy, g_trStrumBtn)) {
    transportDropOpen(kTransportDropStrum, w, h);
    drawTransportSurface(false);
    return;
  }

  if (pointInRect(hx, hy, g_trPlay)) {
    if (transportIsPaused()) {
      transportTogglePlayPause(now);
    } else if (transportIsPlaying() || transportIsCountIn()) {
      transportTogglePlayPause(now);
    } else {
      if (transportRecordArmed()) {
        g_screen = Screen::Play;
        transportBeginLiveRecording(now);
        drawPlaySurface();
      } else {
        transportStartMetronomeOnly(now);
        drawTransportSurface(false);
      }
    }
    return;
  }
  if (pointInRect(hx, hy, g_trStop)) {
    panicForTrigger(PanicTrigger::TransportStop);
    transportStop();
    drawTransportSurface(false);
    return;
  }
  if (pointInRect(hx, hy, g_trRec)) {
    transportSetRecordArmed(!transportRecordArmed());
    drawTransportSurface(false);
    return;
  }
  if (pointInRect(hx, hy, g_trMetro)) {
    g_prefsMetronome = !g_prefsMetronome;
    transportPrefsSave();
    drawTransportSurface(false);
    return;
  }
  if (pointInRect(hx, hy, g_trCntIn)) {
    g_prefsCountIn = !g_prefsCountIn;
    transportPrefsSave();
    drawTransportSurface(false);
    return;
  }
  drawTransportSurface(false);
}
