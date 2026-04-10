// ----- Settings UI (sections, large type, dropdowns, bottom bezel bar) -----

static const uint8_t kSetMidi[] = {0, 1, 18, 19, 17, 2, 3, 37, 4, 22, 23, 21, 20, 24, 25, 26, 27, 28, 29, 33, 35, 34, 36, 30, 31, 32};
static const uint8_t kSetDisplay[] = {5, 8, 9, 7};
static const uint8_t kSetSeq[] = {6, 43, 38, 39, 40, 41, 42, 10, 11, 12};
static const uint8_t kSetStorage[] = {13, 14, 15, 16};

static int settingsPanelRowCount(SettingsPanel p) {
  switch (p) {
    case SettingsPanel::Midi:
      return 26;
    case SettingsPanel::Display:
      return 4;
    case SettingsPanel::SeqArp:
      return 10;
    case SettingsPanel::Storage:
      return 4;
    case SettingsPanel::UserGuide:
      return 0;
    default:
      return 0;
  }
}

static uint8_t settingsPanelRowId(SettingsPanel p, int idx) {
  const uint8_t* t = nullptr;
  int c = 0;
  switch (p) {
    case SettingsPanel::Midi:
      t = kSetMidi;
      c = 26;
      break;
    case SettingsPanel::Display:
      t = kSetDisplay;
      c = 4;
      break;
    case SettingsPanel::SeqArp:
      t = kSetSeq;
      c = 10;
      break;
    case SettingsPanel::Storage:
      t = kSetStorage;
      c = 4;
      break;
    default:
      return 0;
  }
  if (idx < 0 || idx >= c) return t[0];
  return t[idx];
}

static void settingsSyncRowFromCursor() {
  if (g_settingsPanel == SettingsPanel::Menu) return;
  int n = settingsPanelRowCount(g_settingsPanel);
  if (n <= 0) return;
  int i = g_settingsCursorRow;
  if (i < 0) i = 0;
  if (i >= n) i = n - 1;
  g_settingsRow = settingsPanelRowId(g_settingsPanel, i);
}

static void settingsEnsureListScroll(int nRows, int rowH, int contentTop, int contentBottom) {
  int visible = (contentBottom - contentTop) / rowH;
  if (visible < 1) visible = 1;
  if (g_settingsCursorRow < g_settingsListScroll) {
    g_settingsListScroll = g_settingsCursorRow;
  }
  if (g_settingsCursorRow >= g_settingsListScroll + visible) {
    g_settingsListScroll = g_settingsCursorRow - visible + 1;
  }
  int maxScroll = max(0, nRows - visible);
  if (g_settingsListScroll > maxScroll) g_settingsListScroll = maxScroll;
  if (g_settingsListScroll < 0) g_settingsListScroll = 0;
}

static void settingsMoveCursorInSection(int delta, int h) {
  int n = settingsPanelRowCount(g_settingsPanel);
  if (n <= 0) return;
  g_settingsCursorRow = (g_settingsCursorRow + delta + n * 100) % n;
  settingsSyncRowFromCursor();
  constexpr int kRowH = 36;
  constexpr int kTop = 42;
  int fb = (g_settingsFeedback[0] != '\0') ? 20 : 4;
  int contentBottom = h - kBezelBarH - fb;
  settingsEnsureListScroll(n, kRowH, kTop, contentBottom);
}

static int settingsDropdownOptionCount(int8_t rid) {
  switch (rid) {
    case 0:
      return 16;
    case 1:
    case 18:
    case 19:
      return 17;
    case 2:
    case 3:
    case 4:
      return 4;
    case 37:
      return 8;
    case 5:
      return 10;
    case 6:
      return 9;
    case 43:
      return 3;
    case 38:
      return 11;
    case 39:
      return 11;
    case 40:
    case 41:
      return 5;
    case 42:
      return 4;
    case 8:
      return kUiThemeCount;
    case 9:
      return 11;
    case 10:
      return kArpeggiatorModeCount;
    case 11:
      return 33;
    case 17:
      return 49;
    case 13:
      return 2;
    case 21:
    case 22:
    case 23:
    case 25:
      return 2;
    case 26:
      return 3;
    case 27:
      return 2;
    case 28:
      return 4;
    case 29:
      return 4;
    case 30:
    case 31:
      return 5;
    case 33:
    case 35:
      return 2;
    default:
      return 0;
  }
}

static int settingsDropVisibleRows(int h) {
  const int rowH = 20;
  const int maxH = h - kBezelBarH - 52;
  return max(3, maxH / rowH);
}

static int settingsDropdownScrollForCurrent(int8_t rid, int h) {
  int cnt = settingsDropdownOptionCount(rid);
  if (cnt <= 0) return 0;
  int idx = 0;
  switch (rid) {
    case 0:
      idx = g_settings.midiOutChannel - 1;
      break;
    case 1:
      idx = (g_settings.midiInChannelUsb == 0) ? 0 : g_settings.midiInChannelUsb;
      break;
    case 18:
      idx = (g_settings.midiInChannelBle == 0) ? 0 : g_settings.midiInChannelBle;
      break;
    case 19:
      idx = (g_settings.midiInChannelDin == 0) ? 0 : g_settings.midiInChannelDin;
      break;
    case 2:
      idx = g_settings.midiTransportSend;
      break;
    case 3:
      idx = g_settings.midiTransportReceive;
      break;
    case 4:
      idx = g_settings.midiClockSource;
      break;
    case 37:
      idx = static_cast<int>(g_settings.midiThruMask & 0x07U);
      break;
    case 5:
      idx = static_cast<int>(g_settings.brightnessPercent) / 10 - 1;
      if (idx < 0) idx = 0;
      if (idx > 9) idx = 9;
      break;
    case 6: {
      uint8_t v = g_settings.outputVelocity;
      if (v <= 40) {
        idx = 0;
      } else if (v >= 120) {
        idx = 8;
      } else {
        idx = (static_cast<int>(v) - 40 + 5) / 10;
        if (idx > 8) idx = 8;
      }
      break;
    }
    case 43:
      idx = static_cast<int>(g_settings.velocityCurve);
      if (idx < 0) idx = 0;
      if (idx > 2) idx = 0;
      break;
    case 38:
      idx = static_cast<int>(g_settings.globalSwingPct) / 10;
      if (idx < 0) idx = 0;
      if (idx > 10) idx = 10;
      break;
    case 39:
      idx = static_cast<int>(g_settings.globalHumanizePct) / 10;
      if (idx < 0) idx = 0;
      if (idx > 10) idx = 10;
      break;
    case 40:
      idx = static_cast<int>(g_settings.settingsEntryHoldPreset);
      if (idx < 0) idx = 0;
      if (idx > 4) idx = 2;
      break;
    case 41:
      idx = static_cast<int>(g_settings.seqLongPressPreset);
      if (idx < 0) idx = 0;
      if (idx > 4) idx = 2;
      break;
    case 42:
      idx = static_cast<int>(g_settings.bpmHoldPreset);
      if (idx < 0) idx = 0;
      if (idx > 3) idx = 1;
      break;
    case 8:
      idx = g_uiTheme;
      break;
    case 9:
      idx = static_cast<int>(g_clickVolumePercent) / 10;
      if (idx > 10) idx = 10;
      break;
    case 10:
      idx = g_settings.arpeggiatorMode;
      break;
    case 11:
      idx = (static_cast<int>(g_projectBpm) - 40) / 5;
      if (idx < 0) idx = 0;
      if (idx > 32) idx = 32;
      break;
    case 17:
      idx = static_cast<int>(g_settings.transposeSemitones) + 24;
      if (idx < 0) idx = 0;
      if (idx > 48) idx = 48;
      break;
    case 21:
      idx = g_settings.midiDebugEnabled ? 1 : 0;
      break;
    case 22:
      idx = g_settings.clkFollow ? 1 : 0;
      break;
    case 23:
      idx = g_settings.clkFlashEdge ? 1 : 0;
      break;
    case 25:
      idx = g_settings.playInMonitor ? 1 : 0;
      break;
    case 26:
      idx = static_cast<int>(g_settings.playInClockHold);
      if (idx < 0) idx = 0;
      if (idx > 2) idx = 1;
      break;
    case 27:
      idx = g_settings.playInMonitorMode ? 1 : 0;
      break;
    case 28:
      idx = static_cast<int>(g_settings.bleDebugPeakDecay);
      if (idx < 0) idx = 0;
      if (idx > 3) idx = 2;
      break;
    case 29:
      idx = static_cast<int>(g_settings.panicDebugRotate);
      if (idx < 0) idx = 0;
      if (idx > 3) idx = 2;
      break;
    case 30:
      idx = static_cast<int>(g_settings.suggestStableWindow);
      if (idx < 0) idx = 0;
      if (idx > 4) idx = 2;
      break;
    case 31:
      idx = static_cast<int>(g_settings.suggestStableGap);
      if (idx < 0) idx = 0;
      if (idx > 4) idx = 2;
      break;
    case 33:
      idx = g_settings.suggestProfile ? 1 : 0;
      break;
    case 35:
      idx = g_settings.suggestProfileLock ? 1 : 0;
      break;
    default:
      return 0;
  }
  int vis = settingsDropVisibleRows(h);
  int scroll = idx - vis / 2;
  if (scroll < 0) scroll = 0;
  int maxScroll = max(0, cnt - vis);
  if (scroll > maxScroll) scroll = maxScroll;
  return scroll;
}

static void settingsFormatDropdownOption(int8_t rid, int opt, char* buf, size_t n) {
  switch (rid) {
    case 0:
      snprintf(buf, n, "Channel %u", static_cast<unsigned>(opt + 1));
      break;
    case 1:
    case 18:
    case 19:
      if (opt == 0) {
        snprintf(buf, n, "OMNI");
      } else {
        snprintf(buf, n, "Channel %u", static_cast<unsigned>(opt));
      }
      break;
    case 2:
    case 3:
      snprintf(buf, n, "%s", midiTransportRouteLabel(static_cast<uint8_t>(opt)));
      break;
    case 4:
      snprintf(buf, n, "%s", midiClockSourceLabel(static_cast<uint8_t>(opt)));
      break;
    case 37:
      switch (opt) {
        case 0:
          snprintf(buf, n, "Off");
          break;
        case 1:
          snprintf(buf, n, "USB");
          break;
        case 2:
          snprintf(buf, n, "BLE");
          break;
        case 3:
          snprintf(buf, n, "USB+BLE");
          break;
        case 4:
          snprintf(buf, n, "DIN");
          break;
        case 5:
          snprintf(buf, n, "USB+DIN");
          break;
        case 6:
          snprintf(buf, n, "BLE+DIN");
          break;
        default:
          snprintf(buf, n, "All");
          break;
      }
      break;
    case 5:
      snprintf(buf, n, "%u%%", static_cast<unsigned>((opt + 1) * 10));
      break;
    case 6:
      snprintf(buf, n, "%u", static_cast<unsigned>(40 + opt * 10));
      break;
    case 43:
      snprintf(buf, n, "%s", opt == 0 ? "Linear" : (opt == 1 ? "Soft" : "Hard"));
      break;
    case 38:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(opt * 10));
      break;
    case 39:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(opt * 10));
      break;
    case 40:
      snprintf(buf, n, "%s", opt == 0 ? "0.5s" :
                             (opt == 1 ? "0.65s" : (opt == 2 ? "0.8s" : (opt == 3 ? "1.0s" : "1.2s"))));
      break;
    case 41:
      snprintf(buf, n, "%s", opt == 0 ? "0.25s" :
                             (opt == 1 ? "0.3s" : (opt == 2 ? "0.4s" : (opt == 3 ? "0.5s" : "0.7s"))));
      break;
    case 42:
      snprintf(buf, n, "%s", opt == 0 ? "0.35s" : (opt == 1 ? "0.5s" : (opt == 2 ? "0.7s" : "0.9s")));
      break;
    case 8:
      snprintf(buf, n, "%s", uiThemeName(static_cast<uint8_t>(opt)));
      break;
    case 9:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(opt * 10));
      break;
    case 10:
      snprintf(buf, n, "%s", arpeggiatorModeLabel(static_cast<uint8_t>(opt)));
      break;
    case 11:
      snprintf(buf, n, "%u BPM", static_cast<unsigned>(40 + opt * 5));
      break;
    case 17: {
      const int v = opt - 24;
      if (v > 0) {
        snprintf(buf, n, "+%d", v);
      } else {
        snprintf(buf, n, "%d", v);
      }
      break;
    }
    case 13:
      snprintf(buf, n, "%s", opt == 0 ? "Cancel" : "Erase everything");
      break;
    case 21:
      snprintf(buf, n, "%s", opt == 0 ? "Off" : "On");
      break;
    case 22:
      snprintf(buf, n, "%s", opt == 0 ? "Off" : "On");
      break;
    case 23:
      snprintf(buf, n, "%s", opt == 0 ? "Quarter" : "Eighth");
      break;
    case 25:
      snprintf(buf, n, "%s", opt == 0 ? "Off" : "On");
      break;
    case 26:
      snprintf(buf, n, "%s", opt == 0 ? "Short (0.6s)" : (opt == 1 ? "Medium (1.2s)" : "Long (2.0s)"));
      break;
    case 27:
      snprintf(buf, n, "%s", opt == 0 ? "Compact" : "Detailed");
      break;
    case 28:
      snprintf(buf, n, "%s", opt == 0 ? "Off" :
                             (opt == 1 ? "5 seconds" : (opt == 2 ? "10 seconds" : "30 seconds")));
      break;
    case 29:
      snprintf(buf, n, "%s", opt == 0 ? "Off" : (opt == 1 ? "1 second" : (opt == 2 ? "2 seconds" : "4 seconds")));
      break;
    case 30:
      snprintf(buf, n, "%s", opt == 0
                               ? "0.6s"
                               : (opt == 1 ? "1.0s" : (opt == 2 ? "1.4s" : (opt == 3 ? "2.0s" : "3.0s"))));
      break;
    case 31:
      snprintf(buf, n, "%s", opt == 0
                               ? "8"
                               : (opt == 1 ? "12" : (opt == 2 ? "18" : (opt == 3 ? "24" : "32"))));
      break;
    case 33:
      snprintf(buf, n, "%s", opt == 0 ? "A (balanced)" : "B (sticky)");
      break;
    case 35:
      snprintf(buf, n, "%s", opt == 0 ? "Off" : "On");
      break;
    default:
      buf[0] = '\0';
      break;
  }
}

static void settingsRunFactoryReset() {
  panicForTrigger(PanicTrigger::FactoryReset);
  factoryResetAll(g_settings, g_model);
  memset(g_seqPattern, kSeqRest, sizeof(g_seqPattern));
  seqExtrasLoad(&g_seqExtras);
  seqLaneChannelsLoad(g_seqMidiCh);
  chordVoicingLoad(&g_chordVoicing);
  g_seqLane = 0;
  xyMappingLoad(&g_xyOutChannel, &g_xyCcA, &g_xyCcB, &g_xyCurveA, &g_xyCurveB);
  projectBpmLoad(&g_projectBpm);
  projectCustomNameLoad(g_projectCustomName);
  lastProjectFolderLoad(g_lastProjectFolder);
  uiThemeLoad(&g_uiTheme);
  uiThemeApply(g_uiTheme);
  transportPrefsLoad();
  transportApplyClickVolume();
  g_factoryResetConfirmArmed = false;
  applyBrightness();
}

static void settingsRunBackup() {
  char folder[48];
  resolveBackupFolder(folder);
  const uint16_t bm = g_projectBpm;
  if (sdBackupWriteAll(g_settings, g_model, g_seqPattern, g_seqMidiCh, g_xyOutChannel, g_xyCcA, g_xyCcB,
                       g_xyCurveA, g_xyCurveB, bm, g_chordVoicing, &g_seqExtras, folder)) {
    lastProjectFolderSave(folder);
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "OK %s/%s", SD_BACKUP_ROOT, folder);
  } else {
    panicForTrigger(PanicTrigger::ErrorPath);
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SD backup failed");
  }
}

static void settingsRunRestoreFlow() {
  char dirs[8][48];
  int n = 0;
  if (!sdBackupListProjects(dirs, 8, &n)) {
    panicForTrigger(PanicTrigger::ErrorPath);
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SD list failed");
    return;
  }
  if (n == 0) {
    panicForTrigger(PanicTrigger::ErrorPath);
    snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "No projects in " SD_BACKUP_ROOT);
    return;
  }
  if (n == 1) {
    panicForTrigger(PanicTrigger::RestoreTransition);
    applySdRestoreFromFolder(dirs[0]);
    return;
  }
  panicForTrigger(PanicTrigger::RestoreTransition);
  g_sdPickCount = n;
  for (int i = 0; i < n && i < 8; ++i) {
    strncpy(g_sdPickNames[i], dirs[i], 47);
    g_sdPickNames[i][47] = '\0';
  }
  g_screen = Screen::SdProjectPick;
  drawSdProjectPick();
}

static void settingsRunClearMidiDebugData() {
  s_midiEventHistory.clear();
  s_midiInState.reset();
  bleMidiResetRxBytes();
  s_bleDbgRatePrevMs = 0;
  s_bleDbgRatePrevRx = 0;
  s_bleDbgRateBps = 0;
  s_bleDbgRatePeakBps = 0;
  s_bleDbgLastActivityMs = 0;
  s_bleDbgPeakHeld = true;
  dinMidiResetRxBytes();
  s_dinDbgRatePrevMs = 0;
  s_dinDbgRatePrevRx = 0;
  s_dinDbgRateBps = 0;
  s_dinDbgRatePeakBps = 0;
  s_dinDbgLastActivityMs = 0;
  s_dinDbgPeakHeld = true;
  midiOutResetTxStats();
  s_outDbgRatePrevMs = 0;
  s_outDbgRatePrevTx = 0;
  s_outDbgRateBps = 0;
  s_outDbgRatePeakBps = 0;
  midiIngressResetQueueDropTotals();
  s_bleDbgPrevDrop = 0;
  s_bleDbgWarnUntilMs = 0;
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
  s_playIngressInfo[0] = '\0';
  s_playIngressInfoUntilMs = 0;
  s_playIngressLastMusicalMs = 0;
  s_playIngressInfoEventMs = 0;
  for (size_t i = 0; i < static_cast<size_t>(PanicTrigger::Count); ++i) {
    s_panicTriggerCounts[i] = 0;
  }
  snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "MIDI debug data cleared");
}

static void settingsSaveAndExit() {
  panicForTrigger(PanicTrigger::SettingsSaveExit);
  g_settings.normalize();
  settingsSave(g_settings);
  chordStateSave(g_model);
  seqPatternSave(g_seqPattern);
  seqExtrasSave(&g_seqExtras);
  seqLaneChannelsSave(g_seqMidiCh);
  chordVoicingSave(g_chordVoicing);
  xyMappingSave(g_xyOutChannel, g_xyCcA, g_xyCcB, g_xyCurveA, g_xyCurveB);
  projectBpmSave(g_projectBpm);
  projectCustomNameSave(g_projectCustomName);
  uiThemeSave(g_uiTheme);
  transportPrefsSave();
  g_screen = Screen::Play;
  g_lastAction = "Settings saved";
  resetSettingsNav();
  drawPlaySurface();
}

static void settingsApplyDropdownPick(int8_t rid, int opt) {
  switch (rid) {
    case 0:
      g_settings.midiOutChannel = static_cast<uint8_t>(opt + 1);
      break;
    case 1:
      g_settings.midiInChannelUsb = static_cast<uint8_t>(opt == 0 ? 0 : opt);
      break;
    case 18:
      g_settings.midiInChannelBle = static_cast<uint8_t>(opt == 0 ? 0 : opt);
      break;
    case 19:
      g_settings.midiInChannelDin = static_cast<uint8_t>(opt == 0 ? 0 : opt);
      break;
    case 2:
      g_settings.midiTransportSend = static_cast<uint8_t>(opt);
      settingsSave(g_settings);
      break;
    case 3:
      g_settings.midiTransportReceive = static_cast<uint8_t>(opt);
      settingsSave(g_settings);
      break;
    case 4:
      g_settings.midiClockSource = static_cast<uint8_t>(opt);
      settingsSave(g_settings);
      break;
    case 37:
      g_settings.midiThruMask = static_cast<uint8_t>(opt & 0x07);
      settingsSave(g_settings);
      break;
    case 5:
      g_settings.brightnessPercent = static_cast<uint8_t>((opt + 1) * 10);
      applyBrightness();
      break;
    case 6:
      g_settings.outputVelocity = static_cast<uint8_t>(40 + opt * 10);
      break;
    case 43:
      g_settings.velocityCurve = static_cast<uint8_t>((opt < 0 || opt > 2) ? 0 : opt);
      settingsSave(g_settings);
      break;
    case 38:
      g_settings.globalSwingPct = static_cast<uint8_t>(opt * 10);
      settingsSave(g_settings);
      transportSetGlobalSwing(g_settings.globalSwingPct);
      break;
    case 39:
      g_settings.globalHumanizePct = static_cast<uint8_t>(opt * 10);
      settingsSave(g_settings);
      transportSetGlobalHumanize(g_settings.globalHumanizePct);
      break;
    case 40:
      g_settings.settingsEntryHoldPreset = static_cast<uint8_t>((opt < 0 || opt > 4) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 41:
      g_settings.seqLongPressPreset = static_cast<uint8_t>((opt < 0 || opt > 4) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 42:
      g_settings.bpmHoldPreset = static_cast<uint8_t>((opt < 0 || opt > 3) ? 1 : opt);
      settingsSave(g_settings);
      break;
    case 8:
      g_uiTheme = static_cast<uint8_t>(opt);
      uiThemeSave(g_uiTheme);
      uiThemeApply(g_uiTheme);
      break;
    case 9:
      g_clickVolumePercent = static_cast<uint8_t>(opt * 10);
      transportApplyClickVolume();
      transportPrefsSave();
      break;
    case 10:
      g_settings.arpeggiatorMode = static_cast<uint8_t>(opt);
      settingsSave(g_settings);
      break;
    case 11:
      g_projectBpm = static_cast<uint16_t>(40 + opt * 5);
      projectBpmSave(g_projectBpm);
      break;
    case 17:
      g_settings.transposeSemitones = static_cast<int8_t>(opt - 24);
      settingsSave(g_settings);
      break;
    case 13:
      if (opt == 1) {
        settingsRunFactoryReset();
      }
      break;
    case 21:
      g_settings.midiDebugEnabled = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      break;
    case 22:
      g_settings.clkFollow = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      break;
    case 23:
      g_settings.clkFlashEdge = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      break;
    case 25:
      g_settings.playInMonitor = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      break;
    case 26:
      g_settings.playInClockHold = static_cast<uint8_t>(opt > 2 ? 1 : opt);
      settingsSave(g_settings);
      break;
    case 27:
      g_settings.playInMonitorMode = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      break;
    case 28:
      g_settings.bleDebugPeakDecay = static_cast<uint8_t>((opt < 0 || opt > 3) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 29:
      g_settings.panicDebugRotate = static_cast<uint8_t>((opt < 0 || opt > 3) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 30:
      if (g_settings.suggestProfileLock) {
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Set SUG lock Off to edit");
        break;
      }
      g_settings.suggestStableWindow = static_cast<uint8_t>((opt < 0 || opt > 4) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 31:
      if (g_settings.suggestProfileLock) {
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Set SUG lock Off to edit");
        break;
      }
      g_settings.suggestStableGap = static_cast<uint8_t>((opt < 0 || opt > 4) ? 2 : opt);
      settingsSave(g_settings);
      break;
    case 33:
      g_settings.suggestProfile = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      if (g_settings.suggestProfile == 0) {
        g_settings.suggestStableWindow = 2;  // 1.4s
        g_settings.suggestStableGap = 2;     // 18
      } else {
        g_settings.suggestStableWindow = 3;  // 2.0s
        g_settings.suggestStableGap = 3;     // 24
      }
      g_settings.suggestProfileLock = 1;
      settingsSave(g_settings);
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG profile %c applied (lock on)",
               g_settings.suggestProfile == 0 ? 'A' : 'B');
      break;
    case 35:
      g_settings.suggestProfileLock = static_cast<uint8_t>(opt == 0 ? 0 : 1);
      settingsSave(g_settings);
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG lock %s",
               g_settings.suggestProfileLock ? "on" : "off");
      break;
    default:
      break;
  }
  g_settings.normalize();
}

static void settingsApplySuggestionDefaults() {
  g_settings.suggestStableWindow = 2;  // 1.4s
  g_settings.suggestStableGap = 2;     // 18
  g_settings.suggestProfile = 0;
  g_settings.suggestProfileLock = 0;
  g_settings.normalize();
  settingsSave(g_settings);
  snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG defaults restored");
}

static void settingsFlipSuggestionProfile() {
  g_settings.suggestProfile = (g_settings.suggestProfile == 0) ? 1 : 0;
  if (g_settings.suggestProfile == 0) {
    g_settings.suggestStableWindow = 2;  // 1.4s
    g_settings.suggestStableGap = 2;     // 18
  } else {
    g_settings.suggestStableWindow = 3;  // 2.0s
    g_settings.suggestStableGap = 3;     // 24
  }
  g_settings.suggestProfileLock = 1;
  g_settings.normalize();
  settingsSave(g_settings);
  snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG flipped to %c (lock on)",
           g_settings.suggestProfile == 0 ? 'A' : 'B');
}

static void settingsOpenDropdownOrAction(uint8_t rid, int w, int h) {
  (void)w;
  switch (rid) {
    case 7:
      return;
    case 12:
      openProjectNameEditor();
      return;
    case 14:
      settingsRunBackup();
      return;
    case 15:
      g_settingsConfirmAction = SettingsConfirmAction::RestoreSd;
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Confirm restore from SD");
      return;
    case 16:
      settingsSaveAndExit();
      return;
    case 20:
#if M5CHORD_ENABLE_MIDI_DEBUG_SCREEN
      if (!g_settings.midiDebugEnabled) {
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Enable MIDI debug first");
        return;
      }
      g_screen = Screen::MidiDebug;
      drawMidiDebugSurface();
#else
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "MIDI debug disabled in build");
#endif
      return;
    case 24:
      g_settingsConfirmAction = SettingsConfirmAction::ClearMidiDebug;
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Confirm clear MIDI debug");
      return;
    case 32:
      settingsApplySuggestionDefaults();
      return;
    case 34:
      settingsFlipSuggestionProfile();
      return;
    case 36:
      if (!g_settings.suggestProfileLock) {
        snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG already unlocked");
        return;
      }
      g_settings.suggestProfileLock = 0;
      g_settings.normalize();
      settingsSave(g_settings);
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "SUG lock off");
      return;
    default:
      break;
  }
  if (settingsDropdownOptionCount(static_cast<int8_t>(rid)) <= 0) return;
  s_settingsDropRowId = static_cast<int8_t>(rid);
  s_settingsDropOptScroll = settingsDropdownScrollForCurrent(s_settingsDropRowId, h);
  s_settingsDropDragLastY = -1;
  s_settingsDropFingerScrollCount = 0;
}

static void settingsComputeDropdownLayout(int w, int h, int8_t rid, int* outX, int* outY, int* outW,
                                          int* rowH, int* firstOpt, int* visCount, int* totalOpts) {
  *totalOpts = settingsDropdownOptionCount(rid);
  *rowH = 20;
  *visCount = min(*totalOpts, settingsDropVisibleRows(h));
  int totalH = (*rowH) * (*visCount);
  *outW = min(w - 16, 220);
  *outX = (w - *outW) / 2;
  *outY = max(36, (h - kBezelBarH - totalH) / 2);
  *firstOpt = s_settingsDropOptScroll;
  if (*firstOpt + *visCount > *totalOpts) {
    *firstOpt = max(0, *totalOpts - *visCount);
    s_settingsDropOptScroll = *firstOpt;
  }
}

static int settingsHitDropdown(int px, int py, int w, int h) {
  int dx, dy, dw, rh, first, vis, tot;
  settingsComputeDropdownLayout(w, h, s_settingsDropRowId, &dx, &dy, &dw, &rh, &first, &vis, &tot);
  if (px < dx || px >= dx + dw || py < dy || py >= dy + vis * rh) return -1;
  int row = (py - dy) / rh;
  int idx = first + row;
  if (idx < 0 || idx >= tot) return -1;
  return idx;
}

static int settingsHitMenuCell(int px, int py, int w, int h) {
  constexpr int kTop = 38;
  constexpr int gap = 8;
  constexpr int kGuideBtnH = 28;
  int y0 = kTop;
  int availH = h - kBezelBarH - y0 - gap;
  int availW = w - 2 * gap;
  if (availH < 50 || availW < 40) return -1;
  int gridH = availH - kGuideBtnH - gap;
  int ch = (gridH - gap) / 2;
  int cw = (availW - gap) / 2;
  int guideY = y0 + gridH + gap;
  if (py >= guideY && py < guideY + kGuideBtnH && px >= gap && px < w - gap) {
    return 4;
  }
  if (px < gap || py < y0 || py >= guideY) return -1;
  int lx = (px - gap) / (cw + gap);
  int ly = (py - y0) / (ch + gap);
  if (lx < 0 || lx > 1 || ly < 0 || ly > 1) return -1;
  int ox = gap + lx * (cw + gap);
  int oy = y0 + ly * (ch + gap);
  if (px < ox || px >= ox + cw || py < oy || py >= oy + ch) return -1;
  return ly * 2 + lx;
}

static int settingsHitSectionRow(int px, int py, int w, int h) {
  (void)w;
  int n = settingsPanelRowCount(g_settingsPanel);
  if (n <= 0) return -1;
  constexpr int kRowH = 36;
  constexpr int kTop = 42;
  int fb = (g_settingsFeedback[0] != '\0') ? 20 : 4;
  int contentBottom = h - kBezelBarH - fb;
  int visible = (contentBottom - kTop) / kRowH;
  if (visible < 1) return -1;
  if (py < kTop || py >= kTop + visible * kRowH) return -1;
  int rel = (py - kTop) / kRowH + g_settingsListScroll;
  if (rel < 0 || rel >= n) return -1;
  return rel;
}

static const char* settingsConfirmTitle() {
  switch (g_settingsConfirmAction) {
    case SettingsConfirmAction::RestoreSd:
      return "Restore from SD?";
    case SettingsConfirmAction::ClearMidiDebug:
      return "Clear MIDI debug data?";
    default:
      return "";
  }
}

static const char* settingsRowTitle(uint8_t rid) {
  switch (rid) {
    case 0:
      return "MIDI out";
    case 1:
      return "MIDI in USB";
    case 18:
      return "MIDI in BLE";
    case 19:
      return "MIDI in DIN";
    case 2:
      return "Transport send";
    case 3:
      return "Transport recv";
    case 4:
      return "MIDI clock";
    case 37:
      return "MIDI thru src";
    case 17:
      return "Transpose";
    case 5:
      return "Brightness";
    case 6:
      return "Velocity";
    case 43:
      return "Velocity curve";
    case 38:
      return "Global swing";
    case 39:
      return "Global humanize";
    case 40:
      return "Settings hold";
    case 41:
      return "Seq clear hold";
    case 42:
      return "BPM hold";
    case 7:
      return "Build";
    case 8:
      return "Theme";
    case 9:
      return "Click volume";
    case 10:
      return "Arpeggiator";
    case 11:
      return "BPM";
    case 12:
      return "Project folder";
    case 13:
      return "Factory reset";
    case 14:
      return "Backup SD";
    case 15:
      return "Restore SD";
    case 16:
      return "Save & exit";
    case 20:
      return "MIDI debug";
    case 21:
      return "MIDI debug enabled";
    case 22:
      return "Follow MIDI clock";
    case 23:
      return "Clock flash edge";
    case 24:
      return "Clear MIDI debug";
    case 25:
      return "Play IN monitor";
    case 26:
      return "IN clock hold";
    case 27:
      return "IN monitor mode";
    case 28:
      return "BLE peak decay";
    case 29:
      return "Panic rotate";
    case 30:
      return "SUG lane window";
    case 31:
      return "SUG score gap";
    case 33:
      return "SUG profile";
    case 35:
      return "SUG profile lock";
    case 36:
      return "SUG unlock now";
    case 34:
      return "SUG flip A/B";
    case 32:
      return "SUG defaults";
    default:
      return "";
  }
}

static void settingsRowValueString(uint8_t rid, char* buf, size_t n) {
  switch (rid) {
    case 0:
      snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.midiOutChannel));
      break;
    case 1:
      if (g_settings.midiInChannelUsb == 0) {
        snprintf(buf, n, "OMNI");
      } else {
        snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.midiInChannelUsb));
      }
      break;
    case 18:
      if (g_settings.midiInChannelBle == 0) {
        snprintf(buf, n, "OMNI");
      } else {
        snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.midiInChannelBle));
      }
      break;
    case 19:
      if (g_settings.midiInChannelDin == 0) {
        snprintf(buf, n, "OMNI");
      } else {
        snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.midiInChannelDin));
      }
      break;
    case 2:
      snprintf(buf, n, "%s", midiTransportRouteLabel(g_settings.midiTransportSend));
      break;
    case 3:
      snprintf(buf, n, "%s", midiTransportRouteLabel(g_settings.midiTransportReceive));
      break;
    case 4:
      snprintf(buf, n, "%s", midiClockSourceLabel(g_settings.midiClockSource));
      break;
    case 37:
      switch (g_settings.midiThruMask & 0x07U) {
        case 0:
          snprintf(buf, n, "Off");
          break;
        case 1:
          snprintf(buf, n, "USB");
          break;
        case 2:
          snprintf(buf, n, "BLE");
          break;
        case 3:
          snprintf(buf, n, "USB+BLE");
          break;
        case 4:
          snprintf(buf, n, "DIN");
          break;
        case 5:
          snprintf(buf, n, "USB+DIN");
          break;
        case 6:
          snprintf(buf, n, "BLE+DIN");
          break;
        default:
          snprintf(buf, n, "All");
          break;
      }
      break;
    case 17: {
      const int v = static_cast<int>(g_settings.transposeSemitones);
      if (v > 0) {
        snprintf(buf, n, "+%d", v);
      } else {
        snprintf(buf, n, "%d", v);
      }
      break;
    }
    case 5:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(g_settings.brightnessPercent));
      break;
    case 6:
      snprintf(buf, n, "%u", static_cast<unsigned>(g_settings.outputVelocity));
      break;
    case 43:
      snprintf(buf, n, "%s", g_settings.velocityCurve == 0 ? "Linear" :
                             (g_settings.velocityCurve == 1 ? "Soft" : "Hard"));
      break;
    case 38:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(g_settings.globalSwingPct));
      break;
    case 39:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(g_settings.globalHumanizePct));
      break;
    case 40:
      snprintf(buf, n, "%s", g_settings.settingsEntryHoldPreset == 0
                               ? "0.5s"
                               : (g_settings.settingsEntryHoldPreset == 1
                                      ? "0.65s"
                                      : (g_settings.settingsEntryHoldPreset == 2
                                             ? "0.8s"
                                             : (g_settings.settingsEntryHoldPreset == 3 ? "1.0s" : "1.2s"))));
      break;
    case 41:
      snprintf(buf, n, "%s", g_settings.seqLongPressPreset == 0
                               ? "0.25s"
                               : (g_settings.seqLongPressPreset == 1
                                      ? "0.3s"
                                      : (g_settings.seqLongPressPreset == 2
                                             ? "0.4s"
                                             : (g_settings.seqLongPressPreset == 3 ? "0.5s" : "0.7s"))));
      break;
    case 42:
      snprintf(buf, n, "%s", g_settings.bpmHoldPreset == 0
                               ? "0.35s"
                               : (g_settings.bpmHoldPreset == 1
                                      ? "0.5s"
                                      : (g_settings.bpmHoldPreset == 2 ? "0.7s" : "0.9s")));
      break;
    case 7:
      snprintf(buf, n, "%s", M5CHORD_BUILD_STAMP);
      break;
    case 8:
      snprintf(buf, n, "%s", uiThemeName(g_uiTheme));
      break;
    case 9:
      snprintf(buf, n, "%u%%", static_cast<unsigned>(g_clickVolumePercent));
      break;
    case 10:
      snprintf(buf, n, "%s", arpeggiatorModeLabel(g_settings.arpeggiatorMode));
      break;
    case 20:
      if (!M5CHORD_ENABLE_MIDI_DEBUG_SCREEN) {
        snprintf(buf, n, "Build off");
      } else if (!g_settings.midiDebugEnabled) {
        snprintf(buf, n, "Disabled");
      } else {
        snprintf(buf, n, "Open");
      }
      break;
    case 21:
      snprintf(buf, n, "%s", g_settings.midiDebugEnabled ? "On" : "Off");
      break;
    case 22:
      snprintf(buf, n, "%s", g_settings.clkFollow ? "On" : "Off");
      break;
    case 23:
      snprintf(buf, n, "%s", g_settings.clkFlashEdge ? "1/8" : "1/4");
      break;
    case 24:
      snprintf(buf, n, "Run");
      break;
    case 32:
      snprintf(buf, n, "Apply");
      break;
    case 34:
      snprintf(buf, n, "Flip");
      break;
    case 36:
      snprintf(buf, n, "Run");
      break;
    case 33:
      snprintf(buf, n, "%s", g_settings.suggestProfile == 0 ? "A" : "B");
      break;
    case 35:
      snprintf(buf, n, "%s", g_settings.suggestProfileLock ? "On" : "Off");
      break;
    case 25:
      snprintf(buf, n, "%s", g_settings.playInMonitor ? "On" : "Off");
      break;
    case 26:
      snprintf(buf, n, "%s", g_settings.playInClockHold == 0 ? "Short" :
                             (g_settings.playInClockHold == 1 ? "Medium" : "Long"));
      break;
    case 27:
      snprintf(buf, n, "%s", g_settings.playInMonitorMode ? "Detailed" : "Compact");
      break;
    case 28:
      snprintf(buf, n, "%s", g_settings.bleDebugPeakDecay == 0 ? "Off" :
                             (g_settings.bleDebugPeakDecay == 1
                                  ? "5s"
                                  : (g_settings.bleDebugPeakDecay == 2 ? "10s" : "30s")));
      break;
    case 29:
      snprintf(buf, n, "%s", g_settings.panicDebugRotate == 0
                               ? "Off"
                               : (g_settings.panicDebugRotate == 1
                                      ? "1s"
                                      : (g_settings.panicDebugRotate == 2 ? "2s" : "4s")));
      break;
    case 30:
      snprintf(buf, n, "%s%s", g_settings.suggestStableWindow == 0
                                  ? "0.6s"
                                  : (g_settings.suggestStableWindow == 1
                                         ? "1.0s"
                                         : (g_settings.suggestStableWindow == 2
                                                ? "1.4s"
                                                : (g_settings.suggestStableWindow == 3 ? "2.0s" : "3.0s"))),
               g_settings.suggestProfileLock ? " [L]" : "");
      break;
    case 31:
      snprintf(buf, n, "%s%s", g_settings.suggestStableGap == 0
                                  ? "8"
                                  : (g_settings.suggestStableGap == 1
                                         ? "12"
                                         : (g_settings.suggestStableGap == 2
                                                ? "18"
                                                : (g_settings.suggestStableGap == 3 ? "24" : "32"))),
               g_settings.suggestProfileLock ? " [L]" : "");
      break;
    case 11:
      snprintf(buf, n, "%u", static_cast<unsigned>(g_projectBpm));
      break;
    case 12:
      if (g_projectCustomName[0] != '\0') {
        snprintf(buf, n, "%.12s", g_projectCustomName);
      } else {
        snprintf(buf, n, "(auto)");
      }
      break;
    case 13:
    case 14:
    case 15:
    case 16:
      buf[0] = '\0';
      break;
    default:
      buf[0] = '\0';
      break;
  }
}

static const char* const kUserGuideLines[] = {
    "=== Ring / settings ===",
    "BACK/FWD: Transport / Pad / Seq / XY.",
    "Settings: hold BACK + FWD ~0.8s.",
    "",
    "=== Play ===",
    "SELECT on key: voicing / edit. Hold SELECT: Shift.",
    "Surround = chords. Center = key / mode.",
    "",
    "=== Chord categories ===",
    "Tap a cell to choose a chord from the list.",
    "",
    "=== Sequencer ===",
    "Tap step: set chord. Hold: clear.",
    "Hold SELECT: Shift tools / step focus.",
    "Top row (Shift): step div, arp, pattern.",
    "",
    "=== X-Y MIDI ===",
    "Drag pad for CC. CH / CC / Curve: top row.",
    "CC 0-31: 14-bit pairs (MSB+LSB) when sent.",
    "SELECT + pad: REC to seq (if mode on) else LIVE.",
    "Release: Return center or Hold last (SELECT).",
    "",
    "=== Transport ===",
    "BPM tap/hold; step readout; MIDI/clock rows.",
    "Dropdowns: tap row. FWD/BACK scroll list.",
    "BACK or SEL closes the list.",
    "",
    "=== MIDI debug ===",
    "SEL: exit to Settings. BACK/FWD: other pages.",
    "",
};
static constexpr size_t kUserGuideLineCount = sizeof(kUserGuideLines) / sizeof(kUserGuideLines[0]);

static int userGuideVisibleLines(int h) {
  constexpr int kLineStep = 10;
  constexpr int kTopG = 42;
  int fb = (g_settingsFeedback[0] != '\0') ? 20 : 4;
  int contentBottom = h - kBezelBarH - fb - 16;
  return max(1, (contentBottom - kTopG) / kLineStep);
}

static int userGuideMaxScroll(int h) {
  return max(0, static_cast<int>(kUserGuideLineCount) - userGuideVisibleLines(h));
}

void drawSettingsUi() {
  const int w = M5.Display.width();
  const int h = M5.Display.height();
  layoutBottomBezels(w, h);
  M5.Display.startWrite();
  M5.Display.fillScreen(g_uiPalette.bg);

  M5.Display.setFont(nullptr);
  M5.Display.setTextColor(g_uiPalette.settingsHeader, g_uiPalette.bg);
  M5.Display.setTextDatum(top_center);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Settings", w / 2, 4);

  if (g_settingsPanel == SettingsPanel::Menu) {
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
    M5.Display.drawString("Choose a category", w / 2, 26);
    constexpr int kTop = 38;
    constexpr int gap = 8;
    constexpr int kGuideBtnH = 28;
    int y0 = kTop;
    int availH = h - kBezelBarH - y0 - gap;
    int availW = w - 2 * gap;
    int gridH = availH - kGuideBtnH - gap;
    int ch = (gridH - gap) / 2;
    int cw = (availW - gap) / 2;
    const char* labs[4] = {"MIDI", "Display", "Seq/Arp", "SD/Backup"};
    for (int i = 0; i < 4; ++i) {
      int lx = i % 2;
      int ly = i / 2;
      Rect r = {gap + lx * (cw + gap), y0 + ly * (ch + gap), cw, ch};
      const uint8_t tsize = (ly == 1) ? static_cast<uint8_t>(1) : static_cast<uint8_t>(2);
      drawRoundedButton(r, g_uiPalette.panelMuted, labs[i], tsize);
    }
    const int guideY = y0 + gridH + gap;
    Rect guideR = {gap, guideY, w - 2 * gap, kGuideBtnH};
    drawRoundedButton(guideR, g_uiPalette.panelMuted, "User guide", 2);
  } else if (g_settingsPanel == SettingsPanel::UserGuide) {
    const int mx = userGuideMaxScroll(h);
    if (g_userGuideScroll > mx) {
      g_userGuideScroll = mx;
    }
    if (g_userGuideScroll < 0) {
      g_userGuideScroll = 0;
    }
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
    M5.Display.drawString("User guide", w / 2, 26);
    constexpr int kLineStep = 10;
    constexpr int kTopG = 42;
    int fb = (g_settingsFeedback[0] != '\0') ? 20 : 4;
    int contentBottom = h - kBezelBarH - fb - 16;
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(1);
    int y = kTopG;
    for (size_t li = static_cast<size_t>(g_userGuideScroll); li < kUserGuideLineCount && y + kLineStep <= contentBottom;
         ++li) {
      const char* line = kUserGuideLines[li];
      if (line[0] == '\0') {
        y += kLineStep / 2;
        continue;
      }
      uint16_t col = g_uiPalette.rowNormal;
      if (line[0] == '=' && line[1] == '=') {
        col = g_uiPalette.accentPress;
      }
      M5.Display.setTextColor(col, g_uiPalette.bg);
      M5.Display.drawString(line, 6, y);
      y += kLineStep;
    }
    M5.Display.setTextDatum(top_center);
    M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
    M5.Display.drawString("FWD: more   BACK: up / menu", w / 2, h - kBezelBarH - 14);
  } else {
    const char* sec = "";
    switch (g_settingsPanel) {
      case SettingsPanel::Midi:
        sec = "MIDI";
        break;
      case SettingsPanel::Display:
        sec = "Display";
        break;
      case SettingsPanel::SeqArp:
        sec = "Seq/Arp";
        break;
      case SettingsPanel::Storage:
        sec = "SD/Backup";
        break;
      default:
        break;
    }
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
    M5.Display.drawString(sec, w / 2, 26);

    constexpr int kRowH = 36;
    constexpr int kTop = 42;
    int fb = (g_settingsFeedback[0] != '\0') ? 20 : 4;
    int contentBottom = h - kBezelBarH - fb;
    int n = settingsPanelRowCount(g_settingsPanel);
    settingsEnsureListScroll(n, kRowH, kTop, contentBottom);

    for (int vi = 0; vi < n; ++vi) {
      int si = vi - g_settingsListScroll;
      if (si < 0) continue;
      int y = kTop + si * kRowH;
      if (y + kRowH > contentBottom) break;
      uint8_t rid = settingsPanelRowId(g_settingsPanel, vi);
      bool sel = (vi == g_settingsCursorRow);
      M5.Display.setTextSize(3);
      M5.Display.setTextDatum(middle_left);
      M5.Display.setTextColor(sel ? g_uiPalette.rowSelect : g_uiPalette.rowNormal, g_uiPalette.bg);
      char val[40];
      settingsRowValueString(rid, val, sizeof(val));
      char line[48];
      snprintf(line, sizeof(line), "%s", settingsRowTitle(rid));
      M5.Display.drawString(line, 6, y + kRowH / 2);
      M5.Display.setTextDatum(middle_right);
      M5.Display.drawString(val, w - 6, y + kRowH / 2);
    }

    if (g_settingsPanel == SettingsPanel::Midi &&
        (g_settingsRow == 25 || g_settingsRow == 26 || g_settingsRow == 27)) {
      const int yHint = h - kBezelBarH - ((g_settingsFeedback[0] != '\0') ? 34 : 22);
      const bool detailedMode = (g_settings.playInMonitorMode != 0);
      M5.Display.setTextSize(1);
      M5.Display.setTextDatum(top_center);
      M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
      M5.Display.drawString("Monitor age color:", w / 2, yHint);

      const int ySwatch = yHint + 10;
      M5.Display.setTextDatum(top_left);
      M5.Display.setTextColor(detailedMode ? g_uiPalette.accentPress : g_uiPalette.subtle, g_uiPalette.bg);
      M5.Display.drawString("fresh<0.3s", 16, ySwatch);
      M5.Display.setTextColor(detailedMode ? g_uiPalette.rowNormal : g_uiPalette.subtle, g_uiPalette.bg);
      M5.Display.drawString("recent<1.2s", 112, ySwatch);
      M5.Display.setTextColor(g_uiPalette.subtle, g_uiPalette.bg);
      M5.Display.drawString("stale>=1.2s", 220, ySwatch);
      M5.Display.setTextDatum(top_right);
      M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
      M5.Display.drawString(detailedMode ? "[DETAILED]" : "[COMPACT]", w - 8, ySwatch);
    }

    if (g_settingsFeedback[0] != '\0') {
      M5.Display.setTextDatum(top_center);
      M5.Display.setTextSize(1);
      M5.Display.setTextColor(g_uiPalette.feedback, g_uiPalette.bg);
      M5.Display.drawString(g_settingsFeedback, w / 2, h - kBezelBarH - 18);
    }
  }

  if (s_settingsDropRowId >= 0) {
    int dx, dy, dww, rh, first, vis, tot;
    settingsComputeDropdownLayout(w, h, s_settingsDropRowId, &dx, &dy, &dww, &rh, &first, &vis,
                                  &tot);
    M5.Display.fillRect(dx - 2, dy - 2, dww + 4, vis * rh + 4, g_uiPalette.bg);
    M5.Display.drawRect(dx - 2, dy - 2, dww + 4, vis * rh + 4, g_uiPalette.settingsBtnBorder);
    for (int i = 0; i < vis; ++i) {
      int opt = first + i;
      if (opt >= tot) break;
      Rect dr = {dx, dy + i * rh, dww, rh};
      char lab[40];
      settingsFormatDropdownOption(s_settingsDropRowId, opt, lab, sizeof(lab));
      drawRoundedButton(dr, g_uiPalette.settingsBtnIdle, lab, 1);
    }
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_center);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.bg);
    if (tot > vis) {
      char hint[24];
      snprintf(hint, sizeof(hint), "FWD: more  BACK: up/close");
      M5.Display.drawString(hint, w / 2, max(4, dy - 14));
    }
  }

  if (g_settingsConfirmAction != SettingsConfirmAction::None) {
    const int boxW = min(w - 24, 250);
    const int boxH = 66;
    const int boxX = (w - boxW) / 2;
    const int boxY = (h - kBezelBarH - boxH) / 2;
    M5.Display.fillRoundRect(boxX, boxY, boxW, boxH, 8, g_uiPalette.panelMuted);
    M5.Display.drawRoundRect(boxX, boxY, boxW, boxH, 8, g_uiPalette.settingsBtnActive);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_center);
    M5.Display.setTextColor(g_uiPalette.rowNormal, g_uiPalette.panelMuted);
    M5.Display.drawString(settingsConfirmTitle(), w / 2, boxY + 10);
    M5.Display.setTextColor(g_uiPalette.hintText, g_uiPalette.panelMuted);
    M5.Display.drawString("BACK cancel  SELECT/FWD confirm", w / 2, boxY + 34);
  }

  drawTopSystemStatus(w, 2, nullptr, 0);
  drawBezelBarStrip();
  M5.Display.endWrite();
}

void processSettingsTouch(uint8_t touchCount, int w, int h) {
  layoutBottomBezels(w, h);

  if (touchCount > 0) {
    for (uint8_t i = 0; i < touchCount; ++i) {
      const auto& d = M5.Touch.getDetail(i);
      if (d.isPressed()) {
        g_lastTouchX = d.x;
        g_lastTouchY = d.y;
      }
    }
    if (!wasTouchActive) {
      s_settingsTouchStartX = g_lastTouchX;
      s_settingsTouchStartY = g_lastTouchY;
    }
    if (s_settingsDropRowId >= 0) {
      int ddx, ddy, dww, drh, dfirst, dvis, dtot;
      settingsComputeDropdownLayout(w, h, s_settingsDropRowId, &ddx, &ddy, &dww, &drh, &dfirst, &dvis,
                                    &dtot);
      const Rect listHit = {ddx, ddy, dww, dvis * drh};
      const auto& d0 = M5.Touch.getDetail(0);
      if (d0.isPressed() && pointInRect(d0.x, d0.y, listHit)) {
        if (s_settingsDropDragLastY >= 0) {
          const int delta = d0.y - s_settingsDropDragLastY;
          constexpr int kSettingsDropScrollSlopPx = 18;
          if (delta > kSettingsDropScrollSlopPx) {
            if (s_settingsDropOptScroll > 0) {
              --s_settingsDropOptScroll;
              ++s_settingsDropFingerScrollCount;
              drawSettingsUi();
            }
            s_settingsDropDragLastY = d0.y;
          } else if (delta < -kSettingsDropScrollSlopPx) {
            const int cnt = settingsDropdownOptionCount(s_settingsDropRowId);
            if (s_settingsDropOptScroll + dvis < cnt) {
              ++s_settingsDropOptScroll;
              ++s_settingsDropFingerScrollCount;
              drawSettingsUi();
            }
            s_settingsDropDragLastY = d0.y;
          }
        } else {
          s_settingsDropDragLastY = d0.y;
        }
      } else {
        s_settingsDropDragLastY = -1;
      }
    }
    wasTouchActive = true;
    return;
  }

  if (!wasTouchActive) return;
  wasTouchActive = false;
  const int fingerScrollsSettingsDrop = s_settingsDropFingerScrollCount;
  s_settingsDropFingerScrollCount = 0;
  s_settingsDropDragLastY = -1;

  const int hx = g_lastTouchX;
  const int hy = g_lastTouchY;

  if (g_settingsConfirmAction != SettingsConfirmAction::None) {
    if (pointInRect(hx, hy, g_bezelBack)) {
      g_settingsConfirmAction = SettingsConfirmAction::None;
      snprintf(g_settingsFeedback, sizeof(g_settingsFeedback), "Cancelled");
      drawSettingsUi();
      return;
    }
    if (pointInRect(hx, hy, g_bezelSelect) || pointInRect(hx, hy, g_bezelFwd)) {
      const SettingsConfirmAction action = g_settingsConfirmAction;
      g_settingsConfirmAction = SettingsConfirmAction::None;
      if (action == SettingsConfirmAction::RestoreSd) {
        settingsRunRestoreFlow();
      } else if (action == SettingsConfirmAction::ClearMidiDebug) {
        settingsRunClearMidiDebugData();
      }
      if (g_screen == Screen::Settings) {
        drawSettingsUi();
      }
      return;
    }
    drawSettingsUi();
    return;
  }

  if (pointInRect(hx, hy, g_bezelBack)) {
    if (s_settingsDropRowId >= 0) {
      if (s_settingsDropOptScroll > 0) {
        --s_settingsDropOptScroll;
        drawSettingsUi();
        return;
      }
      s_settingsDropRowId = -1;
      drawSettingsUi();
      return;
    }
    if (g_settingsPanel == SettingsPanel::UserGuide) {
      if (g_userGuideScroll > 0) {
        --g_userGuideScroll;
        drawSettingsUi();
        return;
      }
      g_factoryResetConfirmArmed = false;
      g_settingsPanel = SettingsPanel::Menu;
      g_userGuideScroll = 0;
      drawSettingsUi();
      return;
    }
    if (g_settingsPanel != SettingsPanel::Menu) {
      g_factoryResetConfirmArmed = false;
      g_settingsPanel = SettingsPanel::Menu;
      drawSettingsUi();
      return;
    }
    settingsSaveAndExit();
    return;
  }

  if (pointInRect(hx, hy, g_bezelFwd)) {
    if (s_settingsDropRowId >= 0) {
      int cnt = settingsDropdownOptionCount(s_settingsDropRowId);
      int vis = settingsDropVisibleRows(h);
      if (s_settingsDropOptScroll + vis < cnt) {
        ++s_settingsDropOptScroll;
        drawSettingsUi();
      }
      return;
    }
    if (g_settingsPanel == SettingsPanel::UserGuide) {
      const int mx = userGuideMaxScroll(h);
      if (g_userGuideScroll < mx) {
        ++g_userGuideScroll;
        drawSettingsUi();
      }
      return;
    }
    if (g_settingsPanel != SettingsPanel::Menu) {
      settingsMoveCursorInSection(1, h);
      drawSettingsUi();
    }
    return;
  }

  if (pointInRect(hx, hy, g_bezelSelect)) {
    if (s_settingsDropRowId >= 0) {
      drawSettingsUi();
      return;
    }
    if (g_settingsPanel == SettingsPanel::UserGuide) {
      drawSettingsUi();
      return;
    }
    if (g_settingsPanel != SettingsPanel::Menu) {
      settingsSyncRowFromCursor();
      settingsOpenDropdownOrAction(static_cast<uint8_t>(g_settingsRow), w, h);
      if (g_screen == Screen::Settings) {
        drawSettingsUi();
      }
    }
    return;
  }

  if (s_settingsDropRowId >= 0) {
    int pick = settingsHitDropdown(hx, hy, w, h);
    if (pick >= 0) {
      const bool movedPastThreshold =
          touchMovedPastSuppressThreshold(s_settingsTouchStartX, s_settingsTouchStartY, hx, hy);
      if (fingerScrollsSettingsDrop > 0 || movedPastThreshold) {
        drawSettingsUi();
        return;
      }
      settingsApplyDropdownPick(s_settingsDropRowId, pick);
      s_settingsDropRowId = -1;
    } else {
      s_settingsDropRowId = -1;
    }
    drawSettingsUi();
    return;
  }

  if (g_settingsPanel == SettingsPanel::Menu) {
    int cell = settingsHitMenuCell(hx, hy, w, h);
    if (cell == 0) {
      g_settingsPanel = SettingsPanel::Midi;
    } else if (cell == 1) {
      g_settingsPanel = SettingsPanel::Display;
    } else if (cell == 2) {
      g_settingsPanel = SettingsPanel::SeqArp;
    } else if (cell == 3) {
      g_settingsPanel = SettingsPanel::Storage;
    } else if (cell == 4) {
      g_settingsPanel = SettingsPanel::UserGuide;
      g_userGuideScroll = 0;
    } else {
      drawSettingsUi();
      return;
    }
    g_settingsCursorRow = 0;
    g_settingsListScroll = 0;
    g_factoryResetConfirmArmed = false;
    settingsSyncRowFromCursor();
    drawSettingsUi();
    return;
  }

  int rowHit = settingsHitSectionRow(hx, hy, w, h);
  if (rowHit >= 0) {
    g_settingsCursorRow = rowHit;
    settingsSyncRowFromCursor();
    settingsOpenDropdownOrAction(static_cast<uint8_t>(g_settingsRow), w, h);
    if (g_screen == Screen::Settings) {
      drawSettingsUi();
    }
    return;
  }

  drawSettingsUi();
}
