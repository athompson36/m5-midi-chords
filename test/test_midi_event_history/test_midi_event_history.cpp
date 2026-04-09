#include <unity.h>

#include "MidiEventHistory.h"

static MidiEvent mk(MidiSource s, MidiEventType t, uint8_t ch, uint8_t d1, uint8_t d2) {
  MidiEvent ev{};
  ev.source = s;
  ev.type = t;
  ev.channel = ch;
  ev.data1 = d1;
  ev.data2 = d2;
  return ev;
}

void test_newest_first_order() {
  MidiEventHistory h;
  h.clear();
  h.push(10, mk(MidiSource::Usb, MidiEventType::NoteOn, 0, 60, 100));
  h.push(20, mk(MidiSource::Ble, MidiEventType::Clock, 0, 0, 0));

  MidiEventHistoryItem it{};
  TEST_ASSERT_TRUE(h.getNewestFirst(0, &it));
  TEST_ASSERT_EQUAL_UINT32(20, it.atMs);
  TEST_ASSERT_EQUAL(MidiSource::Ble, it.source);
  TEST_ASSERT_EQUAL(MidiEventType::Clock, it.type);

  TEST_ASSERT_TRUE(h.getNewestFirst(1, &it));
  TEST_ASSERT_EQUAL_UINT32(10, it.atMs);
  TEST_ASSERT_EQUAL(MidiSource::Usb, it.source);
}

void test_ring_overwrite_keeps_capacity() {
  MidiEventHistory h;
  h.clear();
  for (size_t i = 0; i < MidiEventHistory::kCapacity + 5; ++i) {
    h.push(static_cast<uint32_t>(i), mk(MidiSource::Usb, MidiEventType::NoteOn, 0, static_cast<uint8_t>(i % 128), 100));
  }
  TEST_ASSERT_EQUAL_UINT32(MidiEventHistory::kCapacity, (uint32_t)h.size());
  MidiEventHistoryItem newest{};
  MidiEventHistoryItem oldest{};
  TEST_ASSERT_TRUE(h.getNewestFirst(0, &newest));
  TEST_ASSERT_TRUE(h.getNewestFirst(MidiEventHistory::kCapacity - 1, &oldest));
  TEST_ASSERT_EQUAL_UINT32(MidiEventHistory::kCapacity + 4, newest.atMs);
  TEST_ASSERT_EQUAL_UINT32(5, oldest.atMs);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_newest_first_order);
  RUN_TEST(test_ring_overwrite_keeps_capacity);
  return UNITY_END();
}
