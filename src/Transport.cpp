#include "Transport.h"

#include "SeqExtras.h"
#include "SettingsStore.h"

#include <M5Unified.h>
#include <stdlib.h>

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
bool s_externalClockDrive = false;
uint8_t s_externalClockTickPhase = 0;  // 6 MIDI clocks per sequencer step.
uint32_t s_externalPrevClockMs = 0;
uint32_t s_externalAvgClockMs = 0;
uint16_t s_externalClockBpm = 0;
uint8_t s_globalSwingPct = 0;
uint8_t s_globalHumanizePct = 0;
uint32_t s_lastStepWindowMs = 500;

uint32_t beatIntervalMs() {
  uint16_t bpm = g_projectBpm;
  if (bpm < 40) bpm = 40;
  if (bpm > 300) bpm = 300;
  return 60000UL / static_cast<uint32_t>(bpm);
}

uint32_t seqStepBaseIntervalMs(uint8_t stepJustPlayed) {
  uint8_t lane = g_seqLane;
  if (lane > 2) lane = 0;
  const uint8_t divEnum = g_seqExtras.stepClockDiv[lane][stepJustPlayed & 0x0F] & 0x03U;
  const uint32_t div = 1UL << divEnum;  // 0..3 => 1x/2x/4x/8x.
  const uint32_t beat = beatIntervalMs();
  uint32_t stepMs = beat / div;
  if (stepMs == 0) stepMs = 1;
  return stepMs;
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

void emitCurrentStep() {
  if (g_prefsMetronome) {
    playClickSound((s_playhead % 4) == 0);
  }
  const uint8_t stepJustPlayed = s_playhead;
  s_audibleStep = stepJustPlayed;
  if (s_recording) {
    captureStep();
  }
  advancePlayhead();
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

void transportSetGlobalSwing(uint8_t pct0_100) {
  s_globalSwingPct = (pct0_100 > 100U) ? 100U : pct0_100;
}

void transportSetGlobalHumanize(uint8_t pct0_100) {
  s_globalHumanizePct = (pct0_100 > 100U) ? 100U : pct0_100;
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

uint32_t transportStepWindowMs() {
  return s_lastStepWindowMs;
}

void transportStop() {
  s_phase = Phase::Idle;
  s_playhead = 0;
  s_audibleStep = 0;
  s_recording = false;
  s_countInLeft = 0;
  s_countInDisplay = 0;
  s_recordArmed = false;
  s_externalClockDrive = false;
  s_externalClockTickPhase = 0;
  s_externalPrevClockMs = 0;
  s_externalAvgClockMs = 0;
  s_externalClockBpm = 0;
  s_lastStepWindowMs = beatIntervalMs();
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
  s_externalClockDrive = false;
  s_phase = Phase::Playing;
  s_nextEventMs = nowMs;
}

void transportBeginLiveRecording(uint32_t nowMs) {
  s_externalClockDrive = false;
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
  if (s_externalClockDrive) return;
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
    const uint8_t stepJustPlayed = s_playhead;
    emitCurrentStep();
    uint8_t lane = g_seqLane;
    if (lane > 2) lane = 0;
    const uint32_t stepBase = seqStepBaseIntervalMs(stepJustPlayed);
    const uint16_t combinedSwing =
        static_cast<uint16_t>(g_seqExtras.swingPct[lane]) + static_cast<uint16_t>(s_globalSwingPct);
    const uint8_t swing = static_cast<uint8_t>(combinedSwing > 100U ? 100U : combinedSwing);
    uint32_t nextInt = seqSwingIntervalAfterStep(stepBase, swing, stepJustPlayed);
    if (s_globalHumanizePct > 0 && nextInt > 0) {
      const uint32_t maxJitterMs = (nextInt * static_cast<uint32_t>(s_globalHumanizePct)) / 500U;  // up to 20%
      if (maxJitterMs > 0) {
        const int span = static_cast<int>(maxJitterMs * 2U + 1U);
        const int jitter = (rand() % span) - static_cast<int>(maxJitterMs);
        const int64_t varied = static_cast<int64_t>(nextInt) + static_cast<int64_t>(jitter);
        nextInt = static_cast<uint32_t>(varied < 30 ? 30 : varied);
      }
    }
    s_lastStepWindowMs = nextInt;
    s_nextEventMs = nowMs + nextInt;
  }
}

void transportOnExternalStart(uint32_t nowMs) {
  s_externalClockDrive = true;
  s_externalClockTickPhase = 0;
  s_externalPrevClockMs = 0;
  s_phase = Phase::Playing;
  s_playhead = 0;
  s_audibleStep = 0;
  s_countInLeft = 0;
  s_countInDisplay = 0;
  s_recording = s_recordArmed;
  s_nextEventMs = nowMs;
}

void transportOnExternalContinue(uint32_t nowMs) {
  s_externalClockDrive = true;
  s_phase = Phase::Playing;
  s_recording = s_recordArmed;
  s_nextEventMs = nowMs;
}

void transportOnExternalStop() {
  s_phase = Phase::Idle;
  s_recording = false;
  s_countInLeft = 0;
  s_countInDisplay = 0;
  s_externalClockDrive = false;
  s_externalClockTickPhase = 0;
}

void transportOnExternalClockTick(uint32_t nowMs) {
  if (!s_externalClockDrive || s_phase != Phase::Playing) return;

  if (s_externalPrevClockMs != 0 && nowMs > s_externalPrevClockMs) {
    const uint32_t dt = nowMs - s_externalPrevClockMs;
    if (dt >= 1 && dt <= 250) {
      if (s_externalAvgClockMs == 0) {
        s_externalAvgClockMs = dt;
      } else {
        s_externalAvgClockMs = (s_externalAvgClockMs * 7U + dt) / 8U;
      }
      const uint32_t bpm = 60000UL / (s_externalAvgClockMs * 24UL);
      if (bpm >= 40 && bpm <= 300) {
        s_externalClockBpm = static_cast<uint16_t>(bpm);
      }
    }
  }
  s_externalPrevClockMs = nowMs;

  s_externalClockTickPhase = static_cast<uint8_t>((s_externalClockTickPhase + 1U) % 6U);
  if (s_externalClockTickPhase == 0) {
    if (s_externalAvgClockMs > 0) {
      s_lastStepWindowMs = s_externalAvgClockMs * 6U;
    } else {
      s_lastStepWindowMs = beatIntervalMs();
    }
    emitCurrentStep();
  }
}

void transportOnExternalSongPosition(uint16_t spp) {
  // SPP units are MIDI beats (16th-note = 1 unit in this transport grid).
  s_playhead = static_cast<uint8_t>(spp & 0x0F);
  s_audibleStep = s_playhead;
}

uint16_t transportExternalClockBpm() {
  return s_externalClockBpm;
}

bool transportExternalClockActive() {
  return s_externalClockDrive;
}
