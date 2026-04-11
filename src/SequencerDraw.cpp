#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

using Rect = ::ui::Rect;

extern bool s_shiftActive;

static const char* seqStepLabel(uint8_t v) {
  if (v == kSeqRest) return "-";
  if (v == kSeqTie) return "~";
  if (v == kSeqSurprise) return "S?";
  if (v <= 7) return g_model.surround[v].name;
  return "?";
}

bool seqSelectToolsActive() { return s_seqSelectHeld || s_shiftActive; }

int seqChordDropItemCount() {
  return 11;  // 8 surrounds + tie + rest + optional surprise pool token
}

int seqChordDropVisibleRows(int h) {
  constexpr int rowH = 56;
  const int maxH = h - kBezelBarH - 12;
  int rows = max(2, maxH / rowH);
  const int items = seqChordDropItemCount();
  const int maxRows = (items + 1) / 2;
  if (rows > maxRows) rows = maxRows;
  return rows;
}

void seqChordDropComputeLayout(int w, int h, int* outX, int* outY, int* outW, int* outRowH, int* outVis) {
  constexpr int rowH = 56;
  const int rowsVis = seqChordDropVisibleRows(h);
  const int totalH = rowH * rowsVis;
  const int dropW = min(w - 16, max(100, w / 3));
  const Rect& anchor = g_seqCellRects[s_seqChordDropStep];
  int dx = anchor.x + anchor.w / 2 - dropW / 2;
  if (dx < 8) dx = 8;
  if (dx + dropW > w - 8) dx = w - 8 - dropW;
  int dy = (h - kBezelBarH - totalH) / 2;
  if (dy < 4) dy = 4;
  *outX = dx;
  *outY = dy;
  *outW = dropW;
  *outRowH = rowH;
  *outVis = rowsVis;
}

void seqStepEditComputeLayout(int w, int h) {
  const int avail = h - kBezelBarH - 24;
  const int pw = min(w - 16, 300);
  int ph = min(avail, 268);
  if (ph > avail) ph = avail;
  if (ph < 168 && avail >= 168) ph = 168;
  const int px = (w - pw) / 2;
  const int py = max(6, (h - kBezelBarH - ph) / 2);
  const int btnH = 42;
  const int btnSide = 44;
  const int hdr = 26;
  const int footer = 36;
  int rowStep = 40;
  int minNeed = hdr + 6 * rowStep + footer;
  if (minNeed > ph) {
    rowStep = (ph - hdr - footer) / 6;
    if (rowStep < 30) rowStep = 30;
  }
  g_seqStepEditPanel = {px, py, pw, ph};
  const int rowVo = py + hdr;
  const int rowArp = rowVo + rowStep;
  const int rowPat = rowArp + rowStep;
  const int rowRate = rowPat + rowStep;
  const int rowOct = rowRate + rowStep;
  const int rowGate = rowOct + rowStep;
  g_seqStepEditVoMinus = {px + 8, rowVo, btnSide, btnH};
  g_seqStepEditVoPlus = {px + pw - 8 - btnSide, rowVo, btnSide, btnH};
  g_seqStepEditArpToggle = {px + pw / 2 - 48, rowArp, 96, btnH};
  g_seqStepEditPatMinus = {px + 8, rowPat, btnSide, btnH};
  g_seqStepEditPatPlus = {px + pw - 8 - btnSide, rowPat, btnSide, btnH};
  g_seqStepEditRateMinus = {px + 8, rowRate, btnSide, btnH};
  g_seqStepEditRatePlus = {px + pw - 8 - btnSide, rowRate, btnSide, btnH};
  g_seqStepEditOctMinus = {px + 8, rowOct, btnSide, btnH};
  g_seqStepEditOctPlus = {px + pw - 8 - btnSide, rowOct, btnSide, btnH};
  g_seqStepEditGateMinus = {px + 8, rowGate, btnSide, btnH};
  g_seqStepEditGatePlus = {px + pw - 8 - btnSide, rowGate, btnSide, btnH};
  g_seqStepEditDelete = {px + 12, py + ph - footer + 2, 96, footer - 6};
  g_seqStepEditDone = {px + pw - 108, py + ph - footer + 2, 96, footer - 6};
}

static void drawSeqStepEditPopup() {
  if (s_seqStepEditStep < 0 || s_seqStepEditStep >= 16) return;
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  seqStepEditComputeLayout(w, h);
  const uint8_t lane = seqLaneClamped();
  const int step = s_seqStepEditStep;
  uint8_t vo = g_seqExtras.stepVoicing[lane][step];
  if (vo < 1) vo = 1;
  if (vo > 4) vo = 4;
  const bool arpOn = g_seqExtras.arpEnabled[lane][step] != 0U;
  const uint8_t pat = g_seqExtras.arpPattern[lane][step] & 0x03U;
  const uint8_t rate = g_seqExtras.arpClockDiv[lane][step] & 0x03U;
  const uint8_t oct = g_seqExtras.arpOctRange[lane][step] > 2 ? 2 : g_seqExtras.arpOctRange[lane][step];
  uint8_t gate = g_seqExtras.arpGatePct[lane][step];
  if (gate < 10) gate = 10;
  if (gate > 100) gate = 100;

  M5.Display.fillRoundRect(g_seqStepEditPanel.x, g_seqStepEditPanel.y, g_seqStepEditPanel.w, g_seqStepEditPanel.h, 8,
                           g_uiPalette.panelMuted);
  M5.Display.drawRoundRect(g_seqStepEditPanel.x, g_seqStepEditPanel.y, g_seqStepEditPanel.w, g_seqStepEditPanel.h, 8,
                           g_uiPalette.settingsBtnBorder);
  M5.Display.setFont(nullptr);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.panelMuted);
  char hdr[24];
  snprintf(hdr, sizeof(hdr), "Step %u", (unsigned)(step + 1));
  M5.Display.drawString(hdr, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2, g_seqStepEditPanel.y + 6);

  drawRoundedButton(g_seqStepEditVoMinus, g_uiPalette.accentPress, "-", 2);
  drawRoundedButton(g_seqStepEditVoPlus, g_uiPalette.accentPress, "+", 2);
  drawRoundedButton(g_seqStepEditArpToggle, arpOn ? g_uiPalette.seqLaneTab : g_uiPalette.panelMuted,
                    arpOn ? "ARP On" : "ARP Off", 1);
  drawRoundedButton(g_seqStepEditPatMinus, g_uiPalette.accentPress, "-", 2);
  drawRoundedButton(g_seqStepEditPatPlus, g_uiPalette.accentPress, "+", 2);
  drawRoundedButton(g_seqStepEditRateMinus, g_uiPalette.accentPress, "-", 2);
  drawRoundedButton(g_seqStepEditRatePlus, g_uiPalette.accentPress, "+", 2);
  drawRoundedButton(g_seqStepEditOctMinus, g_uiPalette.accentPress, "-", 2);
  drawRoundedButton(g_seqStepEditOctPlus, g_uiPalette.accentPress, "+", 2);
  drawRoundedButton(g_seqStepEditGateMinus, g_uiPalette.accentPress, "-", 2);
  drawRoundedButton(g_seqStepEditGatePlus, g_uiPalette.accentPress, "+", 2);
  drawRoundedButton(g_seqStepEditDelete, g_uiPalette.danger, "Delete", 1);
  drawRoundedButton(g_seqStepEditDone, g_uiPalette.seqLaneTab, "Done", 1);

  char line[40];
  M5.Display.setTextDatum(middle_center);
  snprintf(line, sizeof(line), "Voicing: V%u", (unsigned)vo);
  M5.Display.drawString(line, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2,
                        g_seqStepEditVoMinus.y + g_seqStepEditVoMinus.h / 2);
  snprintf(line, sizeof(line), "Pattern: %s", kSeqArpPatLabs[pat]);
  M5.Display.drawString(line, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2,
                        g_seqStepEditPatMinus.y + g_seqStepEditPatMinus.h / 2);
  snprintf(line, sizeof(line), "Rate: %s", kSeqStepDivLabs[rate]);
  M5.Display.drawString(line, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2,
                        g_seqStepEditRateMinus.y + g_seqStepEditRateMinus.h / 2);
  snprintf(line, sizeof(line), "Octave range: %u", (unsigned)oct);
  M5.Display.drawString(line, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2,
                        g_seqStepEditOctMinus.y + g_seqStepEditOctMinus.h / 2);
  snprintf(line, sizeof(line), "Gate: %u%%", (unsigned)gate);
  M5.Display.drawString(line, g_seqStepEditPanel.x + g_seqStepEditPanel.w / 2,
                        g_seqStepEditGateMinus.y + g_seqStepEditGateMinus.h / 2);
}

static void drawSequencerActiveStepGlow(const Rect& r) {
  const int rad0 = max(3, min(r.w, r.h) / 8);
  M5.Display.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, rad0, g_uiPalette.seqPlayGlowOuter);
  if (r.w > 12 && r.h > 12) {
    const int rad1 = max(2, rad0 - 2);
    M5.Display.drawRoundRect(r.x + 3, r.y + 3, r.w - 6, r.h - 6, rad1, g_uiPalette.highlightRing);
  }
  if (r.w > 18 && r.h > 18) {
    const int rad2 = max(2, rad0 - 3);
    M5.Display.drawRoundRect(r.x + 5, r.y + 5, r.w - 10, r.h - 10, rad2, g_uiPalette.seqPlayGlowInner);
  }
}

static void drawSequencerPaintCell(int i, int fingerCell) {
  const Rect& r = g_seqCellRects[i];
  static const char* kToolLabs[4] = {"Qnt", "Swg", "Prb", "Rnd"};
  static const SeqTool kToolMap[4] = {SeqTool::Quantize, SeqTool::Swing, SeqTool::StepProb,
                                      SeqTool::ChordRand};

  if (seqSelectToolsActive() && i < 4) {
    if (s_shiftActive) {
      const uint8_t L = seqLaneClamped();
      char lab[16];
      if (s_shiftSeqFocusStep < 0 || s_shiftSeqFocusStep > 15) {
        snprintf(lab, sizeof(lab), "%s",
                 i == 0 ? "Div --" : (i == 1 ? "Arp --" : (i == 2 ? "Pat --" : "ADiv --")));
      } else if (i == 0) {
        snprintf(lab, sizeof(lab), "Div %s",
                 kSeqStepDivLabs[g_seqExtras.stepClockDiv[L][s_shiftSeqFocusStep] & 0x03U]);
      } else if (i == 1) {
        snprintf(lab, sizeof(lab), "Arp %s", g_seqExtras.arpEnabled[L][s_shiftSeqFocusStep] ? "On" : "Off");
      } else if (i == 2) {
        snprintf(lab, sizeof(lab), "Pat %s",
                 kSeqArpPatLabs[g_seqExtras.arpPattern[L][s_shiftSeqFocusStep] & 0x03U]);
      } else {
        snprintf(lab, sizeof(lab), "ADiv %s",
                 kSeqStepDivLabs[g_seqExtras.arpClockDiv[L][s_shiftSeqFocusStep] & 0x03U]);
      }
      drawRoundedButton(r, g_uiPalette.accentPress, lab, 1);
      if (s_shiftSeqFocusField == static_cast<uint8_t>(i)) {
        M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                                 g_uiPalette.highlightRing);
      }
    } else {
      const bool on = (g_seqTool == kToolMap[i]);
      uint16_t bg = on ? g_uiPalette.seqLaneTab : g_uiPalette.accentPress;
      drawRoundedButton(r, bg, kToolLabs[i], 1);
      if (on) {
        M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                                 g_uiPalette.highlightRing);
      }
    }
    return;
  }

  const uint8_t v = g_seqPattern[g_seqLane][i];
  uint16_t bg = g_uiPalette.panelMuted;
  if (v == kSeqRest) {
    bg = g_uiPalette.seqRest;
  } else if (v == kSeqTie) {
    bg = g_uiPalette.seqTie;
  } else if (v == kSeqSurprise) {
    bg = g_uiPalette.surprise;
  } else if (v <= 7) {
    bg = colorForRole(g_model.surround[v].role);
  } else {
    bg = g_uiPalette.panelMuted;
  }
  if (i == fingerCell) {
    bg = g_uiPalette.accentPress;
  }
  drawRoundedButton(r, bg, seqStepLabel(v), 1);
  const uint8_t lane = seqLaneClamped();
  const uint8_t stepDiv = g_seqExtras.stepClockDiv[lane][i] & 0x03U;
  const bool arpOn = g_seqExtras.arpEnabled[lane][i] != 0U;
  const uint8_t arpPat = g_seqExtras.arpPattern[lane][i] & 0x03U;
  const uint8_t arpDiv = g_seqExtras.arpClockDiv[lane][i] & 0x03U;
  if (r.h >= 24 && (stepDiv > 0U || arpOn)) {
    M5.Display.setFont(nullptr);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.hintText, bg);
    if (stepDiv > 0U) {
      char db[8];
      snprintf(db, sizeof(db), "D%s", kSeqStepDivLabs[stepDiv]);
      M5.Display.setTextDatum(top_left);
      M5.Display.drawString(db, r.x + 2, r.y + 2);
    }
    if (arpOn) {
      char ab[12];
      if (arpPat == 0U && arpDiv == 0U) {
        snprintf(ab, sizeof(ab), "A");
      } else {
        snprintf(ab, sizeof(ab), "A%s%s", kSeqArpPatLabs[arpPat], kSeqStepDivLabs[arpDiv]);
      }
      M5.Display.setTextDatum(top_right);
      M5.Display.drawString(ab, r.x + r.w - 2, r.y + 2);
    }
  }
  const uint8_t probV = g_seqExtras.stepProb[lane][i];
  if (probV < 100 && r.h >= 24) {
    M5.Display.setFont(nullptr);
    M5.Display.setTextDatum(bottom_right);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.hintText, bg);
    char pr[8];
    snprintf(pr, sizeof(pr), "%u", (unsigned)probV);
    M5.Display.drawString(pr, r.x + r.w - 2, r.y + r.h - 2);
  }
  if (transportIsPlaying() && transportAudibleStep() == static_cast<uint8_t>(i)) {
    drawSequencerActiveStepGlow(r);
  }
  if (g_seqTool == SeqTool::StepProb && g_seqProbFocusStep == i) {
    M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                             g_uiPalette.highlightRing);
  }
  if (s_shiftActive && s_shiftSeqFocusStep == i) {
    M5.Display.drawRoundRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4, max(4, r.h / 8),
                             g_uiPalette.settingsBtnActive);
  }
}

void sequencerRedrawPlayheadDelta(uint8_t oldAudible, uint8_t newAudible) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  computeSequencerLayout(w, h);
  M5.Display.startWrite();
  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  if (oldAudible < 16) {
    drawSequencerPaintCell(static_cast<int>(oldAudible), -1);
  }
  if (newAudible < 16) {
    drawSequencerPaintCell(static_cast<int>(newAudible), -1);
  }
  M5.Display.endWrite();
}

void drawSequencerSurface(int fingerCell, bool fullClear) {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  M5.Display.startWrite();
  if (fullClear) M5.Display.fillRect(0, 0, w, h - kBezelBarH, g_uiPalette.bg);
  computeSequencerLayout(w, h);

  M5.Display.setFont(nullptr);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);

  for (int t = 0; t < 3; ++t) {
    char lab[20];
    snprintf(lab, sizeof(lab), "S%u ch%u", t + 1, (unsigned)g_seqMidiCh[t]);
    const bool on = (static_cast<int>(g_seqLane) == t);
    uint16_t bg = on ? g_uiPalette.seqLaneTab : g_uiPalette.panelMuted;
    drawRoundedButton(g_seqTabRects[t], bg, lab, 1);
    if (on) {
      M5.Display.drawRoundRect(g_seqTabRects[t].x, g_seqTabRects[t].y, g_seqTabRects[t].w,
                               g_seqTabRects[t].h, max(4, g_seqTabRects[t].h / 8),
                               g_uiPalette.highlightRing);
    }
  }

  for (int i = 0; i < 16; ++i) {
    drawSequencerPaintCell(i, fingerCell);
  }

  if (g_seqSliderActive) {
    M5.Display.fillRoundRect(g_seqSliderPanel.x, g_seqSliderPanel.y, g_seqSliderPanel.w,
                             g_seqSliderPanel.h, 6, g_uiPalette.panelMuted);
    M5.Display.drawRoundRect(g_seqSliderPanel.x, g_seqSliderPanel.y, g_seqSliderPanel.w,
                             g_seqSliderPanel.h, 6, g_uiPalette.settingsBtnBorder);
    const char* title = "";
    switch (g_seqTool) {
      case SeqTool::Quantize: title = "Quantize"; break;
      case SeqTool::Swing: title = "Swing"; break;
      case SeqTool::StepProb: title = "Step probability"; break;
      case SeqTool::ChordRand: title = "Chord random"; break;
      default: break;
    }
    M5.Display.setTextDatum(top_center);
    M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.panelMuted);
    M5.Display.setTextSize(1);
    M5.Display.drawString(title, g_seqSliderPanel.x + g_seqSliderPanel.w / 2,
                          g_seqSliderPanel.y + 4);
    if (g_seqTool == SeqTool::StepProb) {
      char sub[40];
      if (g_seqProbFocusStep >= 0) {
        snprintf(sub, sizeof(sub), "step %u", (unsigned)g_seqProbFocusStep + 1);
      } else {
        snprintf(sub, sizeof(sub), "all steps");
      }
      M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.panelMuted);
      M5.Display.drawString(sub, g_seqSliderPanel.x + g_seqSliderPanel.w / 2,
                            g_seqSliderPanel.y + 14);
    }
    const Rect tr = seqSliderTrackRect();
    M5.Display.fillRoundRect(tr.x, tr.y, tr.w, tr.h, 6, g_uiPalette.bg);
    const uint8_t sv = seqSliderDisplayedValue();
    const int fillW = max(0, (tr.w * static_cast<int>(sv)) / 100);
    if (fillW > 0) {
      M5.Display.fillRoundRect(tr.x, tr.y, fillW, tr.h, 6, g_uiPalette.seqLaneTab);
    }
    M5.Display.drawRoundRect(tr.x, tr.y, tr.w, tr.h, 6, g_uiPalette.settingsBtnBorder);
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", (unsigned)sv);
    M5.Display.setTextDatum(middle_right);
    M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.panelMuted);
    M5.Display.drawString(pct, g_seqSliderPanel.x + g_seqSliderPanel.w - 6,
                          tr.y + tr.h / 2);
  }

  if (s_seqChordDropStep >= 0 && s_seqChordDropStep < 16) {
    const int items = seqChordDropItemCount();
    int dx, dy, dropW, rowH, vis;
    seqChordDropComputeLayout(w, h, &dx, &dy, &dropW, &rowH, &vis);
    if (s_seqChordDropScroll < 0) s_seqChordDropScroll = 0;
    if (s_seqChordDropScroll + vis * 2 > items) s_seqChordDropScroll = max(0, items - vis * 2);
    constexpr int colGap = 4;
    const int colW = (dropW - colGap) / 2;
    const int totalH = rowH * vis;
    M5.Display.fillRect(dx - 2, dy - 2, dropW + 4, totalH + 4, g_uiPalette.bg);
    for (int r = 0; r < vis; ++r) {
      for (int c = 0; c < 2; ++c) {
        const int itemIdx = s_seqChordDropScroll + r * 2 + c;
        if (itemIdx >= items) break;
        const int cellX = dx + c * (colW + colGap);
        const Rect dr = {cellX, dy + r * rowH, colW, rowH};
        const char* lab;
        if (itemIdx < 8) {
          lab = g_model.surround[itemIdx].name;
        } else if (itemIdx == 8) {
          lab = "~ tie";
        } else if (itemIdx == 9) {
          lab = "- rest";
        } else {
          lab = "S? surprise";
        }
        uint16_t bg;
        if (itemIdx < 8) {
          bg = colorForRole(g_model.surround[itemIdx].role);
        } else if (itemIdx == 8) {
          bg = g_uiPalette.seqTie;
        } else if (itemIdx == 9) {
          bg = g_uiPalette.seqRest;
        } else {
          bg = g_uiPalette.surprise;
        }
        drawRoundedButton(dr, bg, lab, 1);
      }
    }
    if (items > vis * 2) {
      const int railX = dx + dropW - 3;
      M5.Display.drawFastVLine(railX, dy, totalH, g_uiPalette.subtle);
      const int thumbH = max(14, (totalH * vis) / items);
      const int thumbY = dy + ((totalH - thumbH) * s_seqChordDropScroll) / max(1, items - vis * 2);
      M5.Display.fillRect(railX - 1, thumbY, 3, thumbH, g_uiPalette.rowNormal);
    }
  }

  if (s_seqStepEditStep >= 0 && s_seqStepEditStep < 16) {
    drawSeqStepEditPopup();
  }

  drawBezelBarStrip();
  M5.Display.endWrite();
  paintAppTopDrawer();
}

}  // namespace m5chords_app
