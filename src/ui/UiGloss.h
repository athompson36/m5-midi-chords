#pragma once

#include "UiTypes.h"

namespace ui {

/// Opaque display handle (M5GFX* on device; unused on native tests).
struct DisplayAdapter {
  void* gfx = nullptr;
  constexpr DisplayAdapter() = default;
  explicit constexpr DisplayAdapter(void* g) : gfx(g) {}
};

DisplayAdapter displayAdapterForM5();

GlossColors resolveGlossColors(GlossRole role, GlossState state);

void drawGlossButton(DisplayAdapter& d, const GlossButtonSpec& spec);
void drawDrawerSheet(DisplayAdapter& d, const DrawerSheetSpec& spec);
void drawStripShell(DisplayAdapter& d, const StripShellSpec& spec);
void drawValuePill(DisplayAdapter& d, const ValuePillSpec& spec);
void drawSliderTile(DisplayAdapter& d, const SliderTileSpec& spec);
void drawStatusBadge(DisplayAdapter& d, const StatusBadgeSpec& spec);

}  // namespace ui
