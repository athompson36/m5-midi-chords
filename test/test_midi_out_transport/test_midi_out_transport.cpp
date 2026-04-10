#include <stdint.h>
#include <vector>

#include <unity.h>

#include "MidiOut.h"

namespace {

std::vector<uint8_t> g_tx;

}  // namespace

extern "C" size_t m5ChordMidiBroadcastWrite(const uint8_t* bytes, size_t len) {
  if (!bytes || len == 0) return 0;
  g_tx.insert(g_tx.end(), bytes, bytes + len);
  return len;
}

void setUp(void) {
  g_tx.clear();
  midiOutResetTxStats();
}

void tearDown(void) {}

void test_midi_send_note_on_writes_three_bytes(void) {
  midiSendNoteOn(1, 64, 100);
  TEST_ASSERT_EQUAL_UINT32(3, g_tx.size());
  TEST_ASSERT_EQUAL_HEX8(0x91, g_tx[0]);
  TEST_ASSERT_EQUAL_HEX8(0x40, g_tx[1]);
  TEST_ASSERT_EQUAL_HEX8(0x64, g_tx[2]);
}

void test_midi_send_program_change_writes_two_bytes(void) {
  midiSendProgramChange(3, 10);
  TEST_ASSERT_EQUAL_UINT32(2, g_tx.size());
  TEST_ASSERT_EQUAL_HEX8(0xC3, g_tx[0]);
  TEST_ASSERT_EQUAL_HEX8(0x0A, g_tx[1]);
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_midi_send_note_on_writes_three_bytes);
  RUN_TEST(test_midi_send_program_change_writes_two_bytes);
  return UNITY_END();
}
