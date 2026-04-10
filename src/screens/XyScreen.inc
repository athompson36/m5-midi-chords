static const char* xyCurveLabel(uint8_t curve);
static const char* xyReleaseModeLabel();

static Rect s_xyDrReturn{};
static Rect s_xyDrRec{};

static void layoutXyDrawerControlRects(int screenW, int screenH) {
  const ::ui::Rect drawer = ::ui::makeTopDrawerRect(screenW, screenH, 120);
  const ::ui::Rect content = ::ui::makeDrawerContentRect(drawer);
  const int gap = ::ui::LayoutMetrics::ControlGap;
  ::ui::Rect row1 = ::ui::makeStripRect(content, 4, 26);
  ::ui::Rect r3[3];
  ::ui::layout3Up(row1, r3, gap);
  g_xyCfgChRect = {r3[0].x, r3[0].y, r3[0].w, r3[0].h};
  g_xyCfgCcARect = {r3[1].x, r3[1].y, r3[1].w, r3[1].h};
  g_xyCfgCcBRect = {r3[2].x, r3[2].y, r3[2].w, r3[2].h};
  ::ui::Rect row2 = ::ui::makeStripRect(content, 36, 26);
  ::ui::Rect r2a[2];
  ::ui::layout2Up(row2, r2a, gap);
  g_xyCfgCvARect = {r2a[0].x, r2a[0].y, r2a[0].w, r2a[0].h};
  g_xyCfgCvBRect = {r2a[1].x, r2a[1].y, r2a[1].w, r2a[1].h};
  ::ui::Rect row3 = ::ui::makeStripRect(content, 68, 26);
  ::ui::Rect r2b[2];
  ::ui::layout2Up(row3, r2b, gap);
  s_xyDrReturn = {r2b[0].x, r2b[0].y, r2b[0].w, r2b[0].h};
  s_xyDrRec = {r2b[1].x, r2b[1].y, r2b[1].w, r2b[1].h};
}

void drawXyTopDrawer(::ui::DisplayAdapter& d) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutXyDrawerControlRects(w, h);
  ::ui::DrawerSheetSpec sheet{};
  sheet.bounds = ::ui::makeTopDrawerRect(w, h, 120);
  sheet.title = "X-Y setup";
  sheet.showHandle = true;
  ::ui::drawDrawerSheet(d, sheet);

  char bl[24];
  snprintf(bl, sizeof(bl), "CH%u", (unsigned)g_xyOutChannel);
  drawRoundedButton(g_xyCfgChRect, g_uiPalette.panelMuted, bl, 1);
  snprintf(bl, sizeof(bl), "X CC%u", (unsigned)g_xyCcA);
  drawRoundedButton(g_xyCfgCcARect, g_uiPalette.panelMuted, bl, 1);
  snprintf(bl, sizeof(bl), "Y CC%u", (unsigned)g_xyCcB);
  drawRoundedButton(g_xyCfgCcBRect, g_uiPalette.panelMuted, bl, 1);
  snprintf(bl, sizeof(bl), "X %s", xyCurveLabel(g_xyCurveA));
  drawRoundedButton(g_xyCfgCvARect, g_uiPalette.panelMuted, bl, 1);
  snprintf(bl, sizeof(bl), "Y %s", xyCurveLabel(g_xyCurveB));
  drawRoundedButton(g_xyCfgCvBRect, g_uiPalette.panelMuted, bl, 1);

  char ret[20];
  snprintf(ret, sizeof(ret), "%s", xyReleaseModeLabel());
  drawRoundedButton(s_xyDrReturn, g_uiPalette.seqLaneTab, ret, 1);
  snprintf(bl, sizeof(bl), "Rec %s", g_xyRecordToSeq ? "on" : "off");
  drawRoundedButton(s_xyDrRec, g_xyRecordToSeq ? g_uiPalette.accentPress : g_uiPalette.panelMuted, bl, 1);
}

void computeXyLayout(int w, int h) {
  layoutBottomBezels(w, h);
  constexpr int margin = 8;
  constexpr int titleH = 10;
  constexpr int coordH = 12;
  const int padTop = titleH + 8;
  const int padBottom = h - kBezelBarH - coordH - 2;
  const int padH = padBottom - padTop;
  const int padW = w - 2 * margin;
  g_xyPadRect = {margin, padTop, padW, max(40, padH)};
}

bool hitTestXyPad(int px, int py) { return pointInRect(px, py, g_xyPadRect); }

void xyMapTouchToVals(int px, int py, uint8_t* outX, uint8_t* outY) {
  const int rw = max(1, g_xyPadRect.w - 1);
  const int rh = max(1, g_xyPadRect.h - 1);
  int relX = px - g_xyPadRect.x;
  int relY = py - g_xyPadRect.y;
  if (relX < 0) relX = 0;
  if (relX > rw) relX = rw;
  if (relY < 0) relY = 0;
  if (relY > rh) relY = rh;
  *outX = static_cast<uint8_t>((int64_t)relX * 127 / rw);
  *outY = static_cast<uint8_t>(127 - (int64_t)relY * 127 / rh);
}

void drawXyCrosshairOnly() {
  const Rect& pr = g_xyPadRect;
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillRoundRect(pr.x + 1, pr.y + 1, pr.w - 2, pr.h - 2, 7, g_uiPalette.xyPadFill);
  M5.Display.drawRoundRect(pr.x, pr.y, pr.w, pr.h, 8, TFT_WHITE);
  const int cx = pr.x + (int)((int64_t)g_xyValX * (pr.w - 1) / 127);
  const int cy = pr.y + (pr.h - 1) - (int)((int64_t)g_xyValY * (pr.h - 1) / 127);
  M5.Display.drawFastVLine(cx, pr.y, pr.h, g_uiPalette.xyAxis);
  M5.Display.drawFastHLine(pr.x, cy, pr.w, g_uiPalette.xyAxis);
  char vals[24];
  snprintf(vals, sizeof(vals), "%u  %u", (unsigned)g_xyValX, (unsigned)g_xyValY);
  const int coordY = pr.y + pr.h + 2;
  M5.Display.fillRect(0, coordY, w, h - kBezelBarH - coordY, g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
  M5.Display.drawString(vals, w / 2, coordY);
  M5.Display.endWrite();
}

static const char* xyCurveLabel(uint8_t curve) {
  switch (curve) {
    case 1:
      return "Log";
    case 2:
      return "Inv";
    default:
      return "Lin";
  }
}

static uint8_t applyXyCurve(uint8_t raw, uint8_t curve) {
  uint8_t v = raw > 127 ? 127 : raw;
  switch (curve) {
    case 1: {
      const uint16_t curved = (static_cast<uint16_t>(v) * static_cast<uint16_t>(v)) / 127U;
      return static_cast<uint8_t>(curved == 0U && v > 0U ? 1U : curved);
    }
    case 2:
      return static_cast<uint8_t>(127U - v);
    default:
      return v;
  }
}

static void xySendAxisCc(uint8_t channel0_15, uint8_t cc, uint8_t value7, uint8_t* lastSent7) {
  if (!lastSent7) return;
  if (*lastSent7 == value7) return;
  if (cc <= 31U) {
    const uint16_t v14 = static_cast<uint16_t>(value7) * 129U;
    midiSendControlChange(channel0_15, cc, static_cast<uint8_t>((v14 >> 7) & 0x7F));
    midiSendControlChange(channel0_15, static_cast<uint8_t>(cc + 32U), static_cast<uint8_t>(v14 & 0x7F));
  } else {
    midiSendControlChange(channel0_15, cc, value7);
  }
  *lastSent7 = value7;
}

static void xyQueueAxisCc(uint8_t channel0_15, uint8_t cc, uint8_t value7, uint8_t* lastSent7,
                          uint32_t* lastSendMs, XyAxisPending* pending, uint32_t nowMs, bool force) {
  if (!lastSent7 || !lastSendMs || !pending) return;
  if (*lastSent7 == value7) {
    pending->dirty = false;
    return;
  }
  if (force || (nowMs - *lastSendMs) >= kXyCcMinIntervalMs) {
    xySendAxisCc(channel0_15, cc, value7, lastSent7);
    *lastSendMs = nowMs;
    pending->dirty = false;
    return;
  }
  pending->dirty = true;
  pending->channel = channel0_15;
  pending->cc = cc;
  pending->value = value7;
}

static void xyFlushPending(uint32_t nowMs) {
  if (s_xyPendingX.dirty && (nowMs - s_xyLastSendMsX) >= kXyCcMinIntervalMs) {
    xySendAxisCc(s_xyPendingX.channel, s_xyPendingX.cc, s_xyPendingX.value, &s_xyMidiSentX);
    s_xyLastSendMsX = nowMs;
    s_xyPendingX.dirty = false;
  }
  if (s_xyPendingY.dirty && (nowMs - s_xyLastSendMsY) >= kXyCcMinIntervalMs) {
    xySendAxisCc(s_xyPendingY.channel, s_xyPendingY.cc, s_xyPendingY.value, &s_xyMidiSentY);
    s_xyLastSendMsY = nowMs;
    s_xyPendingY.dirty = false;
  }
}

static const char* xyReleaseModeLabel() {
  return g_settings.xyReturnToCenter ? "Return center" : "Hold last";
}

void drawXyPadSurface(bool fullClear) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  if (fullClear) M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  computeXyLayout(w, h);

  M5.Display.setFont(nullptr);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString("X-Y MIDI", w / 2, 2);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("Swipe down: setup", w / 2, 14);

  const Rect& pr = g_xyPadRect;
  M5.Display.fillRoundRect(pr.x, pr.y, pr.w, pr.h, 8, g_uiPalette.xyPadFill);
  M5.Display.drawRoundRect(pr.x, pr.y, pr.w, pr.h, 8, TFT_WHITE);

  const int cx = pr.x + (int)((int64_t)g_xyValX * (pr.w - 1) / 127);
  const int cy = pr.y + (pr.h - 1) - (int)((int64_t)g_xyValY * (pr.h - 1) / 127);
  M5.Display.drawFastVLine(cx, pr.y, pr.h, g_uiPalette.xyAxis);
  M5.Display.drawFastHLine(pr.x, cy, pr.w, g_uiPalette.xyAxis);

  char vals[24];
  snprintf(vals, sizeof(vals), "%u  %u", (unsigned)g_xyValX, (unsigned)g_xyValY);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
  M5.Display.drawString(vals, w / 2, pr.y + pr.h + 2);

  drawTopSystemStatus(w, 2, nullptr, 0);
  drawBezelBarStrip();
  M5.Display.endWrite();
  paintAppTopDrawer();
}

void processXyTouch(uint8_t touchCount, int w, int h) {
  computeXyLayout(w, h);

  if (::ui::topDrawerIsOpen(g_topDrawerUi) && g_topDrawerUi.ignoreUntilTouchUp) {
    if (touchCount > 0) {
      wasTouchActive = true;
      return;
    }
    g_topDrawerUi.ignoreUntilTouchUp = false;
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
  }

  if (::ui::topDrawerIsOpen(g_topDrawerUi) && g_topDrawerUi.kind == ::ui::TopDrawerKind::Xy) {
    layoutXyDrawerControlRects(w, h);
    const ::ui::Rect sheet = ::ui::makeTopDrawerRect(w, h, 120);
    if (touchCount == 1) {
      const auto& d = M5.Touch.getDetail(0);
      if (d.isPressed() && !pointInRect(d.x, d.y, Rect{sheet.x, sheet.y, sheet.w, sheet.h})) {
        ::ui::topDrawerClose(g_topDrawerUi);
        drawXyPadSurface(false);
      }
      wasTouchActive = true;
      return;
    }
    if (touchCount == 0) {
      if (!wasTouchActive) return;
      wasTouchActive = false;
      const int hx = g_lastTouchX;
      const int hy = g_lastTouchY;
      const uint8_t midiCh = static_cast<uint8_t>((g_xyOutChannel < 1 || g_xyOutChannel > 16) ? 0U
                                                                                               : (g_xyOutChannel - 1U));
      if (pointInRect(hx, hy, g_xyCfgChRect)) {
        g_xyOutChannel = static_cast<uint8_t>((g_xyOutChannel % 16U) + 1U);
        xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
      } else if (pointInRect(hx, hy, g_xyCfgCcARect)) {
        g_xyCcA = static_cast<uint8_t>((g_xyCcA + 1U) & 0x7FU);
        xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
      } else if (pointInRect(hx, hy, g_xyCfgCcBRect)) {
        g_xyCcB = static_cast<uint8_t>((g_xyCcB + 1U) & 0x7FU);
        xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
      } else if (pointInRect(hx, hy, g_xyCfgCvARect)) {
        g_xyCurveA = static_cast<uint8_t>((g_xyCurveA + 1U) % 3U);
        xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
      } else if (pointInRect(hx, hy, g_xyCfgCvBRect)) {
        g_xyCurveB = static_cast<uint8_t>((g_xyCurveB + 1U) % 3U);
        xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
      } else if (pointInRect(hx, hy, s_xyDrReturn)) {
        g_settings.xyReturnToCenter = g_settings.xyReturnToCenter ? 0U : 1U;
        settingsSave(g_settings);
      } else if (pointInRect(hx, hy, s_xyDrRec)) {
        g_xyRecordToSeq = !g_xyRecordToSeq;
        transportPrefsSave();
      } else if (!pointInRect(hx, hy, Rect{sheet.x, sheet.y, sheet.w, sheet.h})) {
        ::ui::topDrawerClose(g_topDrawerUi);
        drawXyPadSurface(false);
        return;
      }
      drawXyPadSurface(false);
      return;
    }
    wasTouchActive = true;
    return;
  }

  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount >= 2) {
    playResetBezelFastCycle();
    wasTouchActive = true;
    layoutBottomBezels(w, h);
    int sel = -1;
    int pad = -1;
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (!d.isPressed()) continue;
      if (pointInRect(d.x, d.y, g_bezelSelect)) {
        sel = static_cast<int>(i);
      } else if (hitTestXyPad(d.x, d.y)) {
        pad = static_cast<int>(i);
      }
    }
    if (sel >= 0 && pad >= 0) {
      const auto& dm = M5.Touch.getDetail(static_cast<uint8_t>(pad));
      if (!s_xyCombo.armed) {
        s_xyCombo.sx = dm.x;
        s_xyCombo.sy = dm.y;
        s_xyCombo.armed = true;
      }
      s_xyCombo.lx = dm.x;
      s_xyCombo.ly = dm.y;
    }
    wasTouchActive = true;
    if (!s_xyTwoFingerSurfaceDrawn) {
      drawXyPadSurface(false);
      s_xyTwoFingerSurfaceDrawn = true;
    }
    return;
  }

  const uint8_t midiCh = static_cast<uint8_t>((g_xyOutChannel < 1 || g_xyOutChannel > 16) ? 0U
                                                                                           : (g_xyOutChannel - 1U));

  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    const bool bezel = pointInRect(d.x, d.y, g_bezelBack) || pointInRect(d.x, d.y, g_bezelSelect) ||
                       pointInRect(d.x, d.y, g_bezelFwd);
    if (bezel) {
      if (s_xyTouchZone == 1) {
        drawXyPadSurface(false);
      }
      s_xyTouchZone = 0;
    } else if (hitTestXyPad(d.x, d.y)) {
      const uint8_t ox = g_xyValX;
      const uint8_t oy = g_xyValY;
      uint8_t vx, vy;
      xyMapTouchToVals(d.x, d.y, &vx, &vy);
      g_xyValX = vx;
      g_xyValY = vy;
      const bool suppressMidi =
          g_xyRecordToSeq && transportIsPlaying() && transportIsRecordingLive();
      if (!suppressMidi) {
        const uint8_t sx = applyXyCurve(vx, g_xyCurveA);
        const uint8_t sy = applyXyCurve(vy, g_xyCurveB);
        const uint32_t nowMs = millis();
        xyQueueAxisCc(midiCh, g_xyCcA, sx, &s_xyMidiSentX, &s_xyLastSendMsX, &s_xyPendingX, nowMs, false);
        xyQueueAxisCc(midiCh, g_xyCcB, sy, &s_xyMidiSentY, &s_xyLastSendMsY, &s_xyPendingY, nowMs, false);
      }
      if (s_xyTouchZone != 1) {
        drawXyPadSurface(false);
      } else if (vx != ox || vy != oy) {
        drawXyCrosshairOnly();
      }
      s_xyTouchZone = 1;
    } else {
      if (s_xyTouchZone == 1) {
        drawXyPadSurface(false);
      }
      s_xyTouchZone = 0;
    }
    wasTouchActive = true;
    return;
  }

  if (touchCount == 0) {
    if (!wasTouchActive) return;
    const int8_t zoneBeforeRelease = s_xyTouchZone;
    const bool suppressMidi = g_xyRecordToSeq && transportIsPlaying() && transportIsRecordingLive();
    s_xyTouchZone = -1;
    s_xyTwoFingerSurfaceDrawn = false;
    wasTouchActive = false;
    if (suppressNextPlayTap) {
      suppressNextPlayTap = false;
      drawXyPadSurface(false);
      return;
    }

    if (s_xyCombo.armed) {
      const int dx = s_xyCombo.lx - s_xyCombo.sx;
      const int dy = s_xyCombo.ly - s_xyCombo.sy;
      s_xyCombo.armed = false;
      if (abs(dx) < 22 && abs(dy) < 22) {
        g_xyRecordToSeq = !g_xyRecordToSeq;
        transportPrefsSave();
        drawXyPadSurface(false);
        return;
      }
    }

    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;
    if (pointInRect(hx, hy, g_bezelBack)) {
      navigateMainRing(-1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      navigateMainRing(1);
      return;
    }
    if (zoneBeforeRelease == 1 && g_settings.xyReturnToCenter) {
      g_xyValX = 64;
      g_xyValY = 64;
      if (!suppressMidi) {
        const uint8_t sx = applyXyCurve(g_xyValX, g_xyCurveA);
        const uint8_t sy = applyXyCurve(g_xyValY, g_xyCurveB);
        const uint32_t nowMs = millis();
        xyQueueAxisCc(midiCh, g_xyCcA, sx, &s_xyMidiSentX, &s_xyLastSendMsX, &s_xyPendingX, nowMs, true);
        xyQueueAxisCc(midiCh, g_xyCcB, sy, &s_xyMidiSentY, &s_xyLastSendMsY, &s_xyPendingY, nowMs, true);
      }
    }
    drawXyPadSurface(false);
  }
}
