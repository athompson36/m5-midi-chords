#pragma once

// First include for `src/screens/*Screen.cpp`: same headers as main, plus `extern` for globals defined in main.cpp.
// Do not include this from main.cpp (it defines those symbols).

#include "m5chords_app/M5ChordsAppCommonIncludes.h"

extern SeqExtras g_seqExtras;
extern uint8_t g_seqPattern[3][16];
extern uint8_t g_seqLane;
extern uint8_t g_chordVoicing;

extern ::ui::TopDrawerState g_topDrawerUi;

extern AppSettings g_settings;
extern int g_lastTouchX;
extern int g_lastTouchY;
extern uint8_t g_xyValX;
extern uint8_t g_xyValY;
extern bool wasTouchActive;
extern uint16_t g_projectBpm;
