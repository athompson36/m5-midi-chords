#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

using Rect = ::ui::Rect;

extern int s_keyPickTouchStartX;
extern int s_keyPickTouchStartY;
extern int s_sdPickTouchStartX;
extern int s_sdPickTouchStartY;

// =====================================================================
//  KEY + MODE PICKER (full-screen “dropdown”)
// =====================================================================

void drawKeyPicker() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillScreen(g_uiPalette.bg);

  constexpr int pad = 5;
  constexpr int titleY = 4;
  constexpr int keyCols = 4;
  constexpr int keyRows = 3;
  constexpr int keyRowGap = 5;
  constexpr int gapKeysToMode = 6;
  constexpr int gapModeToBtn = 6;
  constexpr int bottomMargin = 4;

  M5.Display.setFont(nullptr);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Key & mode", w / 2, titleY);
  const int keyTop = titleY + 22;

  const int btnH = max(42, min(54, h / 5));
  const int btnY = h - bottomMargin - btnH;
  const int btnW = (w - pad * 3) / 2;

  const int innerBottom = btnY - gapModeToBtn;
  const int inner = innerBottom - keyTop;
  const int budget = inner - gapKeysToMode - keyRowGap * (keyRows - 1);
  const int nMode = static_cast<int>(KeyMode::kCount);
  int modeH = max(30, min(48, (budget * 24) / 100));
  int kh = (budget - modeH) / keyRows;
  while (kh < 26 && modeH > 28) {
    --modeH;
    kh = (budget - modeH) / keyRows;
  }
  kh = max(26, kh);
  if (modeH + keyRows * kh > budget) {
    modeH = max(28, budget - keyRows * kh);
  }

  const int kw = (w - pad * (keyCols + 1)) / keyCols;
  for (int i = 0; i < ChordModel::kKeyCount; ++i) {
    const int col = i % keyCols;
    const int row = i / keyCols;
    const int x = pad + col * (kw + pad);
    const int y = keyTop + row * (kh + keyRowGap);
    g_keyPickCells[i] = {x, y, kw, kh};
    const bool sel = (i == g_pickTonic);
    uint16_t bg = sel ? g_uiPalette.keyPickSelected : g_uiPalette.panelMuted;
    drawRoundedButton(g_keyPickCells[i], bg, ChordModel::kKeyNames[i], 2);
  }

  const int modeTop = keyTop + keyRows * kh + (keyRows - 1) * keyRowGap + gapKeysToMode;
  const int modeGap = 4;
  const int modeW = (w - 2 * pad - (nMode - 1) * modeGap) / nMode;
  const uint8_t modeTextSize = (modeH >= 34) ? 2 : 1;
  for (int m = 0; m < nMode; ++m) {
    const int x = pad + m * (modeW + modeGap);
    g_modePickCells[m] = {x, modeTop, modeW, modeH};
    const bool sel = (static_cast<int>(g_pickMode) == m);
    uint16_t bg = sel ? g_uiPalette.keyPickSelected : g_uiPalette.panelMuted;
    drawRoundedButton(g_modePickCells[m], bg, ChordModel::modeShortLabel(static_cast<KeyMode>(m)),
                      modeTextSize);
  }

  g_keyPickCancel = {pad, btnY, btnW, btnH};
  g_keyPickDone = {w - pad - btnW, btnY, btnW, btnH};
  drawRoundedButton(g_keyPickCancel, g_uiPalette.keyPickCancel, "Cancel", 2);
  drawRoundedButton(g_keyPickDone, g_uiPalette.keyPickDone, "Done", 2);

  drawTopSystemStatus(w, 2, nullptr, 0);
  M5.Display.endWrite();
}

void processKeyPickerTouch(uint8_t touchCount, int w, int h) {
  (void)w;
  (void)h;
  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
    if (!wasTouchActive) {
      s_keyPickTouchStartX = g_lastTouchX;
      s_keyPickTouchStartY = g_lastTouchY;
    }
    wasTouchActive = true;
    return;
  }

  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int px = g_lastTouchX;
  const int py = g_lastTouchY;
  const bool movedPast = touchMovedPastSuppressThreshold(s_keyPickTouchStartX, s_keyPickTouchStartY, px, py);

  if (pointInRect(px, py, g_keyPickDone)) {
    panicForTrigger(PanicTrigger::KeyPickerTransition);
    g_model.setTonicAndMode(g_pickTonic, g_pickMode);
    chordStateSave(g_model);
    seqPatternSave(g_seqPattern);
    g_lastAction = "Key saved";
    g_screen = Screen::Play;
    drawPlaySurface();
    return;
  }
  if (pointInRect(px, py, g_keyPickCancel)) {
    panicForTrigger(PanicTrigger::KeyPickerTransition);
    g_screen = Screen::Play;
    drawPlaySurface();
    return;
  }
  if (!movedPast) {
    for (int i = 0; i < ChordModel::kKeyCount; ++i) {
      if (pointInRect(px, py, g_keyPickCells[i])) {
        g_pickTonic = i;
        drawKeyPicker();
        return;
      }
    }
    for (int m = 0; m < static_cast<int>(KeyMode::kCount); ++m) {
      if (pointInRect(px, py, g_modePickCells[m])) {
        g_pickMode = static_cast<KeyMode>(m);
        drawKeyPicker();
        return;
      }
    }
  }
  drawKeyPicker();
}

// =====================================================================
//  Project name editor + SD restore (shared with Settings)
// =====================================================================

static void applyProjectNameFromEditor() {
  int start = 0;
  while (start < kProjEditLen && g_projEditBuf[start] == ' ') {
    ++start;
  }
  int end = kProjEditLen - 1;
  while (end > start && g_projEditBuf[end] == ' ') {
    --end;
  }
  char tmp[48];
  int j = 0;
  for (int i = start; i <= end && j < 47; ++i) {
    tmp[j++] = g_projEditBuf[i];
  }
  tmp[j] = '\0';
  strncpy(g_projectCustomName, tmp, 47);
  g_projectCustomName[47] = '\0';
  projectCustomNameSave(g_projectCustomName);
}

void openProjectNameEditor() {
  panicForTrigger(PanicTrigger::ProjectNameTransition);
  memset(g_projEditBuf, ' ', static_cast<size_t>(kProjEditLen));
  g_projEditBuf[kProjEditLen] = '\0';
  const size_t len = strlen(g_projectCustomName);
  const size_t copyLen = len > static_cast<size_t>(kProjEditLen) ? static_cast<size_t>(kProjEditLen) : len;
  memcpy(g_projEditBuf, g_projectCustomName, copyLen);
  g_projEditCursor = 0;
  g_screen = Screen::ProjectNameEdit;
  drawProjectNameEdit();
}

void resolveBackupFolder(char out[48]) {
  if (g_projectCustomName[0] != '\0') {
    strncpy(out, g_projectCustomName, 47);
    out[47] = '\0';
    sdBackupSanitizeFolderName(out);
    if (out[0] == '\0') {
      sdBackupFormatDefaultProjectFolder(out, 48, g_model, g_projectBpm);
    }
  } else {
    sdBackupFormatDefaultProjectFolder(out, 48, g_model, g_projectBpm);
  }
}

void applySdRestoreFromFolder(const char* folder) {
  uint16_t bpm = g_projectBpm;
  if (!sdBackupReadAll(g_settings, g_model, g_seqPattern, g_seqMidiCh, &g_xyCcA, &g_xyCcB,
                       &g_xyOutChannel, &g_xyCurveA, &g_xyCurveB, &bpm, &g_chordVoicing, &g_seqExtras,
                       folder)) {
    panicForTrigger(PanicTrigger::ErrorPath);
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Restore failed");
    return;
  }
  g_projectBpm = bpm;
  projectBpmSave(g_projectBpm);
  settingsSave(g_settings);
  chordStateSave(g_model);
  seqPatternSave(g_seqPattern);
  seqLaneChannelsSave(g_seqMidiCh);
  chordVoicingSave(g_chordVoicing);
  seqExtrasSave(&g_seqExtras);
  xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
  lastProjectFolderSave(folder);
  applyBrightness();
  snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Restored OK");
}

static const char kNameCycle[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static void cycleProjectNameChar() {
  char c = g_projEditBuf[g_projEditCursor];
  const char* p = strchr(kNameCycle, c);
  int idx = p ? static_cast<int>(p - kNameCycle) : 0;
  idx = (idx + 1) % static_cast<int>(strlen(kNameCycle));
  g_projEditBuf[g_projEditCursor] = kNameCycle[idx];
}

static void layoutProjectNameButtons(int w, int h) {
  const int btnH = 28;
  const int btnY = h - kBezelBarH - btnH - 6;
  const int gap = 6;
  const int bw = (w - gap * 4) / 3;
  g_peSave = {gap, btnY, bw, btnH};
  g_peClear = {gap * 2 + bw, btnY, bw, btnH};
  g_peCancel = {gap * 3 + bw * 2, btnY, bw, btnH};
}

void drawProjectNameEdit() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillScreen(g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.drawString("Project folder name", w / 2, 4);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("empty = auto (date_key_mode_bpm)", w / 2, 18);

  const int baseY = 40;
  int cx = 8;
  for (int i = 0; i < kProjEditLen; ++i) {
    char ch[2] = {g_projEditBuf[i], '\0'};
    if (i == g_projEditCursor) {
      M5.Display.fillRect(cx - 1, baseY - 2, 10, 14, g_uiPalette.rowSelect);
      M5.Display.setTextColor(TFT_BLACK, g_uiPalette.rowSelect);
    } else {
      M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
    }
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString(ch, cx, baseY);
    cx += 11;
  }

  layoutProjectNameButtons(w, h);
  drawRoundedButton(g_peSave, g_uiPalette.settingsBtnIdle, "Save", 1);
  drawRoundedButton(g_peClear, g_uiPalette.settingsBtnIdle, "Clear", 1);
  drawRoundedButton(g_peCancel, g_uiPalette.settingsBtnIdle, "Cancel", 1);

  layoutBottomBezels(w, h);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
  M5.Display.drawString("BACK/FWD=cursor SEL=char", w / 2, g_peSave.y - 10);
  drawTopSystemStatus(w, 2, nullptr, 0);
  drawBezelBarStrip();
  M5.Display.endWrite();
}

void processProjectNameEditTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);
  layoutProjectNameButtons(w, h);

  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
  }

  if (touchCount == 0) {
    if (!wasTouchActive) return;
    wasTouchActive = false;
    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;
    if (pointInRect(hx, hy, g_peSave)) {
      panicForTrigger(PanicTrigger::ProjectNameTransition);
      applyProjectNameFromEditor();
      g_screen = Screen::Settings;
      drawSettingsUi();
      return;
    }
    if (pointInRect(hx, hy, g_peClear)) {
      memset(g_projEditBuf, ' ', static_cast<size_t>(kProjEditLen));
      g_projEditBuf[kProjEditLen] = '\0';
      g_projEditCursor = 0;
      drawProjectNameEdit();
      return;
    }
    if (pointInRect(hx, hy, g_peCancel)) {
      panicForTrigger(PanicTrigger::ProjectNameTransition);
      g_screen = Screen::Settings;
      drawSettingsUi();
      return;
    }
    if (pointInRect(hx, hy, g_bezelBack)) {
      if (g_projEditCursor > 0) {
        --g_projEditCursor;
      }
      drawProjectNameEdit();
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      if (g_projEditCursor < kProjEditLen - 1) {
        ++g_projEditCursor;
      }
      drawProjectNameEdit();
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      cycleProjectNameChar();
      drawProjectNameEdit();
      return;
    }
    drawProjectNameEdit();
    return;
  }

  wasTouchActive = true;
}

void drawSdProjectPick() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  M5.Display.fillScreen(g_uiPalette.bg);
  M5.Display.setFont(nullptr);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString("Choose project to restore", w / 2, 4);

  const int rowH = 26;
  const int top = 24;
  for (int i = 0; i < g_sdPickCount && i < 8; ++i) {
    g_sdPickRows[i] = {6, top + i * rowH, w - 12, rowH - 2};
    drawRoundedButton(g_sdPickRows[i], g_uiPalette.panelMuted, g_sdPickNames[i], 1);
  }
  g_sdPickCancel = {6, h - 44, w - 12, 36};
  drawRoundedButton(g_sdPickCancel, g_uiPalette.keyPickDone, "Cancel", 2);
  drawTopSystemStatus(w, 2, nullptr, 0);
  M5.Display.endWrite();
}

void processSdProjectPickTouch(uint8_t touchCount, int w, int h) {
  (void)w;
  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
    if (!wasTouchActive) {
      s_sdPickTouchStartX = g_lastTouchX;
      s_sdPickTouchStartY = g_lastTouchY;
    }
    wasTouchActive = true;
    return;
  }
  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;
  const bool movedPast = touchMovedPastSuppressThreshold(s_sdPickTouchStartX, s_sdPickTouchStartY, hx, hy);
  if (pointInRect(hx, hy, g_sdPickCancel)) {
    panicForTrigger(PanicTrigger::RestoreTransition);
    g_screen = Screen::Settings;
    drawSettingsUi();
    return;
  }
  if (!movedPast) {
    for (int i = 0; i < g_sdPickCount && i < 8; ++i) {
      if (pointInRect(hx, hy, g_sdPickRows[i])) {
        panicForTrigger(PanicTrigger::RestoreTransition);
        applySdRestoreFromFolder(g_sdPickNames[i]);
        g_screen = Screen::Settings;
        drawSettingsUi();
        return;
      }
    }
  }
  drawSdProjectPick();
}

}  // namespace m5chords_app
