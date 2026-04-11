#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

namespace {

// Surround slots 0..7 clockwise from top-left; center (1,1) is the key.
constexpr int kChordSlotRow[ChordModel::kSurroundCount] = {0, 0, 0, 1, 2, 2, 2, 1};
constexpr int kChordSlotCol[ChordModel::kSurroundCount] = {0, 1, 2, 2, 2, 1, 0, 0};

}  // namespace

void computePlaySurfaceLayout(int w, int h) {
  layoutBottomBezels(w, h);
  constexpr int hintH = 0;
  constexpr int padAfterHint = 0;
  const int outer = static_cast<int>(::ui::LayoutMetrics::OuterMargin);
  const int gap = static_cast<int>(::ui::LayoutMetrics::ControlGap);
  // Keep the 3×3 chord grid below the top status row (battery, links) so cells never cover it.
  const int gridTop = max(hintH + padAfterHint, static_cast<int>(::ui::LayoutMetrics::StatusH));
  const int gridBottom = h - kBezelBarH;
  const int innerW = w - 2 * outer;
  const int innerH = gridBottom - gridTop;
  const int cellFromW = (innerW - 2 * gap) / 3;
  const int cellFromH = (innerH - 2 * gap) / 3;
  const int cell = min(cellFromW, cellFromH);
  const int totalW = cell * 3 + gap * 2;
  const int totalH = cell * 3 + gap * 2;
  const int ox = outer + (innerW - totalW) / 2;
  const int oy = gridBottom - totalH;

  g_keyRect = {ox + (cell + gap), oy + (cell + gap), cell, cell};
  for (int i = 0; i < ChordModel::kSurroundCount; ++i) {
    const int row = kChordSlotRow[i];
    const int col = kChordSlotCol[i];
    g_chordRects[i] = {ox + col * (cell + gap), oy + row * (cell + gap), cell, cell};
  }
}

}  // namespace m5chords_app
