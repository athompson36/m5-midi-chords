#pragma once

#include "../ui/UiGloss.h"
#include "../ui/UiTypes.h"

namespace screens {

struct TransportScreenState {};

void drawTransportDrawer(ui::DisplayAdapter& d, const TransportScreenState& state);
bool processTransportDrawerTouch(TransportScreenState& state, int16_t x, int16_t y, bool pressed);

}  // namespace screens
