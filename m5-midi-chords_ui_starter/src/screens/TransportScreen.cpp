#include "TransportScreen.h"

namespace screens {

void drawTransportDrawer(ui::DisplayAdapter& /*d*/, const TransportScreenState& /*state*/) {
  // TODO:
  // Move existing transport drawer/page visual presentation here,
  // while keeping transport timing logic in its current non-UI modules.
}

bool processTransportDrawerTouch(TransportScreenState& /*state*/, int16_t /*x*/, int16_t /*y*/, bool /*pressed*/) {
  // TODO:
  return false;
}

}  // namespace screens
