#pragma once

// Shared include bundle for firmware TUs: `main.cpp` and `src/screens/*Screen.cpp` (via M5ChordsAppScreensPch.h).

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_random.h>
#include <stdlib.h>
#include <string.h>

#include "AppSettings.h"
#include "BleMidiTransport.h"
#include "BuildInfo.h"
#include "ChordModel.h"
#include "MidiChordDetect.h"
#include "MidiEventHistory.h"
#include "MidiInState.h"
#include "MidiIngress.h"
#include "MidiMmc.h"
#include "DinMidiTransport.h"
#include "MidiOut.h"
#include "MidiSuggest.h"
#include "SdBackup.h"
#include "SettingsEntryGesture.h"
#include "Arpeggio.h"
#include "SettingsStore.h"
#include "SeqExtras.h"
#include "Transport.h"
#include "ClockRate.h"
#include "LinkSync.h"
#include "UiTheme.h"
#include "UsbMidiTransport.h"
#include "ui/UiTypes.h"
#include "ui/UiLayout.h"
#include "ui/UiGloss.h"
#include "ui/UiDrawers.h"
#include "m5chords_app/M5ChordsAppShared.h"

#ifndef M5CHORD_ENABLE_MIDI_DEBUG_SCREEN
#define M5CHORD_ENABLE_MIDI_DEBUG_SCREEN 1
#endif
