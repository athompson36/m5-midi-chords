static play_screen::PlayDrawerRects s_playDr{};

static void layoutPlayDrawerRects(int screenW, int screenH) {
  s_playDr = play_screen::computePlayDrawerLayout(screenW, screenH, g_topDrawerUi.subpage & 1U);
}

void drawPlayTopDrawer(::ui::DisplayAdapter& d) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutPlayDrawerRects(w, h);
  const int drawerH = play_screen::playDrawerHeightPx();
  ::ui::DrawerSheetSpec sheet{};
  sheet.bounds = ::ui::makeTopDrawerRect(w, h, drawerH);
  sheet.title = "Play setup";
  ::ui::drawDrawerSheet(d, sheet);

  const uint8_t sub = g_topDrawerUi.subpage & 1U;

  ::ui::TabChipSpec tabOut{};
  tabOut.bounds = s_playDr.tabOutput;
  tabOut.label = "Output";
  tabOut.selected = (sub == 0);
  tabOut.textSize = 1;
  ::ui::drawTabChip(d, tabOut);
  ::ui::TabChipSpec tabKey{};
  tabKey.bounds = s_playDr.tabKey;
  tabKey.label = "Key";
  tabKey.selected = (sub == 1);
  tabKey.textSize = 1;
  ::ui::drawTabChip(d, tabKey);

  if (sub == 0) {
    drawRoundedButton(
        Rect{s_playDr.transMinus.x, s_playDr.transMinus.y, s_playDr.transMinus.w, s_playDr.transMinus.h},
        g_uiPalette.panelMuted, "Trans -", 1);
    drawRoundedButton(
        Rect{s_playDr.transPlus.x, s_playDr.transPlus.y, s_playDr.transPlus.w, s_playDr.transPlus.h},
        g_uiPalette.panelMuted, "Trans +", 1);

    ::ui::StripShellSpec shell{};
    shell.bounds = s_playDr.velSlider;
    shell.radius = 8;
    ::ui::drawStripShell(d, shell);
    char velTxt[8];
    snprintf(velTxt, sizeof(velTxt), "%u", (unsigned)g_settings.outputVelocity);
    ::ui::SliderTileSpec vel{};
    vel.bounds = ::ui::insetRect(s_playDr.velSlider, 3);
    vel.label = "Vel";
    vel.value = g_settings.outputVelocity > 127 ? 100 : (g_settings.outputVelocity * 100 / 127);
    vel.valueText = velTxt;
    vel.role = ::ui::GlossRole::Primary;
    ::ui::drawSliderTile(d, vel);

    drawRoundedButton(
        Rect{s_playDr.velMinus.x, s_playDr.velMinus.y, s_playDr.velMinus.w, s_playDr.velMinus.h},
        g_uiPalette.panelMuted, "-", 1);
    drawRoundedButton(
        Rect{s_playDr.velPlus.x, s_playDr.velPlus.y, s_playDr.velPlus.w, s_playDr.velPlus.h},
        g_uiPalette.panelMuted, "+", 1);

    char ch[20];
    snprintf(ch, sizeof(ch), "Out CH %u", (unsigned)g_settings.midiOutChannel);
    drawRoundedButton(
        Rect{s_playDr.midiCh.x, s_playDr.midiCh.y, s_playDr.midiCh.w, s_playDr.midiCh.h},
        g_uiPalette.seqLaneTab, ch, 1);
    char vo[20];
    snprintf(vo, sizeof(vo), "Voice %u", (unsigned)g_chordVoicing);
    drawRoundedButton(
        Rect{s_playDr.voicing.x, s_playDr.voicing.y, s_playDr.voicing.w, s_playDr.voicing.h},
        g_uiPalette.panelMuted, vo, 1);
  } else {
    drawRoundedButton(
        Rect{s_playDr.keyMinus.x, s_playDr.keyMinus.y, s_playDr.keyMinus.w, s_playDr.keyMinus.h},
        g_uiPalette.panelMuted, "Key -", 1);
    drawRoundedButton(
        Rect{s_playDr.keyPlus.x, s_playDr.keyPlus.y, s_playDr.keyPlus.w, s_playDr.keyPlus.h},
        g_uiPalette.panelMuted, "Key +", 1);

    char modeTxt[28];
    snprintf(modeTxt, sizeof(modeTxt), "Mode: %s", ChordModel::modeShortLabel(g_model.mode));
    drawRoundedButton(
        Rect{s_playDr.modeBtn.x, s_playDr.modeBtn.y, s_playDr.modeBtn.w, s_playDr.modeBtn.h},
        g_uiPalette.seqLaneTab, modeTxt, 1);

    char arpTxt[22];
    snprintf(arpTxt, sizeof(arpTxt), "Arp: %s", arpeggiatorModeLabel(g_settings.arpeggiatorMode));
    drawRoundedButton(
        Rect{s_playDr.arpBtn.x, s_playDr.arpBtn.y, s_playDr.arpBtn.w, s_playDr.arpBtn.h},
        g_uiPalette.panelMuted, arpTxt, 1);

    const char* curLab = (g_settings.velocityCurve == 0)   ? "Linear"
                         : (g_settings.velocityCurve == 1) ? "Soft"
                                                           : "Hard";
    char curTxt[20];
    snprintf(curTxt, sizeof(curTxt), "Curve: %s", curLab);
    drawRoundedButton(
        Rect{s_playDr.curveBtn.x, s_playDr.curveBtn.y, s_playDr.curveBtn.w, s_playDr.curveBtn.h},
        g_uiPalette.panelMuted, curTxt, 1);
  }
}

bool playProcessTopDrawerTouch(uint8_t touchCount, int w, int h) {
  if (!::ui::topDrawerIsOpen(g_topDrawerUi)) return false;
  if (g_topDrawerUi.kind != ::ui::TopDrawerKind::Play) return false;
  layoutPlayDrawerRects(w, h);
  const int drawerH = play_screen::playDrawerHeightPx();
  const ::ui::Rect sheet = ::ui::makeTopDrawerRect(w, h, drawerH);
  const uint8_t sub = g_topDrawerUi.subpage & 1U;

  if (touchCount == 1) {
    const auto& td = M5.Touch.getDetail(0);
    if (td.isPressed() && !pointInRect(td.x, td.y, Rect{sheet.x, sheet.y, sheet.w, sheet.h})) {
      ::ui::topDrawerClose(g_topDrawerUi);
      drawPlaySurface();
    }
    wasTouchActive = true;
    return true;
  }
  if (touchCount == 0) {
    if (!wasTouchActive) return true;
    wasTouchActive = false;
    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;

    if (pointInRect(hx, hy, Rect{s_playDr.tabOutput.x, s_playDr.tabOutput.y, s_playDr.tabOutput.w,
                                 s_playDr.tabOutput.h})) {
      g_topDrawerUi.subpage = 0;
      drawPlaySurface();
      return true;
    }
    if (pointInRect(hx, hy, Rect{s_playDr.tabKey.x, s_playDr.tabKey.y, s_playDr.tabKey.w, s_playDr.tabKey.h})) {
      g_topDrawerUi.subpage = 1;
      drawPlaySurface();
      return true;
    }

    if (sub == 0) {
      if (pointInRect(hx, hy, Rect{s_playDr.transMinus.x, s_playDr.transMinus.y, s_playDr.transMinus.w,
                                   s_playDr.transMinus.h})) {
        g_settings.transposeSemitones =
            static_cast<int8_t>(max(-24, min(24, static_cast<int>(g_settings.transposeSemitones) - 1)));
        settingsSave(g_settings);
      } else if (pointInRect(hx, hy, Rect{s_playDr.transPlus.x, s_playDr.transPlus.y, s_playDr.transPlus.w,
                                         s_playDr.transPlus.h})) {
        g_settings.transposeSemitones =
            static_cast<int8_t>(max(-24, min(24, static_cast<int>(g_settings.transposeSemitones) + 1)));
        settingsSave(g_settings);
      } else if (pointInRect(hx, hy, Rect{s_playDr.velMinus.x, s_playDr.velMinus.y, s_playDr.velMinus.w,
                                           s_playDr.velMinus.h})) {
        if (g_settings.outputVelocity > 1) g_settings.outputVelocity--;
        settingsSave(g_settings);
      } else if (pointInRect(hx, hy, Rect{s_playDr.velPlus.x, s_playDr.velPlus.y, s_playDr.velPlus.w,
                                           s_playDr.velPlus.h})) {
        if (g_settings.outputVelocity < 127) g_settings.outputVelocity++;
        settingsSave(g_settings);
      } else if (pointInRect(hx, hy, Rect{s_playDr.midiCh.x, s_playDr.midiCh.y, s_playDr.midiCh.w,
                                           s_playDr.midiCh.h})) {
        g_settings.midiOutChannel = static_cast<uint8_t>((g_settings.midiOutChannel % 16U) + 1U);
        settingsSave(g_settings);
      } else if (pointInRect(hx, hy, Rect{s_playDr.voicing.x, s_playDr.voicing.y, s_playDr.voicing.w,
                                           s_playDr.voicing.h})) {
        g_chordVoicing = static_cast<uint8_t>((g_chordVoicing % 4U) + 1U);
        chordVoicingSave(g_chordVoicing);
      }
    } else {
      if (pointInRect(hx, hy, Rect{s_playDr.keyMinus.x, s_playDr.keyMinus.y, s_playDr.keyMinus.w,
                                   s_playDr.keyMinus.h})) {
        panicForTrigger(PanicTrigger::KeyPickerTransition);
        g_model.setTonicAndMode(g_model.keyIndex - 1, g_model.mode);
        chordStateSave(g_model);
      } else if (pointInRect(hx, hy, Rect{s_playDr.keyPlus.x, s_playDr.keyPlus.y, s_playDr.keyPlus.w,
                                         s_playDr.keyPlus.h})) {
        panicForTrigger(PanicTrigger::KeyPickerTransition);
        g_model.setTonicAndMode(g_model.keyIndex + 1, g_model.mode);
        chordStateSave(g_model);
      } else if (pointInRect(hx, hy, Rect{s_playDr.modeBtn.x, s_playDr.modeBtn.y, s_playDr.modeBtn.w,
                                         s_playDr.modeBtn.h})) {
        panicForTrigger(PanicTrigger::KeyPickerTransition);
        const auto nextMode = static_cast<KeyMode>(
            (static_cast<int>(g_model.mode) + 1) % static_cast<int>(KeyMode::kCount));
        g_model.setTonicAndMode(g_model.keyIndex, nextMode);
        chordStateSave(g_model);
      } else if (pointInRect(hx, hy, Rect{s_playDr.arpBtn.x, s_playDr.arpBtn.y, s_playDr.arpBtn.w,
                                         s_playDr.arpBtn.h})) {
        g_settings.cycleArpeggiatorMode();
        settingsSave(g_settings);
      } else if (pointInRect(hx, hy, Rect{s_playDr.curveBtn.x, s_playDr.curveBtn.y, s_playDr.curveBtn.w,
                                         s_playDr.curveBtn.h})) {
        g_settings.velocityCurve = static_cast<uint8_t>((g_settings.velocityCurve + 1U) % 3U);
        settingsSave(g_settings);
      }
    }

    if (!pointInRect(hx, hy, Rect{sheet.x, sheet.y, sheet.w, sheet.h})) {
      ::ui::topDrawerClose(g_topDrawerUi);
      drawPlaySurface();
      return true;
    }
    drawPlaySurface();
    return true;
  }
  wasTouchActive = true;
  return true;
}
