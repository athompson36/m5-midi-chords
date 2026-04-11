static Rect s_seqDrTabFeel{};
static Rect s_seqDrTabUtil{};
static Rect s_seqDrSwingM{};
static Rect s_seqDrSwingP{};
static Rect s_seqDrQuantM{};
static Rect s_seqDrQuantP{};
static Rect s_seqDrProbM{};
static Rect s_seqDrProbP{};
static Rect s_seqDrLane{};
static Rect s_seqDrHumM{};
static Rect s_seqDrHumP{};
static Rect s_seqDrChM{};
static Rect s_seqDrChP{};
static Rect s_seqDrRandM{};
static Rect s_seqDrRandP{};

static constexpr int kSeqDrawerH = 210;

static void layoutSeqDrawerRects(int screenW, int screenH) {
  const ::ui::Rect drawer = ::ui::makeTopDrawerRect(screenW, screenH, kSeqDrawerH);
  const ::ui::Rect content = ::ui::makeDrawerContentRect(drawer);
  const int gap = ::ui::LayoutMetrics::ControlGap;
  ::ui::Rect tabStrip = ::ui::makeStripRect(content, 4, 28);
  ::ui::Rect tabPair[2];
  ::ui::layout2Up(tabStrip, tabPair, gap);
  s_seqDrTabFeel = {tabPair[0].x, tabPair[0].y, tabPair[0].w, tabPair[0].h};
  s_seqDrTabUtil = {tabPair[1].x, tabPair[1].y, tabPair[1].w, tabPair[1].h};

  const uint8_t sub = g_topDrawerUi.subpage & 1U;
  if (sub == 0) {
    ::ui::Rect row1 = ::ui::makeStripRect(content, 36, 40);
    ::ui::Rect row2 = ::ui::makeStripRect(content, 80, 40);
    ::ui::Rect row3 = ::ui::makeStripRect(content, 124, 40);
    ::ui::Rect a2[2];
    ::ui::layout2Up(row1, a2, gap);
    s_seqDrSwingM = {a2[0].x, a2[0].y, a2[0].w, a2[0].h};
    s_seqDrSwingP = {a2[1].x, a2[1].y, a2[1].w, a2[1].h};
    ::ui::layout2Up(row2, a2, gap);
    s_seqDrQuantM = {a2[0].x, a2[0].y, a2[0].w, a2[0].h};
    s_seqDrQuantP = {a2[1].x, a2[1].y, a2[1].w, a2[1].h};
    ::ui::layout2Up(row3, a2, gap);
    s_seqDrProbM = {a2[0].x, a2[0].y, a2[0].w, a2[0].h};
    s_seqDrProbP = {a2[1].x, a2[1].y, a2[1].w, a2[1].h};
    s_seqDrLane = ::ui::makeStripRect(content, 168, 28);
  } else {
    ::ui::Rect rowHum = ::ui::makeStripRect(content, 36, 40);
    ::ui::Rect rowCh = ::ui::makeStripRect(content, 80, 40);
    ::ui::Rect rowR = ::ui::makeStripRect(content, 124, 40);
    ::ui::Rect a2[2];
    ::ui::layout2Up(rowHum, a2, gap);
    s_seqDrHumM = {a2[0].x, a2[0].y, a2[0].w, a2[0].h};
    s_seqDrHumP = {a2[1].x, a2[1].y, a2[1].w, a2[1].h};
    ::ui::layout2Up(rowCh, a2, gap);
    s_seqDrChM = {a2[0].x, a2[0].y, a2[0].w, a2[0].h};
    s_seqDrChP = {a2[1].x, a2[1].y, a2[1].w, a2[1].h};
    ::ui::layout2Up(rowR, a2, gap);
    s_seqDrRandM = {a2[0].x, a2[0].y, a2[0].w, a2[0].h};
    s_seqDrRandP = {a2[1].x, a2[1].y, a2[1].w, a2[1].h};
  }
}

void drawSequencerTopDrawer(::ui::DisplayAdapter& d) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutSeqDrawerRects(w, h);
  ::ui::DrawerSheetSpec sheet{};
  sheet.bounds = ::ui::makeTopDrawerRect(w, h, kSeqDrawerH);
  sheet.title = "Seq setup";
  ::ui::drawDrawerSheet(d, sheet);

  const uint8_t sub = g_topDrawerUi.subpage & 1U;
  const uint8_t lane = g_seqLane < 3 ? g_seqLane : 0;

  ::ui::TabChipSpec tabFeel{};
  tabFeel.bounds = {s_seqDrTabFeel.x, s_seqDrTabFeel.y, s_seqDrTabFeel.w, s_seqDrTabFeel.h};
  tabFeel.label = "Feel";
  tabFeel.selected = (sub == 0);
  tabFeel.textSize = 1;
  ::ui::drawTabChip(d, tabFeel);
  ::ui::TabChipSpec tabUtil{};
  tabUtil.bounds = {s_seqDrTabUtil.x, s_seqDrTabUtil.y, s_seqDrTabUtil.w, s_seqDrTabUtil.h};
  tabUtil.label = "Util";
  tabUtil.selected = (sub == 1);
  tabUtil.textSize = 1;
  ::ui::drawTabChip(d, tabUtil);

  if (sub == 0) {
    drawRoundedButton(s_seqDrSwingM, g_uiPalette.panelMuted, "Swing -", 1);
    drawRoundedButton(s_seqDrSwingP, g_uiPalette.panelMuted, "Swing +", 1);
    drawRoundedButton(s_seqDrQuantM, g_uiPalette.panelMuted, "Quant -", 1);
    drawRoundedButton(s_seqDrQuantP, g_uiPalette.panelMuted, "Quant +", 1);
    drawRoundedButton(s_seqDrProbM, g_uiPalette.panelMuted, "Prob -", 1);
    drawRoundedButton(s_seqDrProbP, g_uiPalette.panelMuted, "Prob +", 1);
    char tab[24];
    snprintf(tab, sizeof(tab), "Lane L%u", static_cast<unsigned>(lane + 1U));
    drawRoundedButton(
        Rect{s_seqDrLane.x, s_seqDrLane.y, s_seqDrLane.w, s_seqDrLane.h},
        g_uiPalette.seqLaneTab, tab, 1);
  } else {
    drawRoundedButton(s_seqDrHumM, g_uiPalette.panelMuted, "Hum -", 1);
    drawRoundedButton(s_seqDrHumP, g_uiPalette.panelMuted, "Hum +", 1);
    drawRoundedButton(s_seqDrChM, g_uiPalette.panelMuted, "CH -", 1);
    drawRoundedButton(s_seqDrChP, g_uiPalette.panelMuted, "CH +", 1);
    drawRoundedButton(s_seqDrRandM, g_uiPalette.panelMuted, "Rand -", 1);
    drawRoundedButton(s_seqDrRandP, g_uiPalette.panelMuted, "Rand +", 1);
    M5.Display.setFont(nullptr);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
    M5.Display.drawString("BPM / clock: Transport", s_seqDrRandM.x, s_seqDrRandM.y + s_seqDrRandM.h + 4);
  }
}

bool sequencerProcessTopDrawerTouch(uint8_t touchCount, int w, int h) {
  if (!::ui::topDrawerIsOpen(g_topDrawerUi)) return false;
  if (g_topDrawerUi.kind != ::ui::TopDrawerKind::Sequencer) {
    return false;
  }
  layoutSeqDrawerRects(w, h);
  const ::ui::Rect sheet = ::ui::makeTopDrawerRect(w, h, kSeqDrawerH);
  const uint8_t lane = g_seqLane < 3 ? g_seqLane : 0;
  const uint8_t sub = g_topDrawerUi.subpage & 1U;

  if (touchCount == 1) {
    const auto& td = M5.Touch.getDetail(0);
    if (td.isPressed() && !pointInRect(td.x, td.y, Rect{sheet.x, sheet.y, sheet.w, sheet.h})) {
      ::ui::topDrawerClose(g_topDrawerUi);
      drawSequencerSurface();
    }
    wasTouchActive = true;
    return true;
  }
  if (touchCount == 0) {
    if (!wasTouchActive) return true;
    wasTouchActive = false;
    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;
    auto clampU8 = [](int v) -> uint8_t {
      if (v < 0) return 0;
      if (v > 100) return 100;
      return static_cast<uint8_t>(v);
    };

    if (pointInRect(hx, hy, s_seqDrTabFeel)) {
      g_topDrawerUi.subpage = 0;
      drawSequencerSurface();
      return true;
    }
    if (pointInRect(hx, hy, s_seqDrTabUtil)) {
      g_topDrawerUi.subpage = 1;
      drawSequencerSurface();
      return true;
    }

    if (sub == 0) {
      if (pointInRect(hx, hy, s_seqDrSwingM)) {
        int v = static_cast<int>(g_seqExtras.swingPct[lane]) - 5;
        g_seqExtras.swingPct[lane] = clampU8(v);
        seqExtrasSave(&g_seqExtras);
      } else if (pointInRect(hx, hy, s_seqDrSwingP)) {
        int v = static_cast<int>(g_seqExtras.swingPct[lane]) + 5;
        g_seqExtras.swingPct[lane] = clampU8(v);
        seqExtrasSave(&g_seqExtras);
      } else if (pointInRect(hx, hy, s_seqDrQuantM)) {
        int v = static_cast<int>(g_seqExtras.quantizePct[lane]) - 5;
        g_seqExtras.quantizePct[lane] = clampU8(v);
        seqExtrasSave(&g_seqExtras);
      } else if (pointInRect(hx, hy, s_seqDrQuantP)) {
        int v = static_cast<int>(g_seqExtras.quantizePct[lane]) + 5;
        g_seqExtras.quantizePct[lane] = clampU8(v);
        seqExtrasSave(&g_seqExtras);
      } else if (pointInRect(hx, hy, s_seqDrProbM)) {
        for (int s = 0; s < 16; ++s) {
          int v = static_cast<int>(g_seqExtras.stepProb[lane][s]) - 5;
          g_seqExtras.stepProb[lane][s] = clampU8(v);
        }
        seqExtrasSave(&g_seqExtras);
      } else if (pointInRect(hx, hy, s_seqDrProbP)) {
        for (int s = 0; s < 16; ++s) {
          int v = static_cast<int>(g_seqExtras.stepProb[lane][s]) + 5;
          g_seqExtras.stepProb[lane][s] = clampU8(v);
        }
        seqExtrasSave(&g_seqExtras);
      } else if (pointInRect(hx, hy, Rect{s_seqDrLane.x, s_seqDrLane.y, s_seqDrLane.w, s_seqDrLane.h})) {
        g_seqLane = static_cast<uint8_t>((g_seqLane + 1U) % 3U);
      }
    } else {
      if (pointInRect(hx, hy, s_seqDrHumM)) {
        int v = static_cast<int>(g_settings.globalHumanizePct) - 5;
        if (v < 0) v = 0;
        g_settings.globalHumanizePct = static_cast<uint8_t>(v);
        transportSetGlobalHumanize(g_settings.globalHumanizePct);
        settingsSave(g_settings);
      } else if (pointInRect(hx, hy, s_seqDrHumP)) {
        int v = static_cast<int>(g_settings.globalHumanizePct) + 5;
        if (v > 100) v = 100;
        g_settings.globalHumanizePct = static_cast<uint8_t>(v);
        transportSetGlobalHumanize(g_settings.globalHumanizePct);
        settingsSave(g_settings);
      } else if (pointInRect(hx, hy, s_seqDrChM)) {
        uint8_t ch = g_seqMidiCh[lane];
        ch = static_cast<uint8_t>((ch + 15U) % 16U + 1U);
        g_seqMidiCh[lane] = ch;
        seqLaneChannelsSave(g_seqMidiCh);
      } else if (pointInRect(hx, hy, s_seqDrChP)) {
        uint8_t ch = g_seqMidiCh[lane];
        ch = static_cast<uint8_t>(ch % 16U + 1U);
        g_seqMidiCh[lane] = ch;
        seqLaneChannelsSave(g_seqMidiCh);
      } else if (pointInRect(hx, hy, s_seqDrRandM)) {
        int v = static_cast<int>(g_seqExtras.chordRandPct[lane]) - 5;
        g_seqExtras.chordRandPct[lane] = clampU8(v);
        seqExtrasSave(&g_seqExtras);
      } else if (pointInRect(hx, hy, s_seqDrRandP)) {
        int v = static_cast<int>(g_seqExtras.chordRandPct[lane]) + 5;
        g_seqExtras.chordRandPct[lane] = clampU8(v);
        seqExtrasSave(&g_seqExtras);
      }
    }

    if (!pointInRect(hx, hy, Rect{sheet.x, sheet.y, sheet.w, sheet.h})) {
      ::ui::topDrawerClose(g_topDrawerUi);
      drawSequencerSurface();
      return true;
    }
    drawSequencerSurface();
    return true;
  }
  wasTouchActive = true;
  return true;
}
