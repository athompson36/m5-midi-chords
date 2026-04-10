#include <unity.h>

#include "MidiMmc.h"

void test_mmc_stop_play_deferred_pause() {
  uint8_t cmd = 0;
  const uint8_t stop[] = {0xF0, 0x7F, 0x7F, 0x06, 0x01, 0xF7};
  TEST_ASSERT_TRUE(midiMmcParseRealtime(stop, sizeof(stop), &cmd));
  TEST_ASSERT_EQUAL_UINT8(0x01, cmd);

  const uint8_t play[] = {0xF0, 0x7F, 0x10, 0x06, 0x02, 0xF7};
  TEST_ASSERT_TRUE(midiMmcParseRealtime(play, sizeof(play), &cmd));
  TEST_ASSERT_EQUAL_UINT8(0x02, cmd);

  const uint8_t deferred[] = {0xF0, 0x7F, 0x00, 0x06, 0x03, 0xF7};
  TEST_ASSERT_TRUE(midiMmcParseRealtime(deferred, sizeof(deferred), &cmd));
  TEST_ASSERT_EQUAL_UINT8(0x03, cmd);

  const uint8_t pause[] = {0xF0, 0x7F, 0x7F, 0x06, 0x09, 0xF7};
  TEST_ASSERT_TRUE(midiMmcParseRealtime(pause, sizeof(pause), &cmd));
  TEST_ASSERT_EQUAL_UINT8(0x09, cmd);
}

void test_mmc_rejects_wrong_length_or_header() {
  uint8_t cmd = 0;
  const uint8_t shortMsg[] = {0xF0, 0x7F, 0x7F, 0x06, 0xF7};
  TEST_ASSERT_FALSE(midiMmcParseRealtime(shortMsg, sizeof(shortMsg), &cmd));

  const uint8_t longMsg[] = {0xF0, 0x7F, 0x7F, 0x06, 0x01, 0x00, 0xF7};
  TEST_ASSERT_FALSE(midiMmcParseRealtime(longMsg, sizeof(longMsg), &cmd));

  const uint8_t badSub[] = {0xF0, 0x7F, 0x7F, 0x05, 0x01, 0xF7};
  TEST_ASSERT_FALSE(midiMmcParseRealtime(badSub, sizeof(badSub), &cmd));

  const uint8_t notSysex[] = {0x90, 0x7F, 0x7F, 0x06, 0x01, 0xF7};
  TEST_ASSERT_FALSE(midiMmcParseRealtime(notSysex, sizeof(notSysex), &cmd));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_mmc_stop_play_deferred_pause);
  RUN_TEST(test_mmc_rejects_wrong_length_or_header);
  return UNITY_END();
}
