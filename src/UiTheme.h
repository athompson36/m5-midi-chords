#pragma once

#include <stdint.h>

/// All RGB565 colors used by the Play / Seq / XY / Settings / picker UIs.
struct UiPalette {
  uint16_t bg;
  uint16_t principal;
  uint16_t standard;
  uint16_t tension;
  uint16_t surprise;
  uint16_t keyCenter;
  uint16_t heart;
  uint16_t splash;
  uint16_t accentPress;
  uint16_t highlightRing;
  uint16_t seqLaneTab;
  uint16_t seqRest;
  uint16_t seqTie;
  uint16_t seqCellFinger;
  uint16_t xyPadFill;
  uint16_t xyAxis;
  uint16_t bezelMuted;
  uint16_t hintText;
  uint16_t settingsHeader;
  uint16_t rowSelect;
  uint16_t rowNormal;
  uint16_t feedback;
  uint16_t keyPickSelected;
  uint16_t keyPickDone;
  uint16_t keyPickCancel;
  uint16_t settingsBtnActive;
  uint16_t settingsBtnIdle;
  uint16_t settingsBtnBorder;
  uint16_t danger;
  uint16_t subtle;
  uint16_t panelMuted;
  /// Soft outer halo for sequencer playhead (transport playing).
  uint16_t seqPlayGlowOuter;
  /// Bright inner ring for sequencer playhead glow.
  uint16_t seqPlayGlowInner;
};

extern UiPalette g_uiPalette;

/// Number of built-in themes (Default … last).
constexpr uint8_t kUiThemeCount = 7;

/// Apply palette for theme index 0 … kUiThemeCount-1.
void uiThemeApply(uint8_t themeIndex);

const char* uiThemeName(uint8_t themeIndex);
