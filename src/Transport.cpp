#include "Transport.h"

#include "SeqExtras.h"
#include "SettingsStore.h"

#include <M5Unified.h>

extern uint16_t g_projectBpm;
extern uint8_t g_seqPattern[3][16];
extern uint8_t g_seqLane;
extern uint8_t g_xyValX;
extern uint8_t g_xyValY;
extern uint8_t g_xyAutoA[16];
extern uint8_t g_xyAutoB[16];
extern SeqExtras g_seqExtras;

bool g_prefsMetronome = true;
bool g_prefsCountIn = true;
uint8_t g_clickVolumePercent = 70;
bool g_xyRecordToSeq = false;

namespace {

constexpr uint8_t kSeqRest = 0xFE;

enum class Phase : uint8_t { Idle, CountIn, Playing, Paused };

Phase s_phase = Phase::Idle;
uint32_t s_nextEventMs = 0;
uint8_t s_playhead = 0;
uint8_t s_audibleStep = 0;
uint8_t s_countInLeft = 0;
uint8_t s_countInDisplay = 0;
bool s_recording = false;
bool s_recordArmed = false;
int8_t s_liveChord = -1;

uint32_t beatIntervalMs() {
  uint16_t bpm = g_projectBpm;
  if (bpm < 40) bpm = 40;
  if (bpm > 300) bpm = 300;
  return 60000UL / static_cast<uint32_t>(bpm);
}

void playClickSound(bool accent) {
  if (g_clickVolumePercent == 0) return;
  uint8_t v = static_cast<uint8_t>((static_cast<uint32_t>(g_clickVolumePercent) * 255U) / 100U);
  if (v < 8) v = 8;
  M5.Speaker.setVolume(v);
  const float f = accent ? 1250.0f : 920.0f;
  M5.Speaker.tone(f, 28, -1, true);
}

void captureStep() {
  if (!s_recording) return;
  uint8_t lane = g_seqLane;
  if (lane > 2) lane = 0;
  uint8_t cell = kSeqRest;
  if (s_liveChord >= 0 && s_liveChord < 8) {
    cell = static_cast<uint8_t>(s_liveChord);
  }
  const uint8_t prob = g_seqExtras.stepProb[lane][s_playhead];
  if (!seqRollStepFires(prob)) {
    cell = kSeqRest;
  }
  g_seqPattern[lane][s_playhead] = cell;
  seqPatternSave(g_seqPattern);

  if (g_xyRecordToSeq) {
    xyAutoWriteStep(s_playhead, g_xyValX, g_xyValY, g_xyAutoA, g_xyAutoB);
  }
}

void advancePlayhead() {
  s_playhead = static_cast<uint8_t>((s_playhead + 1) & 15);
}

}  // namespace

void transportInit() {
  transportPrefsLoad();
  transportApplyClickVolume();
}

void transportApplyClickVolume() {
  uint8_t v = static_cast<uint8_t>((static_cast<uint32_t>(g_clickVolumePercent) * 255U) / 100U);
  M5.Speaker.setVolume(v);
}

void transportPrefsLoad() {
  transportPrefsLoadFromStore(&g_clickVolumePercent, &g_prefsMetronome, &g_prefsCountIn,
                              &g_xyRecordToSeq);
  xyAutoPatternLoad(g_xyAutoA, g_xyAutoB);
}

void transportPrefsSave() {
  transportPrefsSaveToStore(g_clickVolumePercent, g_prefsMetronome, g_prefsCountIn,
                            g_xyRecordToSeq);
}

void transportSetLiveChord(int8_t idx0_7_or_neg1) {
  s_liveChord = idx0_7_or_neg1;
}

void transportSetRecordArmed(bool armed) {
  s_recordArmed = armed;
}

bool transportRecordArmed() {
  return s_recordArmed;
}

bool transportIsRunning() {
  return s_phase == Phase::CountIn || s_phase == Phase::Playing || s_phase == Phase::Paused;
}

bool transportIsPaused() {
  return s_phase == Phase::Paused;
}

bool transportIsPlaying() {
  return s_phase == Phase::Playing;
}

bool transportIsRecordingLive() {
  return s_recording;
}

bool transportIsCountIn() {
  return s_phase == Phase::CountIn;
}

uint8_t transportPlayhead() {
  return s_playhead;
}

uint8_t transportAudibleStep() {
  return s_audibleStep;
}

uint8_t transportCountInNumber() {
  if (s_phase != Phase::CountIn) return 0;
  return s_countInDisplay;
}

void transportStop() {
  s_phase = Phase::Idle;
  s_playhead = 0;
  s_audibleStep = 0;
  s_recording = false;
  s_countInLeft = 0;
  s_countInDisplay = 0;
  s_recordArmed = false;
}

void transportTogglePlayPause(uint32_t nowMs) {
  if (s_phase == Phase::Playing) {
    s_phase = Phase::Paused;
    return;
  }
  if (s_phase == Phase::Paused) {
    s_phase = Phase::Playing;
    s_nextEventMs = nowMs + beatIntervalMs();
    return;
  }
}

void transportStartMetronomeOnly(uint32_t nowMs) {
  if (s_phase != Phase::Idle) return;
  s_playhead = 0;
  s_audibleStep = 0;
  s_recording = false;
  s_phase = Phase::Playing;
  s_nextEventMs = nowMs;
}

void transportBeginLiveRecording(uint32_t nowMs) {
  s_playhead = 0;
  s_audibleStep = 0;
  if (g_prefsCountIn) {
    s_phase = Phase::CountIn;
    s_countInLeft = 4;
    s_recording = false;
    s_nextEventMs = nowMs;
  } else {
    s_phase = Phase::Playing;
    s_recording = true;
    s_nextEventMs = nowMs;
  }
}

void transportTick(uint32_t nowMs) {
  if (s_phase != Phase::CountIn && s_phase != Phase::Playing) return;
  if (static_cast<int32_t>(nowMs - s_nextEventMs) < 0) return;

  const uint32_t beat = beatIntervalMs();

  if (s_phase == Phase::CountIn) {
    if (s_countInLeft > 0) {
      s_countInDisplay = s_countInLeft;
      playClickSound(true);
      s_countInLeft--;
      if (s_countInLeft == 0) {
        s_phase = Phase::Playing;
        s_recording = s_recordArmed;
        s_playhead = 0;
        s_audibleStep = 0;
        s_nextEventMs = nowMs;
        s_countInDisplay = 0;
        return;
      }
    }
    s_nextEventMs = nowMs + beat;
    return;
  }

  if (s_phase == Phase::Playing) {
    if (g_prefsMetronome) {
      // Accent downbeat every quarter note (4 steps per beat in the 16-step grid).
      playClickSound((s_playhead % 4) == 0);
    }
    const uint8_t stepJustPlayed = s_playhead;
    s_audibleStep = stepJustPlayed;
    if (s_recording) {
      captureStep();
    }
    uint8_t lane = g_seqLane;
    if (lane > 2) lane = 0;
    const uint32_t nextInt =
        seqSwingIntervalAfterStep(beat, g_seqExtras.swingPct[lane], stepJustPlayed);
    advancePlayhead();
    s_nextEventMs = nowMs + nextInt;
  }
}
