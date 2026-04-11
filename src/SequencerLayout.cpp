#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

using Rect = ::ui::Rect;

constexpr int kSeqHitPadPx = 4;

bool pointInRectPad(int px, int py, const Rect& r, int pad) {
  return px >= (r.x - pad) && px < (r.x + r.w + pad) && py >= (r.y - pad) &&
         py < (r.y + r.h + pad);
}

void computeSequencerLayout(int w, int h) {
  layoutBottomBezels(w, h);
  const int margin = static_cast<int>(::ui::LayoutMetrics::OuterMargin);
  const int gap = static_cast<int>(::ui::LayoutMetrics::ControlGap);
  const int stripGap = static_cast<int>(::ui::LayoutMetrics::StripGap);
  constexpr int hintH = 0;
  constexpr int tabH = 20;
  const int tabGap = gap;
  const int tabY = hintH + stripGap;
  const int tabW = (w - margin * 2 - tabGap * 2) / 3;
  for (int t = 0; t < 3; ++t) {
    g_seqTabRects[t] = {margin + t * (tabW + tabGap), tabY, tabW, tabH};
  }
  g_seqSliderActive = (g_seqTool != SeqTool::None);
  const int gridTop = tabY + tabH + stripGap;
  int gridBottom = h - kBezelBarH - stripGap;
  if (g_seqSliderActive) {
    constexpr int panelH = 52;
    const int panelY = gridBottom - panelH;
    g_seqSliderPanel = {margin, panelY, w - 2 * margin, panelH};
    gridBottom = panelY - stripGap;
  }
  const int availH = gridBottom - gridTop;
  const int hGap = gap;
  const int vGap = gap;
  const int cellW = max(22, (w - 2 * margin - 3 * hGap) / 4);
  const int cellH = max(22, (availH - 3 * vGap) / 4);
  const int totalW = cellW * 4 + hGap * 3;
  const int ox = (w - totalW) / 2;
  const int oy = gridTop;
  for (int i = 0; i < 16; ++i) {
    const int row = i / 4;
    const int col = i % 4;
    g_seqCellRects[i] = {ox + col * (cellW + hGap), oy + row * (cellH + vGap), cellW, cellH};
  }
}

int hitTestSeqTab(int px, int py) {
  for (int t = 0; t < 3; ++t) {
    if (pointInRect(px, py, g_seqTabRects[t])) return t;
  }
  return -1;
}

uint8_t seqLaneClamped() { return g_seqLane > 2 ? 0 : g_seqLane; }

Rect seqSliderTrackRect() {
  const Rect& p = g_seqSliderPanel;
  const bool sub = (g_seqTool == SeqTool::StepProb);
  const int trackTop = sub ? (p.y + 23) : (p.y + 13);
  const int trackBottom = p.y + p.h - 3;
  const int trackH = max(10, trackBottom - trackTop);
  return {p.x + 6, trackTop, max(8, p.w - 44), trackH};
}

void seqSetSliderValueFromX(int px) {
  const Rect tr = seqSliderTrackRect();
  int rw = max(1, tr.w);
  int rel = px - tr.x;
  if (rel < 0) rel = 0;
  if (rel > rw) rel = rw;
  const uint8_t v = static_cast<uint8_t>((int64_t)rel * 100 / rw);
  const uint8_t L = seqLaneClamped();
  switch (g_seqTool) {
    case SeqTool::Quantize:
      g_seqExtras.quantizePct[L] = v;
      break;
    case SeqTool::Swing:
      g_seqExtras.swingPct[L] = v;
      break;
    case SeqTool::ChordRand:
      g_seqExtras.chordRandPct[L] = v;
      break;
    case SeqTool::StepProb:
      if (g_seqProbFocusStep < 0) {
        for (int i = 0; i < 16; ++i) {
          g_seqExtras.stepProb[L][i] = v;
        }
      } else {
        g_seqExtras.stepProb[L][g_seqProbFocusStep] = v;
      }
      break;
    default:
      break;
  }
}

uint8_t seqSliderDisplayedValue() {
  const uint8_t L = seqLaneClamped();
  switch (g_seqTool) {
    case SeqTool::Quantize:
      return g_seqExtras.quantizePct[L];
    case SeqTool::Swing:
      return g_seqExtras.swingPct[L];
    case SeqTool::ChordRand:
      return g_seqExtras.chordRandPct[L];
    case SeqTool::StepProb:
      if (g_seqProbFocusStep >= 0 && g_seqProbFocusStep < 16) {
        return g_seqExtras.stepProb[L][g_seqProbFocusStep];
      }
      return g_seqExtras.stepProb[L][0];
    default:
      return 0;
  }
}

int hitTestSeq(int px, int py) {
  for (int i = 0; i < 16; ++i) {
    if (pointInRectPad(px, py, g_seqCellRects[i], kSeqHitPadPx)) return i;
  }
  return -1;
}

}  // namespace m5chords_app
