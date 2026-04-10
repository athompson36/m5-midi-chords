#pragma once

#include <stdint.h>
#include <stddef.h>

namespace ui {

struct Rect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;

  constexpr Rect() = default;
  constexpr Rect(int xa, int ya, int wa, int ha) : x(xa), y(ya), w(wa), h(ha) {}

  constexpr int right() const { return x + w; }
  constexpr int bottom() const { return y + h; }

  constexpr bool contains(int px, int py) const {
    return px >= x && px < right() && py >= y && py < bottom();
  }
};

enum class GlossRole : uint8_t {
  Neutral,
  Primary,
  Accent,
  Muted,
  Danger,
  Selected
};

enum class GlossState : uint8_t {
  Idle,
  Focused,
  Active,
  Pressed,
  Disabled
};

enum class TopDrawerKind : uint8_t {
  None,
  Play,
  Sequencer,
  Xy
};

struct GlossColors {
  uint16_t base = 0;
  uint16_t topBand = 0;
  uint16_t bottomBand = 0;
  uint16_t borderTop = 0;
  uint16_t borderBottom = 0;
  uint16_t innerHighlight = 0;
  uint16_t ring = 0;
  uint16_t text = 0;
};

struct GlossButtonSpec {
  Rect bounds;
  const char* label = "";
  GlossRole role = GlossRole::Neutral;
  GlossState state = GlossState::Idle;
  uint8_t radius = 10;
  uint8_t textSize = 2;
  bool centered = true;
  /// If non-zero, use this RGB565 as base (legacy `drawRoundedButton` bridge).
  uint16_t customBase565 = 0;
};

struct DrawerSheetSpec {
  Rect bounds;
  const char* title = "";
  GlossRole role = GlossRole::Muted;
  uint8_t radius = 14;
  bool showHandle = true;
};

struct StripShellSpec {
  Rect bounds;
  GlossRole role = GlossRole::Muted;
  bool focused = false;
  uint8_t radius = 10;
};

struct ValuePillSpec {
  Rect bounds;
  const char* label = "";
  const char* value = "";
  GlossRole role = GlossRole::Neutral;
  GlossState state = GlossState::Idle;
  uint8_t radius = 8;
  uint8_t textSize = 1;
};

struct SliderTileSpec {
  Rect bounds;
  const char* label = "";
  uint8_t value = 0;       // 0..100
  const char* valueText = "";
  GlossRole role = GlossRole::Neutral;
  GlossState state = GlossState::Idle;
  uint8_t radius = 10;
  uint8_t textSize = 1;
};

struct StatusBadgeSpec {
  Rect bounds;
  const char* text = "";
  GlossRole role = GlossRole::Muted;
  GlossState state = GlossState::Idle;
  uint8_t radius = 6;
  uint8_t textSize = 1;
};

struct TopDrawerState {
  TopDrawerKind kind = TopDrawerKind::None;
  bool ignoreUntilTouchUp = false;
  bool swipeTracking = false;
  int16_t swipeStartY = 0;
  uint8_t subpage = 0;
};

struct LayoutMetrics {
  static constexpr int ScreenW = 320;
  static constexpr int ScreenH = 240;
  static constexpr int StatusH = 24;
  static constexpr int BezelH = 20;
  static constexpr int OuterMargin = 6;
  static constexpr int DrawerPadding = 6;
  static constexpr int StripGap = 6;
  static constexpr int ControlGap = 6;
};

}  // namespace ui
