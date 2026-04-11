#include "m5chords_app/M5ChordsAppScreensPch.h"

namespace m5chords_app {

uint8_t applyOutputVelocityCurve(uint8_t raw) {
  uint8_t v = raw;
  if (v > 127U) v = 127U;
  switch (g_settings.velocityCurve) {
    case 1: {  // soft
      const uint16_t curved = (static_cast<uint16_t>(v) * static_cast<uint16_t>(v)) / 127U;
      return static_cast<uint8_t>(curved == 0U && v > 0U ? 1U : curved);
    }
    case 2: {  // hard
      const uint16_t inv = static_cast<uint16_t>(127U - v);
      const uint16_t curved = 127U - ((inv * inv) / 127U);
      return static_cast<uint8_t>(curved);
    }
    default:
      return v;
  }
}

struct SeqArpLaneRuntime {
  bool active = false;
  uint8_t channel0 = 0;
  uint8_t noteCount = 0;
  uint8_t notes[6] = {};
  uint8_t orderCount = 0;
  uint8_t orderIdx[16] = {};
  uint8_t emitPos = 0;
  uint32_t nextMs = 0;
  uint32_t endMs = 0;
  uint32_t intervalMs = 1;
  uint8_t gatePct = 80;
  bool noteActive = false;
  uint32_t noteOffMs = 0;
};

static SeqArpLaneRuntime s_seqArpRt[3];

static void seqArpStopLane(uint8_t lane, bool silence) {
  if (lane > 2) return;
  SeqArpLaneRuntime& rt = s_seqArpRt[lane];
  if (silence) {
    midiSilenceLastChord(rt.channel0 & 0x0F);
  }
  rt.active = false;
  rt.noteActive = false;
  rt.orderCount = 0;
  rt.emitPos = 0;
}

void seqArpStopAll(bool silence) {
  for (uint8_t lane = 0; lane < 3; ++lane) {
    seqArpStopLane(lane, silence);
  }
}

static uint8_t seqArpBuildOrder(uint8_t pattern, uint8_t noteCount, uint8_t pulses, uint8_t out[16]) {
  if (noteCount == 0 || pulses == 0) return 0;
  if (pulses > 16) pulses = 16;
  pattern &= 0x03U;
  for (uint8_t i = 0; i < pulses; ++i) {
    if (pattern == 0) {  // up
      out[i] = static_cast<uint8_t>(i % noteCount);
    } else if (pattern == 1) {  // down
      out[i] = static_cast<uint8_t>((noteCount - 1U) - (i % noteCount));
    } else if (pattern == 2) {  // up/down bounce
      if (noteCount <= 1) {
        out[i] = 0;
      } else {
        const uint8_t span = static_cast<uint8_t>((noteCount * 2U) - 2U);
        const uint8_t p = static_cast<uint8_t>(i % span);
        out[i] = (p < noteCount) ? p : static_cast<uint8_t>(span - p);
      }
    } else {  // random
      out[i] = static_cast<uint8_t>(rand() % noteCount);
    }
  }
  return pulses;
}

void seqArpProcessDue(uint32_t nowMs) {
  for (uint8_t lane = 0; lane < 3; ++lane) {
    SeqArpLaneRuntime& rt = s_seqArpRt[lane];
    if (!rt.active) continue;
    if (rt.noteActive && static_cast<int32_t>(nowMs - rt.noteOffMs) >= 0) {
      midiSilenceLastChord(rt.channel0 & 0x0F);
      rt.noteActive = false;
    }
    while (rt.active && static_cast<int32_t>(nowMs - rt.nextMs) >= 0) {
      if (rt.emitPos >= rt.orderCount || static_cast<int32_t>(rt.nextMs - rt.endMs) >= 0) {
        if (rt.noteActive) {
          midiSilenceLastChord(rt.channel0 & 0x0F);
          rt.noteActive = false;
        }
        rt.active = false;  // Truncate policy: never spill into next step.
        break;
      }
      const uint8_t idx = rt.orderIdx[rt.emitPos];
      if (idx >= rt.noteCount) {
        if (rt.noteActive) {
          midiSilenceLastChord(rt.channel0 & 0x0F);
          rt.noteActive = false;
        }
        rt.active = false;
        break;
      }
      midiSilenceLastChord(rt.channel0 & 0x0F);
      midiSendNoteOnTracked(rt.channel0 & 0x0F, rt.notes[idx], applyOutputVelocityCurve(g_settings.outputVelocity));
      rt.noteActive = true;
      uint32_t gateMs = (rt.intervalMs * static_cast<uint32_t>(rt.gatePct)) / 100U;
      if (gateMs == 0) gateMs = 1;
      rt.noteOffMs = rt.nextMs + gateMs;
      rt.emitPos++;
      rt.nextMs += rt.intervalMs;
    }
  }
}

void transportEmitSequencerStepMidi(uint8_t step) {
  uint8_t selectedLane = g_seqLane;
  if (selectedLane > 2) selectedLane = 0;
  const uint32_t stepWindowMs = transportStepWindowMs();
  int8_t primaryLive = -1;
  for (uint8_t lane = 0; lane < 3; ++lane) {
    // End prior step scheduling for this lane before evaluating the new step.
    s_seqArpRt[lane].active = false;
    const uint8_t cell = g_seqPattern[lane][step & 0x0F];
    uint8_t midiCh = g_seqMidiCh[lane];
    if (midiCh < 1 || midiCh > 16) {
      midiCh = 1;
    }
    const uint8_t ch0 = static_cast<uint8_t>(midiCh - 1U);

    if (cell == kSeqTie) {
      // Tie policy: no new attack; keep previously sounding notes (including loop wrap).
      continue;
    }
    if (cell == kSeqRest) {
      midiSilenceLastChord(ch0);
      continue;
    }

    if (cell == kSeqSurprise || cell < ChordModel::kSurroundCount) {
      const uint8_t prob = g_seqExtras.stepProb[lane][step & 0x0F];
      if (!seqRollStepFires(prob)) {
        midiSilenceLastChord(ch0);
        continue;
      }
    }

    uint8_t vcap = g_seqExtras.stepVoicing[lane][step & 0x0F];
    if (vcap < 1 || vcap > 4) {
      vcap = g_chordVoicing;
    }
    if (vcap < 1) vcap = 1;
    if (vcap > 4) vcap = 4;
    const uint8_t vel = applyOutputVelocityCurve(g_settings.outputVelocity);
    const bool stepArp = g_seqExtras.arpEnabled[lane][step & 0x0F] != 0U;

    if (stepArp && (cell == kSeqSurprise || cell < ChordModel::kSurroundCount)) {
      int8_t rel[6] = {};
      uint8_t n = 0;
      int rootMidi = 48;
      if (cell < ChordModel::kSurroundCount) {
        g_model.getSurroundSpell(static_cast<int>(cell), rel, &n);
        rootMidi = 48 + g_model.surroundChordRootPc(static_cast<int>(cell));
      } else {
        const int poolIdx = g_model.surprisePeekIndex();
        g_model.nextSurprise();
        g_model.getSurpriseSpell(poolIdx, rel, &n);
        rootMidi = 48 + g_model.surpriseChordRootPc(poolIdx);
      }
      int8_t ordered[6] = {};
      uint8_t oc = 0;
      arpeggiatorOrderedIntervals(g_settings.arpeggiatorMode, rel, n, vcap, ordered, &oc);
      if (oc == 0) {
        midiSilenceLastChord(ch0);
        continue;
      }
      SeqArpLaneRuntime& rt = s_seqArpRt[lane];
      rt.channel0 = ch0;
      rt.noteCount = oc;
      for (uint8_t i = 0; i < oc; ++i) {
        const int octSpread = static_cast<int>(g_seqExtras.arpOctRange[lane][step & 0x0F] > 2
                                                   ? 2
                                                   : g_seqExtras.arpOctRange[lane][step & 0x0F]);
        int octAdd = 0;
        if (octSpread > 0) {
          const int span = octSpread * 2 + 1;
          octAdd = (rand() % span) - octSpread;
        }
        int note = rootMidi + static_cast<int>(ordered[i]) + static_cast<int>(g_settings.transposeSemitones) +
                   (12 * octAdd);
        if (note < 0) note = 0;
        if (note > 127) note = 127;
        rt.notes[i] = static_cast<uint8_t>(note);
      }
      const uint8_t pulses = static_cast<uint8_t>(1U << (g_seqExtras.arpClockDiv[lane][step & 0x0F] & 0x03U));
      rt.orderCount =
          seqArpBuildOrder(g_seqExtras.arpPattern[lane][step & 0x0F], rt.noteCount, pulses, rt.orderIdx);
      if (rt.orderCount == 0) {
        midiSilenceLastChord(ch0);
        continue;
      }
      rt.emitPos = 0;
      rt.nextMs = millis();
      rt.endMs = rt.nextMs + (stepWindowMs == 0 ? 1U : stepWindowMs);
      rt.intervalMs = stepWindowMs / rt.orderCount;
      if (rt.intervalMs == 0) rt.intervalMs = 1;
      rt.gatePct = g_seqExtras.arpGatePct[lane][step & 0x0F];
      if (rt.gatePct < 10) rt.gatePct = 10;
      if (rt.gatePct > 100) rt.gatePct = 100;
      rt.noteActive = false;
      rt.noteOffMs = 0;
      rt.active = true;
      if (primaryLive < 0 && lane == selectedLane && cell < ChordModel::kSurroundCount) {
        primaryLive = static_cast<int8_t>(cell);
      }
      continue;
    }

    if (cell < ChordModel::kSurroundCount) {
      midiPlaySurroundPad(g_model, static_cast<int>(cell), ch0, vel, vcap, g_settings.arpeggiatorMode,
                          g_settings.transposeSemitones);
      if (primaryLive < 0 && lane == selectedLane) {
        primaryLive = static_cast<int8_t>(cell);
      }
      continue;
    }

    if (cell == kSeqSurprise) {
      const int poolIdx = g_model.surprisePeekIndex();
      g_model.nextSurprise();
      midiPlaySurprisePad(g_model, poolIdx, ch0, vel, vcap, g_settings.arpeggiatorMode,
                          g_settings.transposeSemitones);
      continue;
    }
  }

  transportSetLiveChord(primaryLive);
}

}  // namespace m5chords_app
