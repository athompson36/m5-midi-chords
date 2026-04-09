#include <unity.h>

#include "MidiInState.h"

static MidiEvent makeNote(MidiSource src, uint8_t ch, uint8_t note, uint8_t vel, bool on) {
  MidiEvent ev{};
  ev.source = src;
  ev.channel = ch;
  ev.data1 = note;
  ev.data2 = vel;
  ev.type = on ? MidiEventType::NoteOn : MidiEventType::NoteOff;
  return ev;
}

void test_tracks_notes_per_source_and_channel() {
  MidiInState st;
  st.reset();

  st.onEvent(makeNote(MidiSource::Usb, 0, 60, 100, true));
  st.onEvent(makeNote(MidiSource::Ble, 0, 60, 100, true));
  TEST_ASSERT_EQUAL_UINT8(1, st.activeNotesOnChannel(MidiSource::Usb, 0));
  TEST_ASSERT_EQUAL_UINT8(1, st.activeNotesOnChannel(MidiSource::Ble, 0));
  TEST_ASSERT_TRUE(st.isNoteActive(MidiSource::Usb, 0, 60));
  TEST_ASSERT_FALSE(st.isNoteActive(MidiSource::Din, 0, 60));
}

void test_note_on_zero_velocity_counts_as_off() {
  MidiInState st;
  st.reset();

  st.onEvent(makeNote(MidiSource::Usb, 2, 64, 90, true));
  TEST_ASSERT_TRUE(st.isNoteActive(MidiSource::Usb, 2, 64));

  MidiEvent offByVel0 = makeNote(MidiSource::Usb, 2, 64, 0, true);
  st.onEvent(offByVel0);
  TEST_ASSERT_FALSE(st.isNoteActive(MidiSource::Usb, 2, 64));
  TEST_ASSERT_EQUAL_UINT8(0, st.activeNotesOnChannel(MidiSource::Usb, 2));
}

void test_cc123_clears_channel() {
  MidiInState st;
  st.reset();
  st.onEvent(makeNote(MidiSource::Usb, 5, 50, 100, true));
  st.onEvent(makeNote(MidiSource::Usb, 5, 53, 100, true));
  TEST_ASSERT_EQUAL_UINT8(2, st.activeNotesOnChannel(MidiSource::Usb, 5));

  MidiEvent panic{};
  panic.source = MidiSource::Usb;
  panic.channel = 5;
  panic.type = MidiEventType::ControlChange;
  panic.data1 = 123;
  panic.data2 = 0;
  st.onEvent(panic);
  TEST_ASSERT_EQUAL_UINT8(0, st.activeNotesOnChannel(MidiSource::Usb, 5));
}

void test_collect_active_notes_sorted() {
  MidiInState st;
  st.reset();
  st.onEvent(makeNote(MidiSource::Din, 1, 72, 100, true));
  st.onEvent(makeNote(MidiSource::Din, 1, 48, 100, true));

  uint8_t out[8] = {};
  size_t n = 0;
  st.collectActiveNotes(MidiSource::Din, 1, out, 8, &n);
  TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)n);
  TEST_ASSERT_EQUAL_UINT8(48, out[0]);
  TEST_ASSERT_EQUAL_UINT8(72, out[1]);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tracks_notes_per_source_and_channel);
  RUN_TEST(test_note_on_zero_velocity_counts_as_off);
  RUN_TEST(test_cc123_clears_channel);
  RUN_TEST(test_collect_active_notes_sorted);
  return UNITY_END();
}
