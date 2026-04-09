#include "MidiIngress.h"

#if defined(ARDUINO)
#include <Arduino.h>
#endif

namespace {

constexpr size_t kIngressQueueCap = 512;
uint8_t g_bleQ[kIngressQueueCap];
uint8_t g_dinQ[kIngressQueueCap];
size_t g_bleHead = 0;
size_t g_bleTail = 0;
size_t g_bleCount = 0;
size_t g_dinHead = 0;
size_t g_dinTail = 0;
size_t g_dinCount = 0;
uint32_t g_bleQueueDropTotal = 0;
uint32_t g_dinQueueDropTotal = 0;

void queuePushBytes(uint8_t* q, size_t cap, size_t* head, size_t* tail, size_t* count, const uint8_t* bytes,
                    size_t len, uint32_t* dropCounter) {
  if (!bytes || len == 0) return;
  for (size_t i = 0; i < len; ++i) {
    if (*count >= cap) {
      // Drop oldest when full to keep latest stream bytes.
      *head = (*head + 1) % cap;
      (*count)--;
      if (dropCounter) {
        (*dropCounter)++;
      }
    }
    q[*tail] = bytes[i];
    *tail = (*tail + 1) % cap;
    (*count)++;
  }
}

bool queuePopByte(uint8_t* q, size_t cap, size_t* head, size_t* count, uint8_t* out) {
  if (!out || *count == 0) return false;
  *out = q[*head];
  *head = (*head + 1) % cap;
  (*count)--;
  return true;
}

bool pollQueuedSource(MidiIngressParser& parser, MidiSource src, uint8_t* q, size_t cap, size_t* head,
                      size_t* count, MidiEvent* out) {
  uint8_t b = 0;
  while (queuePopByte(q, cap, head, count, &b)) {
    if (parser.feedByte(b, src, out)) {
      return true;
    }
  }
  return false;
}

uint8_t midiDataByteCount(uint8_t status) {
  const uint8_t hi = status & 0xF0;
  switch (hi) {
    case 0xC0:
    case 0xD0:
      return 1;
    case 0x80:
    case 0x90:
    case 0xA0:
    case 0xB0:
    case 0xE0:
      return 2;
    default:
      break;
  }
  if (status == 0xF2) return 2;  // Song Position
  return 0;
}

MidiEventType midiTypeFromStatus(uint8_t status, uint8_t d1, uint8_t d2) {
  switch (status & 0xF0) {
    case 0x80:
      return MidiEventType::NoteOff;
    case 0x90:
      return d2 == 0 ? MidiEventType::NoteOff : MidiEventType::NoteOn;
    case 0xA0:
      return MidiEventType::PolyAftertouch;
    case 0xB0:
      return MidiEventType::ControlChange;
    case 0xC0:
      return MidiEventType::ProgramChange;
    case 0xD0:
      return MidiEventType::ChannelAftertouch;
    case 0xE0:
      return MidiEventType::PitchBend;
    default:
      break;
  }
  if (status == 0xF8) return MidiEventType::Clock;
  if (status == 0xFA) return MidiEventType::Start;
  if (status == 0xFB) return MidiEventType::Continue;
  if (status == 0xFC) return MidiEventType::Stop;
  if (status == 0xF2) return MidiEventType::SongPosition;
  return MidiEventType::None;
}

}  // namespace

extern "C" size_t __attribute__((weak)) m5ChordBleMidiRead(uint8_t* dst, size_t cap) {
  (void)dst;
  (void)cap;
  return 0;
}

extern "C" size_t __attribute__((weak)) m5ChordDinMidiRead(uint8_t* dst, size_t cap) {
  (void)dst;
  (void)cap;
  return 0;
}

void MidiIngressParser::reset() {
  runningStatus_ = 0;
  pendingStatus_ = 0;
  dataCount_ = 0;
  dataBuf_[0] = 0;
  dataBuf_[1] = 0;
}

bool MidiIngressParser::feedByte(uint8_t b, MidiSource src, MidiEvent* out) {
  if (!out) return false;
  out->type = MidiEventType::None;
  out->source = src;
  out->channel = 0;
  out->data1 = 0;
  out->data2 = 0;
  out->value14 = 0;

  // Single-byte realtime messages can appear anywhere.
  if (b >= 0xF8) {
    out->type = midiTypeFromStatus(b, 0, 0);
    return out->type != MidiEventType::None;
  }

  // Status byte.
  if (b & 0x80) {
    // Ignore SysEx and other system-common for now except SPP.
    if (b >= 0xF0) {
      if (b == 0xF2) {
        pendingStatus_ = b;
        dataCount_ = 0;
      } else {
        pendingStatus_ = 0;
        dataCount_ = 0;
      }
      return false;
    }
    runningStatus_ = b;
    pendingStatus_ = b;
    dataCount_ = 0;
    return false;
  }

  // Data byte.
  if (pendingStatus_ == 0) {
    if (runningStatus_ == 0) return false;
    pendingStatus_ = runningStatus_;
    dataCount_ = 0;
  }

  const uint8_t need = midiDataByteCount(pendingStatus_);
  if (need == 0) return false;

  if (dataCount_ < 2) {
    dataBuf_[dataCount_++] = static_cast<uint8_t>(b & 0x7F);
  }
  if (dataCount_ < need) return false;

  const uint8_t status = pendingStatus_;
  const uint8_t d1 = dataBuf_[0];
  const uint8_t d2 = dataBuf_[1];
  dataCount_ = 0;
  if (status >= 0xF0) {
    pendingStatus_ = 0;
  } else {
    pendingStatus_ = runningStatus_;
  }

  out->type = midiTypeFromStatus(status, d1, d2);
  if (out->type == MidiEventType::None) return false;
  out->data1 = d1;
  out->data2 = d2;
  if ((status & 0xF0) != 0xF0) {
    out->channel = static_cast<uint8_t>(status & 0x0F);
  }
  if (out->type == MidiEventType::PitchBend || out->type == MidiEventType::SongPosition) {
    out->value14 = static_cast<uint16_t>(d1 | (static_cast<uint16_t>(d2) << 7));
  }
  return true;
}

bool midiEventIsChannelVoice(const MidiEvent& ev) {
  return ev.type == MidiEventType::NoteOff || ev.type == MidiEventType::NoteOn ||
         ev.type == MidiEventType::PolyAftertouch || ev.type == MidiEventType::ControlChange ||
         ev.type == MidiEventType::ProgramChange || ev.type == MidiEventType::ChannelAftertouch ||
         ev.type == MidiEventType::PitchBend;
}

bool midiEventIsRealtime(const MidiEvent& ev) {
  return ev.type == MidiEventType::Clock || ev.type == MidiEventType::Start ||
         ev.type == MidiEventType::Continue || ev.type == MidiEventType::Stop ||
         ev.type == MidiEventType::SongPosition;
}

uint8_t midiSourceToRoute(MidiSource src) {
  switch (src) {
    case MidiSource::Usb:
      return 1;
    case MidiSource::Ble:
      return 2;
    case MidiSource::Din:
      return 3;
    default:
      return 0;
  }
}

bool midiIngressPollUsb(MidiIngressParser& parser, MidiEvent* out) {
#if defined(ARDUINO)
  while (Serial.available() > 0) {
    const int rb = Serial.read();
    if (rb < 0) break;
    if (parser.feedByte(static_cast<uint8_t>(rb), MidiSource::Usb, out)) {
      return true;
    }
  }
#else
  (void)parser;
  (void)out;
#endif
  return false;
}

bool midiIngressPollBle(MidiIngressParser& parser, MidiEvent* out) {
  uint8_t buf[64];
  const size_t n = m5ChordBleMidiRead(buf, sizeof(buf));
  if (n > 0) {
    midiIngressEnqueueBleBytes(buf, n);
  }
  return pollQueuedSource(parser, MidiSource::Ble, g_bleQ, kIngressQueueCap, &g_bleHead, &g_bleCount, out);
}

bool midiIngressPollDin(MidiIngressParser& parser, MidiEvent* out) {
  uint8_t buf[64];
  const size_t n = m5ChordDinMidiRead(buf, sizeof(buf));
  if (n > 0) {
    midiIngressEnqueueDinBytes(buf, n);
  }
  return pollQueuedSource(parser, MidiSource::Din, g_dinQ, kIngressQueueCap, &g_dinHead, &g_dinCount, out);
}

void midiIngressEnqueueBleBytes(const uint8_t* bytes, size_t len) {
  queuePushBytes(g_bleQ, kIngressQueueCap, &g_bleHead, &g_bleTail, &g_bleCount, bytes, len,
                 &g_bleQueueDropTotal);
}

void midiIngressEnqueueDinBytes(const uint8_t* bytes, size_t len) {
  queuePushBytes(g_dinQ, kIngressQueueCap, &g_dinHead, &g_dinTail, &g_dinCount, bytes, len,
                 &g_dinQueueDropTotal);
}

uint32_t midiIngressBleQueueDropTotal() { return g_bleQueueDropTotal; }

uint32_t midiIngressDinQueueDropTotal() { return g_dinQueueDropTotal; }

void midiIngressResetQueueDropTotals() {
  g_bleQueueDropTotal = 0;
  g_dinQueueDropTotal = 0;
}

bool midiIngressFeedBytes(MidiIngressParser& parser, MidiSource src, const uint8_t* bytes, size_t len,
                          MidiEvent* out) {
  if (!bytes || !out) return false;
  for (size_t i = 0; i < len; ++i) {
    if (parser.feedByte(bytes[i], src, out)) {
      return true;
    }
  }
  return false;
}

bool midiIngressFeedUsbBytes(MidiIngressParser& parser, const uint8_t* bytes, size_t len, MidiEvent* out) {
  return midiIngressFeedBytes(parser, MidiSource::Usb, bytes, len, out);
}
