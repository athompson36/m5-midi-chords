#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

extern bool s_shiftActive;
extern bool s_shiftTriggeredThisHold;

constexpr uint32_t kShiftSeqNudgeHoldMs = 320U;

static int s_seqComboTab = -1;
static uint8_t s_seqGestureMaxTouches = 0;
static bool s_seqDraggingSlider = false;
static uint32_t s_seqStepDownMs = 0;
static int s_seqStepDownIdx = -1;
static bool s_seqLongPressHandled = false;
static int8_t s_shiftSeqNudgeDir = 0;
static uint32_t s_shiftSeqNudgeStartMs = 0;
static uint32_t s_shiftSeqNudgeLastMs = 0;
static bool s_shiftSeqNudgeConsumed = false;
static uint32_t s_seqTouchStartMs = 0;
static int s_lastSeqPreviewCell = -9999;
static bool s_wasSeqMultiFinger = false;

static uint32_t seqLongPressMs() {
  switch (g_settings.seqLongPressPreset) {
    case 0:
      return 250U;
    case 1:
      return 300U;
    case 2:
      return 400U;
    case 3:
      return 500U;
    default:
      return 700U;
  }
}

static void shiftSeqAdjustFocusedParam(int delta) {
  if (!s_shiftActive) return;
  if (s_shiftSeqFocusStep < 0 || s_shiftSeqFocusStep > 15) return;
  uint8_t L = g_seqLane;
  if (L > 2) L = 0;
  const uint8_t step = static_cast<uint8_t>(s_shiftSeqFocusStep);
  switch (s_shiftSeqFocusField & 0x03U) {
    case 0: {
      int v = static_cast<int>(g_seqExtras.stepClockDiv[L][step]) + delta;
      while (v < 0) v += 4;
      v %= 4;
      g_seqExtras.stepClockDiv[L][step] = static_cast<uint8_t>(v);
      break;
    }
    case 1:
      g_seqExtras.arpEnabled[L][step] = static_cast<uint8_t>(g_seqExtras.arpEnabled[L][step] ? 0U : 1U);
      break;
    case 2: {
      int v = static_cast<int>(g_seqExtras.arpPattern[L][step]) + delta;
      while (v < 0) v += 4;
      v %= 4;
      g_seqExtras.arpPattern[L][step] = static_cast<uint8_t>(v);
      break;
    }
    case 3: {
      int v = static_cast<int>(g_seqExtras.arpClockDiv[L][step]) + delta;
      while (v < 0) v += 4;
      v %= 4;
      g_seqExtras.arpClockDiv[L][step] = static_cast<uint8_t>(v);
      break;
    }
  }
}

static bool shiftSeqHandleBackFwdNudgeHold(int direction) {
  if (!s_shiftActive) return false;
  if (direction != -1 && direction != 1) return false;
  if (s_shiftSeqFocusStep < 0 || s_shiftSeqFocusStep > 15) return false;
  const uint32_t now = millis();
  if (s_shiftSeqNudgeDir != direction) {
    s_shiftSeqNudgeDir = static_cast<int8_t>(direction);
    s_shiftSeqNudgeStartMs = now;
    s_shiftSeqNudgeLastMs = 0;
    s_shiftSeqNudgeConsumed = false;
    return false;
  }
  if (now - s_shiftSeqNudgeStartMs < kShiftSeqNudgeHoldMs) {
    return false;
  }
  const uint32_t held = now - s_shiftSeqNudgeStartMs;
  uint32_t interval = 180U;
  if (held > 1600U) interval = 70U;
  else if (held > 1100U) interval = 95U;
  else if (held > 700U) interval = 130U;
  if (s_shiftSeqNudgeLastMs != 0 && (now - s_shiftSeqNudgeLastMs) < interval) {
    return true;
  }
  s_shiftSeqNudgeConsumed = true;
  s_shiftSeqNudgeLastMs = now;
  shiftSeqAdjustFocusedParam(direction);
  drawSequencerSurface();
  return true;
}

static void shiftSeqResetBackFwdNudgeHold() {
  s_shiftSeqNudgeDir = 0;
  s_shiftSeqNudgeStartMs = 0;
  s_shiftSeqNudgeLastMs = 0;
}

static void shiftSeqFocusSetForCurrentLane(int8_t step) {
  uint8_t L = seqLaneClamped();
  if (step < 0 || step > 15) {
    step = -1;
  }
  s_shiftSeqFocusStep = step;
  s_shiftSeqFocusStepByLane[L] = step;
}

static void shiftSeqFocusSyncFromCurrentLane() {
  const uint8_t L = seqLaneClamped();
  int8_t step = s_shiftSeqFocusStepByLane[L];
  if (step < 0 || step > 15) {
    step = -1;
  }
  s_shiftSeqFocusStep = step;
}

static int hitTestSeqChordDrop(int px, int py) {
  if (s_seqChordDropStep < 0 || s_seqChordDropStep >= 16) return -1;
  const int items = seqChordDropItemCount();
  int dx, dy, dropW, rowH, vis;
  seqChordDropComputeLayout(M5.Display.width(), M5.Display.height(), &dx, &dy, &dropW, &rowH, &vis);
  if (s_seqChordDropScroll < 0) s_seqChordDropScroll = 0;
  if (s_seqChordDropScroll + vis * 2 > items) s_seqChordDropScroll = max(0, items - vis * 2);
  constexpr int colGap = 4;
  const int colW = (dropW - colGap) / 2;
  for (int r = 0; r < vis; ++r) {
    for (int c = 0; c < 2; ++c) {
      const int itemIdx = s_seqChordDropScroll + r * 2 + c;
      if (itemIdx >= items) break;
      const int cellX = dx + c * (colW + colGap);
      if (px >= cellX && px < cellX + colW && py >= dy + r * rowH && py < dy + (r + 1) * rowH) {
        return itemIdx;
      }
    }
  }
  return -1;
}

void processSequencerTouch(uint8_t touchCount, int w, int h) {
  computeSequencerLayout(w, h);

  if (::ui::topDrawerIsOpen(g_topDrawerUi) && g_topDrawerUi.ignoreUntilTouchUp) {
    if (touchCount > 0) {
      wasTouchActive = true;
      return;
    }
    g_topDrawerUi.ignoreUntilTouchUp = false;
    wasTouchActive = false;
    return;
  }

  if (sequencerProcessTopDrawerTouch(touchCount, w, h)) return;

  if (tryEnterSettingsTwoFingerLong(touchCount, w, h)) {
    wasTouchActive = false;
    return;
  }

  if (touchCount > 0) {
    if (!wasTouchActive) {
      s_seqTouchStartMs = millis();
    }
    if (touchCount > s_seqGestureMaxTouches) {
      s_seqGestureMaxTouches = static_cast<uint8_t>(touchCount);
    }
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
  }

  bool selectDown = false;
  for (uint8_t i = 0; i < touchCount; ++i) {
    const auto& d = M5.Touch.getDetail(i);
    if (d.isPressed() && pointInRect(d.x, d.y, g_bezelSelect)) selectDown = true;
  }
  if (touchCount >= 2) {
    shiftSeqResetBackFwdNudgeHold();
    wasTouchActive = true;
    s_seqComboTab = -1;
    if (selectDown) {
      for (uint8_t i = 0; i < touchCount; ++i) {
        const auto& d = M5.Touch.getDetail(i);
        if (!d.isPressed()) continue;
        const int th = hitTestSeqTab(d.x, d.y);
        if (th >= 0) {
          s_seqComboTab = th;
          break;
        }
      }
    }
    s_wasSeqMultiFinger = true;
    return;
  }

  if (s_wasSeqMultiFinger) {
    s_wasSeqMultiFinger = false;
  }

  if (touchCount == 1) {
    const auto& d = M5.Touch.getDetail(0);
    if (s_seqChordDropStep >= 0 && d.isPressed() && !wasTouchActive && hitTestSeqChordDrop(d.x, d.y) >= 0) {
      s_seqChordDropTouchStartX = d.x;
      s_seqChordDropTouchStartY = d.y;
    }
    if (s_seqChordDropStep >= 0 && d.isPressed()) {
      if (s_seqChordDropDragLastY >= 0) {
        const int delta = d.y - s_seqChordDropDragLastY;
        if (delta > 18) {
          if (s_seqChordDropScroll > 0) {
            --s_seqChordDropScroll;
            ++s_seqChordDropFingerScrollCount;
            drawSequencerSurface(-1, false);
          }
          s_seqChordDropDragLastY = d.y;
        } else if (delta < -18) {
          const int vis = seqChordDropVisibleRows(h);
          const int cnt = seqChordDropItemCount();
          if (s_seqChordDropScroll + vis * 2 < cnt) {
            ++s_seqChordDropScroll;
            ++s_seqChordDropFingerScrollCount;
            drawSequencerSurface(-1, false);
          }
          s_seqChordDropDragLastY = d.y;
        }
      } else {
        s_seqChordDropDragLastY = d.y;
      }
    } else {
      s_seqChordDropDragLastY = -1;
    }
    if (s_shiftActive && d.isPressed()) {
      if (pointInRect(d.x, d.y, g_bezelBack)) {
        wasTouchActive = true;
        shiftSeqHandleBackFwdNudgeHold(-1);
        return;
      }
      if (pointInRect(d.x, d.y, g_bezelFwd)) {
        wasTouchActive = true;
        shiftSeqHandleBackFwdNudgeHold(1);
        return;
      }
      shiftSeqResetBackFwdNudgeHold();
    }
    if (g_seqTool != SeqTool::None && g_seqSliderActive && pointInRect(d.x, d.y, g_seqSliderPanel)) {
      const bool wasDragging = s_seqDraggingSlider;
      s_seqDraggingSlider = true;
      const uint8_t svBefore = seqSliderDisplayedValue();
      seqSetSliderValueFromX(d.x);
      const uint8_t svAfter = seqSliderDisplayedValue();
      if (!wasDragging || svBefore != svAfter) {
        drawSequencerSurface(-1, false);
      }
      wasTouchActive = true;
      return;
    }

    const uint32_t now = millis();
    if (s_seqChordDropStep < 0) {
      const int ht = hitTestSeq(d.x, d.y);
      if (ht >= 0 && !(seqSelectToolsActive() && ht < 4)) {
        if (s_seqStepDownIdx != ht) {
          s_seqStepDownIdx = ht;
          s_seqStepDownMs = now;
          s_seqLongPressHandled = false;
        }
        if (!s_seqLongPressHandled && now - s_seqStepDownMs >= seqLongPressMs()) {
          s_seqStepEditStep = ht;
          s_seqStepEditJustOpened = true;
          s_seqLongPressHandled = true;
          drawSequencerSurface(-1, false);
        }
      } else {
        s_seqStepDownIdx = -1;
      }
    }
    wasTouchActive = true;
    return;
  }

  if (touchCount == 0) {
    if (!wasTouchActive) return;
    shiftSeqResetBackFwdNudgeHold();
    if (s_shiftSeqNudgeConsumed) {
      s_shiftSeqNudgeConsumed = false;
      s_lastSeqPreviewCell = -9999;
      s_wasSeqMultiFinger = false;
      wasTouchActive = false;
      return;
    }
    if (millis() - s_seqTouchStartMs < kTapDebounceMs) {
      s_lastSeqPreviewCell = -9999;
      s_wasSeqMultiFinger = false;
      wasTouchActive = false;
      s_seqStepDownIdx = -1;
      s_seqLongPressHandled = false;
      return;
    }
    s_lastSeqPreviewCell = -9999;
    s_wasSeqMultiFinger = false;
    wasTouchActive = false;
    if (suppressNextPlayTap) {
      suppressNextPlayTap = false;
      drawSequencerSurface(-1, false);
      return;
    }

    if (s_seqDraggingSlider) {
      s_seqDraggingSlider = false;
      seqExtrasSave(&g_seqExtras);
      drawSequencerSurface(-1, false);
      return;
    }

    const int hx = g_lastTouchX;
    const int hy = g_lastTouchY;

    if (s_seqStepEditStep >= 0) {
      seqStepEditComputeLayout(w, h);
      if (s_seqStepEditJustOpened) {
        s_seqStepEditJustOpened = false;
        drawSequencerSurface(-1, false);
        return;
      }
      if (pointInRect(hx, hy, g_seqStepEditDone) || pointInRect(hx, hy, g_bezelSelect) ||
          pointInRect(hx, hy, g_bezelBack) || pointInRect(hx, hy, g_bezelFwd)) {
        s_seqStepEditStep = -1;
        s_seqStepEditJustOpened = false;
        seqExtrasSave(&g_seqExtras);
        drawSequencerSurface(-1, true);
        return;
      }
      const uint8_t lane = seqLaneClamped();
      const int step = s_seqStepEditStep;
      bool changed = false;
      if (pointInRect(hx, hy, g_seqStepEditDelete)) {
        g_seqPattern[lane][step] = kSeqRest;
        s_seqStepEditStep = -1;
        s_seqStepEditJustOpened = false;
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditVoMinus)) {
        uint8_t v = g_seqExtras.stepVoicing[lane][step];
        if (v > 1) {
          --v;
          g_seqExtras.stepVoicing[lane][step] = v;
          changed = true;
        }
      } else if (pointInRect(hx, hy, g_seqStepEditVoPlus)) {
        uint8_t v = g_seqExtras.stepVoicing[lane][step];
        if (v < 4) {
          ++v;
          g_seqExtras.stepVoicing[lane][step] = v;
          changed = true;
        }
      } else if (pointInRect(hx, hy, g_seqStepEditArpToggle)) {
        g_seqExtras.arpEnabled[lane][step] = g_seqExtras.arpEnabled[lane][step] ? 0U : 1U;
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditPatMinus)) {
        int v = static_cast<int>(g_seqExtras.arpPattern[lane][step] & 0x03U) - 1;
        if (v < 0) v = 3;
        g_seqExtras.arpPattern[lane][step] = static_cast<uint8_t>(v);
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditPatPlus)) {
        int v = static_cast<int>(g_seqExtras.arpPattern[lane][step] & 0x03U) + 1;
        if (v > 3) v = 0;
        g_seqExtras.arpPattern[lane][step] = static_cast<uint8_t>(v);
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditRateMinus)) {
        int v = static_cast<int>(g_seqExtras.arpClockDiv[lane][step] & 0x03U) - 1;
        if (v < 0) v = 3;
        g_seqExtras.arpClockDiv[lane][step] = static_cast<uint8_t>(v);
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditRatePlus)) {
        int v = static_cast<int>(g_seqExtras.arpClockDiv[lane][step] & 0x03U) + 1;
        if (v > 3) v = 0;
        g_seqExtras.arpClockDiv[lane][step] = static_cast<uint8_t>(v);
        changed = true;
      } else if (pointInRect(hx, hy, g_seqStepEditOctMinus)) {
        uint8_t v = g_seqExtras.arpOctRange[lane][step];
        if (v > 0) {
          --v;
          g_seqExtras.arpOctRange[lane][step] = v;
          changed = true;
        }
      } else if (pointInRect(hx, hy, g_seqStepEditOctPlus)) {
        uint8_t v = g_seqExtras.arpOctRange[lane][step];
        if (v < 2) {
          ++v;
          g_seqExtras.arpOctRange[lane][step] = v;
          changed = true;
        }
      } else if (pointInRect(hx, hy, g_seqStepEditGateMinus)) {
        int v = static_cast<int>(g_seqExtras.arpGatePct[lane][step]) - 5;
        if (v < 10) v = 10;
        if (v != g_seqExtras.arpGatePct[lane][step]) {
          g_seqExtras.arpGatePct[lane][step] = static_cast<uint8_t>(v);
          changed = true;
        }
      } else if (pointInRect(hx, hy, g_seqStepEditGatePlus)) {
        int v = static_cast<int>(g_seqExtras.arpGatePct[lane][step]) + 5;
        if (v > 100) v = 100;
        if (v != g_seqExtras.arpGatePct[lane][step]) {
          g_seqExtras.arpGatePct[lane][step] = static_cast<uint8_t>(v);
          changed = true;
        }
      }
      if (changed) {
        seqExtrasSave(&g_seqExtras);
      }
      drawSequencerSurface(-1, s_seqStepEditStep < 0);
      return;
    }

    if (s_seqGestureMaxTouches >= 2 && s_seqComboTab >= 0) {
      uint8_t ch = g_seqMidiCh[s_seqComboTab];
      ch = static_cast<uint8_t>((ch % 16) + 1);
      g_seqMidiCh[s_seqComboTab] = ch;
      seqLaneChannelsSave(g_seqMidiCh);
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      drawSequencerSurface(-1, false);
      return;
    }

    if (pointInRect(hx, hy, g_bezelBack)) {
      if (::ui::topDrawerIsOpen(g_topDrawerUi)) {
        ::ui::topDrawerClose(g_topDrawerUi);
        drawSequencerSurface(-1, true);
        return;
      }
      if (s_shiftActive && s_shiftSeqFocusStep >= 0) {
        shiftSeqAdjustFocusedParam(-1);
        drawSequencerSurface(-1, false);
        return;
      }
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      s_seqChordDropStep = -1;
      s_seqChordDropDragLastY = -1;
      s_seqChordDropFingerScrollCount = 0;
      s_seqSelectHeld = false;
      navigateMainRing(-1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelFwd)) {
      if (s_shiftActive && s_shiftSeqFocusStep >= 0) {
        shiftSeqAdjustFocusedParam(1);
        drawSequencerSurface(-1, false);
        return;
      }
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      s_seqChordDropStep = -1;
      s_seqChordDropDragLastY = -1;
      s_seqChordDropFingerScrollCount = 0;
      s_seqSelectHeld = false;
      navigateMainRing(1);
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect)) {
      if (s_shiftTriggeredThisHold) {
        s_shiftTriggeredThisHold = false;
        drawSequencerSurface(-1, false);
        return;
      }
      const uint8_t maxT = s_seqGestureMaxTouches;
      s_seqGestureMaxTouches = 0;
      s_seqComboTab = -1;
      const bool hadChordDrop = (s_seqChordDropStep >= 0);
      s_seqChordDropStep = -1;
      s_seqChordDropDragLastY = -1;
      s_seqChordDropFingerScrollCount = 0;
      if (maxT <= 1) s_seqSelectHeld = !s_seqSelectHeld;
      drawSequencerSurface(-1, hadChordDrop);
      return;
    }

    s_seqGestureMaxTouches = 0;
    s_seqComboTab = -1;

    if (s_seqChordDropStep >= 0) {
      const int fingerScrollsThisGesture = s_seqChordDropFingerScrollCount;
      s_seqChordDropFingerScrollCount = 0;
      s_seqChordDropDragLastY = -1;
      const int pick = hitTestSeqChordDrop(hx, hy);
      if (pick >= 0) {
        const bool haveAnchor = hitTestSeqChordDrop(s_seqChordDropTouchStartX, s_seqChordDropTouchStartY) >= 0;
        const bool movedPast = haveAnchor && touchMovedPastSuppressThreshold(s_seqChordDropTouchStartX,
                                                                             s_seqChordDropTouchStartY, hx, hy);
        if (!movedPast && fingerScrollsThisGesture == 0) {
          uint8_t v;
          if (pick < 8) v = static_cast<uint8_t>(pick);
          else if (pick == 8) v = kSeqTie;
          else if (pick == 9) v = kSeqRest;
          else v = kSeqSurprise;
          g_seqPattern[g_seqLane][s_seqChordDropStep] = v;
        }
      }
      s_seqChordDropStep = -1;
      drawSequencerSurface(-1, true);
      return;
    }

    if (seqSelectToolsActive()) {
      const int ht = hitTestSeq(hx, hy);
      if (ht >= 0 && ht < 4) {
        if (s_shiftActive) {
          const uint8_t L = seqLaneClamped();
          if (s_shiftSeqFocusStep >= 0 && s_shiftSeqFocusStep < 16) {
            s_shiftSeqFocusField = static_cast<uint8_t>(ht);
            if (ht == 0) {
              g_seqExtras.stepClockDiv[L][s_shiftSeqFocusStep] =
                  static_cast<uint8_t>((g_seqExtras.stepClockDiv[L][s_shiftSeqFocusStep] + 1U) & 0x03U);
            } else if (ht == 1) {
              g_seqExtras.arpEnabled[L][s_shiftSeqFocusStep] =
                  static_cast<uint8_t>(g_seqExtras.arpEnabled[L][s_shiftSeqFocusStep] ? 0U : 1U);
            } else if (ht == 2) {
              g_seqExtras.arpPattern[L][s_shiftSeqFocusStep] =
                  static_cast<uint8_t>((g_seqExtras.arpPattern[L][s_shiftSeqFocusStep] + 1U) & 0x03U);
            } else {
              g_seqExtras.arpClockDiv[L][s_shiftSeqFocusStep] =
                  static_cast<uint8_t>((g_seqExtras.arpClockDiv[L][s_shiftSeqFocusStep] + 1U) & 0x03U);
            }
          }
        } else {
          static const SeqTool kMap[4] = {SeqTool::Quantize, SeqTool::Swing, SeqTool::StepProb,
                                          SeqTool::ChordRand};
          const SeqTool tapped = kMap[ht];
          if (g_seqTool == tapped) {
            g_seqTool = SeqTool::None;
            g_seqProbFocusStep = -1;
          } else {
            g_seqTool = tapped;
            if (tapped != SeqTool::StepProb) g_seqProbFocusStep = -1;
          }
        }
        seqExtrasSave(&g_seqExtras);
        drawSequencerSurface(-1, false);
        return;
      }
    }

    const int tabHit = hitTestSeqTab(hx, hy);
    if (tabHit >= 0) {
      g_seqLane = static_cast<uint8_t>(tabHit);
      shiftSeqFocusSyncFromCurrentLane();
      drawSequencerSurface(-1, false);
      return;
    }

    if (s_seqLongPressHandled) {
      s_seqStepDownIdx = -1;
      s_seqLongPressHandled = false;
      drawSequencerSurface(-1, false);
      return;
    }

    const int cell = hitTestSeq(hx, hy);
    if (cell >= 0) {
      if (s_shiftActive) {
        shiftSeqFocusSetForCurrentLane(static_cast<int8_t>(cell));
        drawSequencerSurface(-1, false);
        return;
      }
      if (g_seqTool == SeqTool::StepProb) {
        g_seqProbFocusStep = static_cast<int8_t>(cell);
        seqExtrasSave(&g_seqExtras);
        drawSequencerSurface(-1, false);
        return;
      }
      s_seqChordDropStep = cell;
      s_seqChordDropDragLastY = -1;
      s_seqChordDropFingerScrollCount = 0;
      const int items = seqChordDropItemCount();
      const int vis = seqChordDropVisibleRows(h);
      uint8_t cur = g_seqPattern[g_seqLane][cell];
      int curIdx = 0;
      if (cur <= 7) curIdx = static_cast<int>(cur);
      else if (cur == kSeqTie) curIdx = 8;
      else if (cur == kSeqRest) curIdx = 9;
      else curIdx = 10;
      s_seqChordDropScroll = curIdx - vis;
      if (s_seqChordDropScroll < 0) s_seqChordDropScroll = 0;
      if (s_seqChordDropScroll + vis * 2 > items) s_seqChordDropScroll = max(0, items - vis * 2);
      s_seqChordDropTouchStartX = -9999;
      s_seqChordDropTouchStartY = -9999;
      drawSequencerSurface(-1, false);
      return;
    }
    s_seqStepDownIdx = -1;
    drawSequencerSurface(-1, false);
  }
}

}  // namespace m5chords_app
