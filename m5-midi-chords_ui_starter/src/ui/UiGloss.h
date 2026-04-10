#pragma once

#include "UiTypes.h"

namespace ui {

// Forward-declared adapter hook.
// Replace these with repo-specific wrappers around M5GFX / M5.Display.
struct DisplayAdapter {
  void* user = nullptr;
};

GlossColors resolveGlossColors(GlossRole role, GlossState state);

// Primitive renderers.
// These are intentionally backend-agnostic stubs that you should wire to M5.Display.
void drawGlossButton(DisplayAdapter& d, const GlossButtonSpec& spec);
void drawDrawerSheet(DisplayAdapter& d, const DrawerSheetSpec& spec);
void drawStripShell(DisplayAdapter& d, const StripShellSpec& spec);
void drawValuePill(DisplayAdapter& d, const ValuePillSpec& spec);
void drawSliderTile(DisplayAdapter& d, const SliderTileSpec& spec);
void drawStatusBadge(DisplayAdapter& d, const StatusBadgeSpec& spec);

}  // namespace ui
