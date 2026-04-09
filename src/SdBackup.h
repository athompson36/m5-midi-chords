#pragma once

#include "AppSettings.h"
#include "ChordModel.h"
#include "SeqExtras.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// CoreS3 microSD (SPI). Call after M5.begin().
bool sdBackupInit();

bool sdBackupIsMounted();

/// Root folder on the SD card for all app backups.
#define SD_BACKUP_ROOT "/fs-chord-seq"

/// Sanitize a single path segment (folder name) in place: FAT-forbidden chars → '_'.
void sdBackupSanitizeFolderName(char* pathSegment);

/// Default folder name: date_key_mode_bpm (date omitted if RTC/time not set).
void sdBackupFormatDefaultProjectFolder(char* out, size_t cap, const ChordModel& m, uint16_t bpm);

/// `/fs-chord-seq/_global/global_settings.txt` — MIDI channels, brightness, velocity only.
bool sdBackupWriteGlobal(const AppSettings& s);
bool sdBackupReadGlobal(AppSettings& s);

/// `/fs-chord-seq/<project>/settings/project.txt` — key, mode, seq, XY CCs, BPM.
/// `projectFolderBasename` must be a single path segment (no slashes).
bool sdBackupWriteProject(const ChordModel& m, const uint8_t seqPattern[3][16], const uint8_t seqCh[3],
                          uint8_t xyChannel, uint8_t xyCcA, uint8_t xyCcB, uint8_t xyCurveA,
                          uint8_t xyCurveB, uint16_t bpm, uint8_t chordVoicing,
                          const SeqExtras* seqExtras, const char* projectFolderBasename);

bool sdBackupReadProject(ChordModel& m, uint8_t seqPattern[3][16], uint8_t seqCh[3], uint8_t* xyCcA,
                         uint8_t* xyCcB, uint8_t* xyChannel, uint8_t* xyCurveA, uint8_t* xyCurveB,
                         uint16_t* bpm, uint8_t* chordVoicing, SeqExtras* seqExtras,
                         const char* projectFolderBasename);

/// Lists first-level project directories under SD_BACKUP_ROOT (excludes `_global`). Returns false on I/O error.
bool sdBackupListProjects(char out[][48], int maxProjects, int* outCount);

/// Backwards-compatible: writes global + project, same paths as above.
bool sdBackupWriteAll(const AppSettings& s, const ChordModel& m, const uint8_t seqPattern[3][16],
                      const uint8_t seqCh[3], uint8_t xyChannel, uint8_t xyCcA, uint8_t xyCcB,
                      uint8_t xyCurveA, uint8_t xyCurveB, uint16_t bpm, uint8_t chordVoicing,
                      const SeqExtras* seqExtras,
                      const char* projectFolderBasename);

/// Restores global settings, then project from the given folder (or legacy single-file path — none).
bool sdBackupReadAll(AppSettings& s, ChordModel& m, uint8_t seqPattern[3][16], uint8_t seqCh[3],
                     uint8_t* xyCcA, uint8_t* xyCcB, uint8_t* xyChannel, uint8_t* xyCurveA,
                     uint8_t* xyCurveB, uint16_t* bpm, uint8_t* chordVoicing, SeqExtras* seqExtras,
                     const char* projectFolderBasename);
