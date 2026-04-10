#pragma once

#include <stdint.h>

/// Internal transport: metronome (speaker), count-in, playhead, optional live chord + XY capture.
void transportInit();
void transportTick(uint32_t nowMs);

void transportStop();
void transportTogglePlayPause(uint32_t nowMs);

/// Record arm (from Transport UI). Live capture only runs while playing + this set.
void transportSetRecordArmed(bool armed);
bool transportRecordArmed();

bool transportIsRunning();   ///< Playing or count-in (time is moving)
bool transportIsPaused();
bool transportIsPlaying();   ///< Playhead advancing (after count-in)
bool transportIsRecordingLive();  ///< Live capture into pattern active this take
bool transportIsCountIn();
uint8_t transportPlayhead();  ///< 0–15 (next step index after last tick)
uint8_t transportAudibleStep(); ///< Step for last metronome tick (for UI highlight)
uint8_t transportCountInNumber();  ///< 4–1 during count-in, else 0
uint32_t transportStepWindowMs();  ///< Last scheduled step window in milliseconds.

void transportApplyClickVolume();  ///< Sync M5.Speaker master from prefs
void transportSetGlobalSwing(uint8_t pct0_100);  ///< Global swing add-on for step interval timing.
void transportSetGlobalHumanize(uint8_t pct0_100);  ///< Global humanize add-on for step interval timing.

void transportStartMetronomeOnly(uint32_t nowMs);
void transportBeginLiveRecording(uint32_t nowMs);

/// Last chord for live step capture (0–7, or -1 = rest). Main updates on chord taps.
void transportSetLiveChord(int8_t idx0_7_or_neg1);

// External MIDI clock / transport foundation.
void transportOnExternalStart(uint32_t nowMs);
void transportOnExternalContinue(uint32_t nowMs);
void transportOnExternalStop();
void transportOnExternalClockTick(uint32_t nowMs);
void transportOnExternalSongPosition(uint16_t spp);
uint16_t transportExternalClockBpm();
bool transportExternalClockActive();
/// After Stop (host or local), clock ticks alone must not re-arm sync until Start or Continue.
bool transportExternalMayArmFromClockStream();
/// True if MIDI clock from the host has been absent long enough while slaved (host often omits 0xFC).
bool transportExternalClockStalled(uint32_t nowMs);

extern bool g_prefsMetronome;
extern bool g_prefsCountIn;
extern uint8_t g_clickVolumePercent;
extern bool g_xyRecordToSeq;

void transportPrefsLoad();
void transportPrefsSave();
