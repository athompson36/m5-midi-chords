#include "PlayDrawerLayout.h"

static play_screen::PlayDrawerRects s_playDr{};

static void layoutPlayDrawerRects(int screenW, int screenH) {
  s_playDr = play_screen::computePlayDrawerLayout(screenW, screenH);
}

void drawPlayTopDrawer(::ui::DisplayAdapter& d) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutPlayDrawerRects(w, h);
  ::ui::DrawerSheetSpec sheet{};
  sheet.bounds = ::ui::makeTopDrawerRect(w, h, 112);
  sheet.title = "Play setup";
  ::ui::drawDrawerSheet(d, sheet);

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
}

bool playProcessTopDrawerTouch(uint8_t touchCount, int w, int h) {
  if (!::ui::topDrawerIsOpen(g_topDrawerUi)) return false;
  if (g_topDrawerUi.kind != ::ui::TopDrawerKind::Play) return false;
  layoutPlayDrawerRects(w, h);
  const ::ui::Rect sheet = ::ui::makeTopDrawerRect(w, h, 112);
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
    } else if (!pointInRect(hx, hy, Rect{sheet.x, sheet.y, sheet.w, sheet.h})) {
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
