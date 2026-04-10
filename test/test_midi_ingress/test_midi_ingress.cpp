#include <unity.h>

#include "MidiIngress.h"

namespace {

uint8_t g_bleProviderBuf[32];
size_t g_bleProviderLen = 0;
size_t g_bleProviderPos = 0;
uint8_t g_dinProviderBuf[32];
size_t g_dinProviderLen = 0;
size_t g_dinProviderPos = 0;
uint8_t g_usbProviderBuf[32];
size_t g_usbProviderLen = 0;
size_t g_usbProviderPos = 0;

}  // namespace

extern "C" size_t m5ChordBleMidiRead(uint8_t* dst, size_t cap) {
  if (!dst || cap == 0 || g_bleProviderPos >= g_bleProviderLen) return 0;
  size_t n = g_bleProviderLen - g_bleProviderPos;
  if (n > cap) n = cap;
  for (size_t i = 0; i < n; ++i) {
    dst[i] = g_bleProviderBuf[g_bleProviderPos + i];
  }
  g_bleProviderPos += n;
  return n;
}

extern "C" size_t m5ChordDinMidiRead(uint8_t* dst, size_t cap) {
  if (!dst || cap == 0 || g_dinProviderPos >= g_dinProviderLen) return 0;
  size_t n = g_dinProviderLen - g_dinProviderPos;
  if (n > cap) n = cap;
  for (size_t i = 0; i < n; ++i) {
    dst[i] = g_dinProviderBuf[g_dinProviderPos + i];
  }
  g_dinProviderPos += n;
  return n;
}

extern "C" size_t m5ChordUsbMidiRead(uint8_t* dst, size_t cap) {
  if (!dst || cap == 0 || g_usbProviderPos >= g_usbProviderLen) return 0;
  size_t n = g_usbProviderLen - g_usbProviderPos;
  if (n > cap) n = cap;
  for (size_t i = 0; i < n; ++i) {
    dst[i] = g_usbProviderBuf[g_usbProviderPos + i];
  }
  g_usbProviderPos += n;
  return n;
}

static void bleProviderSet(const uint8_t* bytes, size_t len) {
  g_bleProviderLen = (len > sizeof(g_bleProviderBuf)) ? sizeof(g_bleProviderBuf) : len;
  g_bleProviderPos = 0;
  for (size_t i = 0; i < g_bleProviderLen; ++i) {
    g_bleProviderBuf[i] = bytes[i];
  }
}

static void dinProviderSet(const uint8_t* bytes, size_t len) {
  g_dinProviderLen = (len > sizeof(g_dinProviderBuf)) ? sizeof(g_dinProviderBuf) : len;
  g_dinProviderPos = 0;
  for (size_t i = 0; i < g_dinProviderLen; ++i) {
    g_dinProviderBuf[i] = bytes[i];
  }
}

static void usbProviderSet(const uint8_t* bytes, size_t len) {
  g_usbProviderLen = (len > sizeof(g_usbProviderBuf)) ? sizeof(g_usbProviderBuf) : len;
  g_usbProviderPos = 0;
  for (size_t i = 0; i < g_usbProviderLen; ++i) {
    g_usbProviderBuf[i] = bytes[i];
  }
}

void test_running_status_note_stream() {
  MidiIngressParser p;
  MidiEvent ev{};
  const uint8_t bytes[] = {0x90, 60, 100, 62, 110};
  TEST_ASSERT_TRUE(midiIngressFeedUsbBytes(p, bytes, 3, &ev));
  TEST_ASSERT_EQUAL(MidiEventType::NoteOn, ev.type);
  TEST_ASSERT_EQUAL_UINT8(0, ev.channel);
  TEST_ASSERT_EQUAL_UINT8(60, ev.data1);
  TEST_ASSERT_EQUAL_UINT8(100, ev.data2);

  TEST_ASSERT_TRUE(midiIngressFeedUsbBytes(p, bytes + 3, 2, &ev));
  TEST_ASSERT_EQUAL(MidiEventType::NoteOn, ev.type);
  TEST_ASSERT_EQUAL_UINT8(62, ev.data1);
  TEST_ASSERT_EQUAL_UINT8(110, ev.data2);
}

void test_realtime_interleaved_with_note_message() {
  MidiIngressParser p;
  MidiEvent ev{};
  const uint8_t bytes[] = {0x90, 60, 0xF8, 100};

  TEST_ASSERT_TRUE(midiIngressFeedUsbBytes(p, bytes, 3, &ev));
  TEST_ASSERT_EQUAL(MidiEventType::Clock, ev.type);
  TEST_ASSERT_TRUE(midiEventIsRealtime(ev));

  TEST_ASSERT_TRUE(midiIngressFeedUsbBytes(p, bytes + 3, 1, &ev));
  TEST_ASSERT_EQUAL(MidiEventType::NoteOn, ev.type);
  TEST_ASSERT_EQUAL_UINT8(60, ev.data1);
  TEST_ASSERT_EQUAL_UINT8(100, ev.data2);
}

void test_song_position_and_route_mapping() {
  MidiIngressParser p;
  MidiEvent ev{};
  const uint8_t spp[] = {0xF2, 0x01, 0x02};
  TEST_ASSERT_TRUE(midiIngressFeedUsbBytes(p, spp, sizeof(spp), &ev));
  TEST_ASSERT_EQUAL(MidiEventType::SongPosition, ev.type);
  TEST_ASSERT_EQUAL_UINT16(257, ev.value14);
  TEST_ASSERT_EQUAL_UINT8(1, midiSourceToRoute(MidiSource::Usb));
  TEST_ASSERT_EQUAL_UINT8(2, midiSourceToRoute(MidiSource::Ble));
  TEST_ASSERT_EQUAL_UINT8(3, midiSourceToRoute(MidiSource::Din));
}

void test_feed_bytes_with_explicit_source() {
  MidiIngressParser p;
  MidiEvent ev{};
  const uint8_t bytes[] = {0x90, 60, 100};
  TEST_ASSERT_TRUE(midiIngressFeedBytes(p, MidiSource::Ble, bytes, sizeof(bytes), &ev));
  TEST_ASSERT_EQUAL(MidiEventType::NoteOn, ev.type);
  TEST_ASSERT_EQUAL(MidiSource::Ble, ev.source);
}

void test_ble_queue_poll_produces_event() {
  MidiIngressParser p;
  MidiEvent ev{};
  const uint8_t bytes[] = {0x90, 65, 127};
  midiIngressEnqueueBleBytes(bytes, sizeof(bytes));
  TEST_ASSERT_TRUE(midiIngressPollBle(p, &ev));
  TEST_ASSERT_EQUAL(MidiSource::Ble, ev.source);
  TEST_ASSERT_EQUAL(MidiEventType::NoteOn, ev.type);
  TEST_ASSERT_EQUAL_UINT8(65, ev.data1);
}

void test_din_queue_poll_produces_event() {
  MidiIngressParser p;
  MidiEvent ev{};
  const uint8_t bytes[] = {0x80, 65, 0};
  midiIngressEnqueueDinBytes(bytes, sizeof(bytes));
  TEST_ASSERT_TRUE(midiIngressPollDin(p, &ev));
  TEST_ASSERT_EQUAL(MidiSource::Din, ev.source);
  TEST_ASSERT_EQUAL(MidiEventType::NoteOff, ev.type);
  TEST_ASSERT_EQUAL_UINT8(65, ev.data1);
}

void test_ble_provider_hook_feeds_poller() {
  MidiIngressParser p;
  MidiEvent ev{};
  const uint8_t bytes[] = {0x90, 60, 101};
  bleProviderSet(bytes, sizeof(bytes));
  TEST_ASSERT_TRUE(midiIngressPollBle(p, &ev));
  TEST_ASSERT_EQUAL(MidiSource::Ble, ev.source);
  TEST_ASSERT_EQUAL(MidiEventType::NoteOn, ev.type);
  TEST_ASSERT_EQUAL_UINT8(60, ev.data1);
  TEST_ASSERT_EQUAL_UINT8(101, ev.data2);
}

void test_din_provider_hook_feeds_poller() {
  MidiIngressParser p;
  MidiEvent ev{};
  const uint8_t bytes[] = {0x90, 62, 112};
  dinProviderSet(bytes, sizeof(bytes));
  TEST_ASSERT_TRUE(midiIngressPollDin(p, &ev));
  TEST_ASSERT_EQUAL(MidiSource::Din, ev.source);
  TEST_ASSERT_EQUAL(MidiEventType::NoteOn, ev.type);
  TEST_ASSERT_EQUAL_UINT8(62, ev.data1);
  TEST_ASSERT_EQUAL_UINT8(112, ev.data2);
}

void test_din_queue_overflow_counts_drops() {
  midiIngressResetQueueDropTotals();
  uint8_t bytes[700];
  for (size_t i = 0; i < sizeof(bytes); ++i) {
    bytes[i] = static_cast<uint8_t>(i & 0x7F);
  }
  midiIngressEnqueueDinBytes(bytes, sizeof(bytes));
  TEST_ASSERT_TRUE(midiIngressDinQueueDropTotal() > 0);
}

void test_usb_provider_hook_feeds_poller() {
  MidiIngressParser p;
  MidiEvent ev{};
  const uint8_t bytes[] = {0x90, 67, 120};
  usbProviderSet(bytes, sizeof(bytes));
  TEST_ASSERT_TRUE(midiIngressPollUsb(p, &ev));
  TEST_ASSERT_EQUAL(MidiSource::Usb, ev.source);
  TEST_ASSERT_EQUAL(MidiEventType::NoteOn, ev.type);
  TEST_ASSERT_EQUAL_UINT8(67, ev.data1);
  TEST_ASSERT_EQUAL_UINT8(120, ev.data2);
}

void test_sysex_complete_message() {
  MidiIngressParser p;
  MidiEvent ev{};
  const uint8_t bytes[] = {0xF0, 0x01, 0x02, 0x03, 0xF7};
  TEST_ASSERT_TRUE(midiIngressFeedUsbBytes(p, bytes, sizeof(bytes), &ev));
  TEST_ASSERT_EQUAL(MidiEventType::SysEx, ev.type);
  TEST_ASSERT_EQUAL_UINT16(5, ev.value14);
  const uint8_t* sx = midiIngressLastSysexPayload();
  TEST_ASSERT_EQUAL_UINT8(0xF0, sx[0]);
  TEST_ASSERT_EQUAL_UINT8(0x03, sx[3]);
  TEST_ASSERT_EQUAL_UINT8(0xF7, sx[4]);
}

void test_sysex_realtime_interleaved() {
  MidiIngressParser p;
  MidiEvent ev{};
  const uint8_t bytes[] = {0xF0, 0x01, 0xF8, 0x02, 0xF7};
  TEST_ASSERT_TRUE(midiIngressFeedUsbBytes(p, bytes, 3, &ev));
  TEST_ASSERT_EQUAL(MidiEventType::Clock, ev.type);
  TEST_ASSERT_TRUE(midiIngressFeedUsbBytes(p, bytes + 3, 2, &ev));
  TEST_ASSERT_EQUAL(MidiEventType::SysEx, ev.type);
  TEST_ASSERT_EQUAL_UINT16(4, ev.value14);
  const uint8_t* sx = midiIngressLastSysexPayload();
  TEST_ASSERT_EQUAL_UINT8(0xF0, sx[0]);
  TEST_ASSERT_EQUAL_UINT8(0x01, sx[1]);
  TEST_ASSERT_EQUAL_UINT8(0x02, sx[2]);
  TEST_ASSERT_EQUAL_UINT8(0xF7, sx[3]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_running_status_note_stream);
  RUN_TEST(test_realtime_interleaved_with_note_message);
  RUN_TEST(test_song_position_and_route_mapping);
  RUN_TEST(test_feed_bytes_with_explicit_source);
  RUN_TEST(test_ble_queue_poll_produces_event);
  RUN_TEST(test_din_queue_poll_produces_event);
  RUN_TEST(test_ble_provider_hook_feeds_poller);
  RUN_TEST(test_din_provider_hook_feeds_poller);
  RUN_TEST(test_din_queue_overflow_counts_drops);
  RUN_TEST(test_usb_provider_hook_feeds_poller);
  RUN_TEST(test_sysex_complete_message);
  RUN_TEST(test_sysex_realtime_interleaved);
  return UNITY_END();
}
