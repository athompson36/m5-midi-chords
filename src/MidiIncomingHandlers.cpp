#include "m5chords_app/M5ChordsAppScreensPch.h"

#include <stdio.h>

namespace m5chords_app {

extern MidiEventHistory s_midiEventHistory;
extern MidiInState s_midiInState;
extern char s_midiDetectedChord[12];
extern uint32_t s_midiDetectedChordUntilMs;
extern char s_midiDetectedSuggest[12];
extern uint32_t s_midiDetectedSuggestUntilMs;
extern char s_midiSuggestTop3[3][12];
extern int16_t s_midiSuggestTop3Score[3];
extern uint32_t s_midiSuggestTop3UntilMs;
extern uint32_t s_midiSuggestStableUntilMs;
extern MidiSource s_midiSuggestLastSource;
extern uint8_t s_midiSuggestLastChannel;
extern uint32_t s_midiSuggestLastAtMs;
extern char s_playIngressInfo[44];
extern uint32_t s_playIngressInfoUntilMs;
extern bool s_playIngressInfoDirty;
extern uint32_t s_playIngressLastMusicalMs;
extern uint32_t s_playIngressInfoEventMs;

static uint8_t midiInputChannelForSource(MidiSource src) {
  switch (src) {
    case MidiSource::Usb:
      return g_settings.midiInChannelUsb;
    case MidiSource::Ble:
      return g_settings.midiInChannelBle;
    case MidiSource::Din:
      return g_settings.midiInChannelDin;
    default:
      return 0;
  }
}

static bool midiEventPassesChannelFilter(const MidiEvent& ev) {
  if (!midiEventIsChannelVoice(ev)) return true;
  const uint8_t filter = midiInputChannelForSource(ev.source);
  if (filter == 0) return true;  // OMNI
  return ev.channel == static_cast<uint8_t>(filter - 1U);
}

static const char* midiSourceCompactLabel(MidiSource src) {
  switch (src) {
    case MidiSource::Usb:
      return "USB";
    case MidiSource::Ble:
      return "BLE";
    case MidiSource::Din:
      return "DIN";
    default:
      return "?";
  }
}

static const char* midiEventCompactLabel(const MidiEvent& ev) {
  switch (ev.type) {
    case MidiEventType::NoteOn:
      return "On";
    case MidiEventType::NoteOff:
      return "Off";
    case MidiEventType::ControlChange:
      return "CC";
    case MidiEventType::Clock:
      return "Clk";
    case MidiEventType::Start:
      return "Start";
    case MidiEventType::Stop:
      return "Stop";
    case MidiEventType::Continue:
      return "Cont";
    case MidiEventType::SongPosition:
      return "SPP";
    case MidiEventType::SysEx:
      return "SX";
    default:
      return "Evt";
  }
}

static bool midiEventIsPlayMonitorMusical(const MidiEvent& ev) {
  switch (ev.type) {
    case MidiEventType::NoteOn:
    case MidiEventType::NoteOff:
    case MidiEventType::ControlChange:
    case MidiEventType::Start:
    case MidiEventType::Stop:
    case MidiEventType::Continue:
    case MidiEventType::SongPosition:
      return true;
    default:
      return false;
  }
}

static uint32_t playMonitorClockSuppressMs() {
  switch (g_settings.playInClockHold) {
    case 0:
      return 600U;
    case 2:
      return 2000U;
    default:
      return 1200U;
  }
}

static void playMonitorFormatAge(uint32_t ageMs, char* out, size_t outSize) {
  if (ageMs < 1000U) {
    snprintf(out, outSize, "+%ums", static_cast<unsigned>(ageMs));
    return;
  }
  const unsigned tenths = static_cast<unsigned>((ageMs + 50U) / 100U);
  const unsigned whole = tenths / 10U;
  const unsigned frac = tenths % 10U;
  snprintf(out, outSize, "+%u.%us", whole, frac);
}

static void midiMaybeReplyUniversalDeviceInquiry(const MidiEvent& ev) {
  if (ev.type != MidiEventType::SysEx) return;
  const uint8_t* p = midiIngressLastSysexPayload();
  const uint16_t n = ev.value14;
  if (n != 6) return;
  if (p[0] != 0xF0 || p[1] != 0x7E || p[3] != 0x06 || p[4] != 0x01 || p[5] != 0xF7) return;
  const uint8_t devicePath = p[2];
  // Standard Identity Reply: F0 7E <dev> 06 02 <mfr> <fam_lo> <fam_hi> <mdl_lo> <mdl_hi> <v1> <v2> <v3> <v4> F7
  uint8_t reply[15];
  reply[0]  = 0xF0;
  reply[1]  = 0x7E;
  reply[2]  = devicePath;
  reply[3]  = 0x06;
  reply[4]  = 0x02;
  reply[5]  = 0x7D;  // educational / non-commercial manufacturer ID
  reply[6]  = 0x01;  // family LSB
  reply[7]  = 0x00;  // family MSB
  reply[8]  = 0x01;  // model LSB
  reply[9]  = 0x00;  // model MSB
  reply[10] = 0x01;  // version byte 1
  reply[11] = 0x00;  // version byte 2
  reply[12] = 0x00;  // version byte 3
  reply[13] = 0x00;  // version byte 4
  reply[14] = 0xF7;
  midiSendSysEx(reply, sizeof(reply));
}

void playMonitorFormatLine(const MidiEvent& ev, char* out, size_t outSize, uint32_t nowMs) {
  const bool detailed = g_settings.playInMonitorMode != 0;
  const char* src = midiSourceCompactLabel(ev.source);
  const char* typ = midiEventCompactLabel(ev);
  const uint32_t ageMs = (nowMs >= s_playIngressInfoEventMs) ? (nowMs - s_playIngressInfoEventMs) : 0U;
  char ageBuf[12];
  playMonitorFormatAge(ageMs, ageBuf, sizeof(ageBuf));
  if (!detailed) {
    if (midiEventIsChannelVoice(ev)) {
      snprintf(out, outSize, "IN:%s ch%u %s", src, static_cast<unsigned>(ev.channel + 1U), typ);
    } else {
      snprintf(out, outSize, "IN:%s -- %s", src, typ);
    }
    return;
  }
  switch (ev.type) {
    case MidiEventType::NoteOn:
    case MidiEventType::NoteOff:
      snprintf(out, outSize, "IN:%s ch%u %s n%u v%u %s", src, static_cast<unsigned>(ev.channel + 1U), typ,
               static_cast<unsigned>(ev.data1), static_cast<unsigned>(ev.data2), ageBuf);
      return;
    case MidiEventType::ControlChange:
      snprintf(out, outSize, "IN:%s ch%u CC %u=%u %s", src, static_cast<unsigned>(ev.channel + 1U),
               static_cast<unsigned>(ev.data1), static_cast<unsigned>(ev.data2), ageBuf);
      return;
    case MidiEventType::SongPosition:
      snprintf(out, outSize, "IN:%s -- SPP %u %s", src, static_cast<unsigned>(ev.value14), ageBuf);
      return;
    case MidiEventType::SysEx: {
      const uint8_t* sx = midiIngressLastSysexPayload();
      const unsigned n = static_cast<unsigned>(ev.value14);
      if (n >= 3U) {
        snprintf(out, outSize, "IN:%s -- SX len=%u %02X%02X%02X… %s", src, n,
                 static_cast<unsigned>(sx[0]), static_cast<unsigned>(sx[1]),
                 static_cast<unsigned>(sx[2]), ageBuf);
      } else if (n >= 1U) {
        snprintf(out, outSize, "IN:%s -- SX len=%u %02X… %s", src, n,
                 static_cast<unsigned>(sx[0]), ageBuf);
      } else {
        snprintf(out, outSize, "IN:%s -- SX len=%u %s", src, n, ageBuf);
      }
      return;
    }
    default:
      break;
  }
  if (midiEventIsChannelVoice(ev)) {
    snprintf(out, outSize, "IN:%s ch%u %s %s", src, static_cast<unsigned>(ev.channel + 1U), typ, ageBuf);
  } else {
    snprintf(out, outSize, "IN:%s -- %s %s", src, typ, ageBuf);
  }
}

/// Mackie Control subset: master transport switches (typical note assignments 0x5B–0x5F).
static bool tryMackieTransportAction(const MidiEvent& ev, uint32_t nowMs) {
  if (g_settings.mackieControlPort == 0) return false;
  if (midiSourceToRoute(ev.source) != g_settings.mackieControlPort) return false;
  if (ev.type != MidiEventType::NoteOn || ev.data2 == 0) return false;
  switch (ev.data1) {
    case 0x5D:  // Stop
      panicForTrigger(PanicTrigger::ExternalTransportStop);
      transportOnExternalStop();
      return true;
    case 0x5E:  // Play
      transportOnExternalStart(nowMs);
      return true;
    case 0x5F:  // Record arm
      transportSetRecordArmed(!transportRecordArmed());
      return true;
    case 0x5B:  // Rew
    case 0x5C:  // FF
      return true;
    default:
      return false;
  }
}

void processIncomingMidiEvent(const MidiEvent& ev, uint32_t nowMs) {
  s_midiEventHistory.push(nowMs, ev);
  const bool isClock = ev.type == MidiEventType::Clock;
  const bool isMusical = midiEventIsPlayMonitorMusical(ev);
  if (isMusical) {
    s_playIngressLastMusicalMs = nowMs;
  }
  // Avoid monitor spam: never drive a full play-surface redraw from MIDI Clock (24 ticks/qn).
  // Also only mark dirty when the formatted line actually changes (dense CC/notes).
  const bool allowClockUpdate = !isClock || (nowMs - s_playIngressLastMusicalMs >= playMonitorClockSuppressMs());
  if (allowClockUpdate && !isClock) {
    char line[sizeof(s_playIngressInfo)];
    playMonitorFormatLine(ev, line, sizeof(line), nowMs);
    if (strcmp(line, s_playIngressInfo) != 0) {
      snprintf(s_playIngressInfo, sizeof(s_playIngressInfo), "%s", line);
      s_playIngressInfoEventMs = nowMs;
      s_playIngressInfoUntilMs = nowMs + 2200;
      s_playIngressInfoDirty = true;
    }
  }
  if (!midiEventIsRealtime(ev)) {
    if (!midiEventPassesChannelFilter(ev)) return;
    if (tryMackieTransportAction(ev, nowMs)) return;
    if (ev.type == MidiEventType::SysEx) {
      midiMaybeReplyUniversalDeviceInquiry(ev);
      uint8_t mmcCmd = 0;
      if (midiMmcParseRealtime(midiIngressLastSysexPayload(), ev.value14, &mmcCmd)) {
        const uint8_t sourceRoute = midiSourceToRoute(ev.source);
        const bool followClock = g_settings.clkFollow != 0;
        const bool receiveFromThisSource = g_settings.midiTransportReceive == sourceRoute;
        if (followClock && receiveFromThisSource) {
          switch (mmcCmd) {
            case 0x01:  // MMC Stop
              panicForTrigger(PanicTrigger::ExternalTransportStop);
              transportOnExternalStop();
              break;
            case 0x02:  // MMC Play
              transportOnExternalStart(nowMs);
              break;
            case 0x03:  // MMC Deferred Play (resume from current position)
              transportOnExternalContinue(nowMs);
              break;
            case 0x09:  // MMC Pause — treat like transport stop (notes off + idle)
              panicForTrigger(PanicTrigger::ExternalTransportStop);
              transportOnExternalStop();
              break;
            default:
              break;
          }
        }
      }
    }
    const uint8_t srcBit =
        (ev.source == MidiSource::Usb) ? 0x01U : ((ev.source == MidiSource::Ble) ? 0x02U : 0x04U);
    if ((g_settings.midiThruMask & srcBit) != 0U) {
      switch (ev.type) {
        case MidiEventType::NoteOn:
          if (ev.data2 == 0) {
            midiSendNoteOff(ev.channel, ev.data1);
          } else {
            midiSendNoteOn(ev.channel, ev.data1, ev.data2);
          }
          break;
        case MidiEventType::NoteOff:
          midiSendNoteOff(ev.channel, ev.data1);
          break;
        case MidiEventType::ControlChange:
          midiSendControlChange(ev.channel, ev.data1, ev.data2);
          break;
        case MidiEventType::SysEx:
          midiSendSysEx(midiIngressLastSysexPayload(), ev.value14);
          break;
        default:
          break;
      }
    }
    s_midiInState.onEvent(ev);
    if (ev.type == MidiEventType::NoteOn || ev.type == MidiEventType::NoteOff) {
      uint8_t active[16] = {};
      size_t n = 0;
      s_midiInState.collectActiveNotes(ev.source, ev.channel, active, sizeof(active), &n);
      char chord[12];
      if (midiDetectChordFromNotes(active, n, chord, sizeof(chord))) {
        snprintf(s_midiDetectedChord, sizeof(s_midiDetectedChord), "%s", chord);
        s_midiDetectedChordUntilMs = nowMs + 2000;
        MidiSuggestionItem ranked[ChordModel::kSurroundCount]{};
        const size_t rankedN = midiRankSuggestions(g_model, chord, ranked, ChordModel::kSurroundCount);
        if (rankedN > 0) {
          size_t chosenIdx = 0;
          bool usedStability = false;
          uint32_t stableWindowMs = 1400U;
          switch (g_settings.suggestStableWindow) {
            case 0:
              stableWindowMs = 600U;
              break;
            case 1:
              stableWindowMs = 1000U;
              break;
            case 2:
              stableWindowMs = 1400U;
              break;
            case 3:
              stableWindowMs = 2000U;
              break;
            default:
              stableWindowMs = 3000U;
              break;
          }
          int stableScoreGap = 18;
          switch (g_settings.suggestStableGap) {
            case 0:
              stableScoreGap = 8;
              break;
            case 1:
              stableScoreGap = 12;
              break;
            case 2:
              stableScoreGap = 18;
              break;
            case 3:
              stableScoreGap = 24;
              break;
            default:
              stableScoreGap = 32;
              break;
          }
          // Source/channel stability: when events stay on the same lane, keep prior best
          // unless a new candidate is materially stronger.
          const bool stableLane =
              (s_midiSuggestLastSource == ev.source) && (s_midiSuggestLastChannel == ev.channel) &&
              (s_midiSuggestLastAtMs > 0) && ((nowMs - s_midiSuggestLastAtMs) <= stableWindowMs);
          if (stableLane && s_midiDetectedSuggest[0] != '\0') {
            int prevIdx = -1;
            for (size_t i = 0; i < rankedN; ++i) {
              if (strcmp(ranked[i].name, s_midiDetectedSuggest) == 0) {
                prevIdx = static_cast<int>(i);
                break;
              }
            }
            if (prevIdx >= 0) {
              const int topScore = static_cast<int>(ranked[0].score);
              const int prevScore = static_cast<int>(ranked[prevIdx].score);
              if (topScore - prevScore < stableScoreGap) {
                chosenIdx = static_cast<size_t>(prevIdx);
                usedStability = true;
              }
            }
          }
          snprintf(s_midiDetectedSuggest, sizeof(s_midiDetectedSuggest), "%s", ranked[chosenIdx].name);
          s_midiDetectedSuggestUntilMs = nowMs + 2000;
          s_midiSuggestStableUntilMs = usedStability ? (nowMs + 1800U) : 0U;
          s_midiSuggestLastSource = ev.source;
          s_midiSuggestLastChannel = ev.channel;
          s_midiSuggestLastAtMs = nowMs;
          for (int i = 0; i < 3; ++i) {
            if (static_cast<size_t>(i) < rankedN) {
              snprintf(s_midiSuggestTop3[i], sizeof(s_midiSuggestTop3[i]), "%s", ranked[i].name);
              s_midiSuggestTop3Score[i] = ranked[i].score;
            } else {
              s_midiSuggestTop3[i][0] = '\0';
              s_midiSuggestTop3Score[i] = 0;
            }
          }
          s_midiSuggestTop3UntilMs = nowMs + 4000;
        } else {
          s_midiDetectedSuggest[0] = '\0';
          s_midiDetectedSuggestUntilMs = 0;
          for (int i = 0; i < 3; ++i) {
            s_midiSuggestTop3[i][0] = '\0';
            s_midiSuggestTop3Score[i] = 0;
          }
          s_midiSuggestTop3UntilMs = 0;
          s_midiSuggestStableUntilMs = 0;
        }
      } else if (n == 0) {
        s_midiDetectedChord[0] = '\0';
        s_midiDetectedChordUntilMs = 0;
        s_midiDetectedSuggest[0] = '\0';
        s_midiDetectedSuggestUntilMs = 0;
        s_midiSuggestLastAtMs = 0;
        for (int i = 0; i < 3; ++i) {
          s_midiSuggestTop3[i][0] = '\0';
          s_midiSuggestTop3Score[i] = 0;
        }
        s_midiSuggestTop3UntilMs = 0;
        s_midiSuggestStableUntilMs = 0;
      }
    }
    return;
  }

  const uint8_t sourceRoute = midiSourceToRoute(ev.source);
  const bool followClock = g_settings.clkFollow != 0;
  const bool receiveFromThisSource = g_settings.midiTransportReceive == sourceRoute;
  switch (ev.type) {
    case MidiEventType::Start:
      if (followClock && receiveFromThisSource) transportOnExternalStart(nowMs);
      break;
    case MidiEventType::Continue:
      if (followClock && receiveFromThisSource) transportOnExternalContinue(nowMs);
      break;
    case MidiEventType::Stop:
      if (followClock && receiveFromThisSource) {
        panicForTrigger(PanicTrigger::ExternalTransportStop);
        transportOnExternalStop();
      }
      break;
    case MidiEventType::SongPosition:
      if (followClock && receiveFromThisSource) transportOnExternalSongPosition(ev.value14);
      break;
    case MidiEventType::Clock:
      // Arm from clock alone when the host never sends 0xFA/0xFB (see Transport). After MIDI Stop
      // or local Stop, do not re-arm from 0xF8 alone — hosts often keep sending clock while stopped.
      if (followClock && receiveFromThisSource) {
        if (!transportExternalClockActive()) {
          if (transportExternalMayArmFromClockStream()) {
            transportOnExternalContinue(nowMs);
          }
        }
        if (transportExternalClockActive()) {
          transportOnExternalClockTick(nowMs);
        }
      }
      break;
    default:
      break;
  }
}

void clearMidiDebugDiagnosticsState() {
  s_midiEventHistory.clear();
  s_midiInState.reset();
  bleMidiResetRxBytes();
  dinMidiResetRxBytes();
  midiOutResetTxStats();
  midiIngressResetQueueDropTotals();
  midiDebugResetUiDiagnosticsState();
  s_midiDetectedChord[0] = '\0';
  s_midiDetectedChordUntilMs = 0;
  s_midiDetectedSuggest[0] = '\0';
  s_midiDetectedSuggestUntilMs = 0;
  s_midiSuggestLastAtMs = 0;
  for (int i = 0; i < 3; ++i) {
    s_midiSuggestTop3[i][0] = '\0';
    s_midiSuggestTop3Score[i] = 0;
  }
  s_midiSuggestTop3UntilMs = 0;
  s_midiSuggestStableUntilMs = 0;
  snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "MIDI debug data cleared");
}

}  // namespace m5chords_app
