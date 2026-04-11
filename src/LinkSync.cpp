#include "LinkSync.h"

#include "AppSettings.h"

extern AppSettings g_settings;

void linkSyncInit() {
  if (g_settings.abletonLinkEnabled == 0) return;
  // Full Link integration needs the Ableton Link library, a stable clock, and WiFi STA — stub for now.
}

void linkSyncPoll(uint32_t /*nowMs*/) {
  if (g_settings.abletonLinkEnabled == 0) return;
}
