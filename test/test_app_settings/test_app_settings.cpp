#include <unity.h>

#include "AppSettings.h"

void test_normalize_clamps_channels() {
  AppSettings s;
  s.midiOutChannel = 0;
  s.midiInChannel = 20;
  s.midiInChannelUsb = 20;
  s.midiInChannelBle = 33;
  s.midiInChannelDin = 99;
  s.midiTransportSend = 9;
  s.midiTransportReceive = 9;
  s.midiThruMask = 99;
  s.midiClockSource = 9;
  s.clkFollow = 9;
  s.clkFlashEdge = 9;
  s.midiDebugEnabled = 9;
  s.playInMonitor = 9;
  s.playInClockHold = 9;
  s.playInMonitorMode = 9;
  s.bleDebugPeakDecay = 9;
  s.panicDebugRotate = 9;
  s.suggestStableWindow = 9;
  s.suggestStableGap = 9;
  s.suggestProfile = 9;
  s.suggestProfileLock = 9;
  s.brightnessPercent = 3;
  s.outputVelocity = 200;
  s.velocityCurve = 9;
  s.globalSwingPct = 255;
  s.globalHumanizePct = 255;
  s.xyReturnToCenter = 9;
  s.settingsEntryHoldPreset = 255;
  s.seqLongPressPreset = 255;
  s.bpmHoldPreset = 255;
  s.normalize();
  TEST_ASSERT_EQUAL_UINT8(1, s.midiOutChannel);
  TEST_ASSERT_EQUAL_UINT8(16, s.midiInChannelUsb);
  TEST_ASSERT_EQUAL_UINT8(16, s.midiInChannelBle);
  TEST_ASSERT_EQUAL_UINT8(16, s.midiInChannelDin);
  TEST_ASSERT_EQUAL_UINT8(16, s.midiInChannel);
  TEST_ASSERT_EQUAL_UINT8(0, s.midiTransportSend);
  TEST_ASSERT_EQUAL_UINT8(0, s.midiTransportReceive);
  TEST_ASSERT_EQUAL_UINT8(0, s.midiThruMask);
  TEST_ASSERT_EQUAL_UINT8(0, s.midiClockSource);
  TEST_ASSERT_EQUAL_UINT8(1, s.clkFollow);
  TEST_ASSERT_EQUAL_UINT8(0, s.clkFlashEdge);
  TEST_ASSERT_EQUAL_UINT8(1, s.midiDebugEnabled);
  TEST_ASSERT_EQUAL_UINT8(0, s.playInMonitor);
  TEST_ASSERT_EQUAL_UINT8(1, s.playInClockHold);
  TEST_ASSERT_EQUAL_UINT8(0, s.playInMonitorMode);
  TEST_ASSERT_EQUAL_UINT8(2, s.bleDebugPeakDecay);
  TEST_ASSERT_EQUAL_UINT8(2, s.panicDebugRotate);
  TEST_ASSERT_EQUAL_UINT8(2, s.suggestStableWindow);
  TEST_ASSERT_EQUAL_UINT8(2, s.suggestStableGap);
  TEST_ASSERT_EQUAL_UINT8(0, s.suggestProfile);
  TEST_ASSERT_EQUAL_UINT8(0, s.suggestProfileLock);
  TEST_ASSERT_EQUAL_UINT8(10, s.brightnessPercent);
  TEST_ASSERT_EQUAL_UINT8(127, s.outputVelocity);
  TEST_ASSERT_EQUAL_UINT8(0, s.velocityCurve);
  TEST_ASSERT_EQUAL_UINT8(0, s.globalSwingPct);
  TEST_ASSERT_EQUAL_UINT8(0, s.globalHumanizePct);
  TEST_ASSERT_EQUAL_UINT8(0, s.xyReturnToCenter);
  TEST_ASSERT_EQUAL_UINT8(2, s.settingsEntryHoldPreset);
  TEST_ASSERT_EQUAL_UINT8(2, s.seqLongPressPreset);
  TEST_ASSERT_EQUAL_UINT8(1, s.bpmHoldPreset);
  s.transposeSemitones = 100;
  s.normalize();
  TEST_ASSERT_EQUAL_INT8(24, s.transposeSemitones);
  s.transposeSemitones = -100;
  s.normalize();
  TEST_ASSERT_EQUAL_INT8(-24, s.transposeSemitones);
}

void test_cycle_midi_out_wraps() {
  AppSettings s;
  s.midiOutChannel = 16;
  s.cycleMidiOut();
  TEST_ASSERT_EQUAL_UINT8(1, s.midiOutChannel);
}

void test_cycle_midi_in_usb_includes_omni() {
  AppSettings s;
  s.midiInChannelUsb = 16;
  s.cycleMidiInUsb();
  TEST_ASSERT_EQUAL_UINT8(0, s.midiInChannelUsb);
  TEST_ASSERT_EQUAL_UINT8(0, s.midiInChannel);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_normalize_clamps_channels);
  RUN_TEST(test_cycle_midi_out_wraps);
  RUN_TEST(test_cycle_midi_in_usb_includes_omni);
  return UNITY_END();
}
