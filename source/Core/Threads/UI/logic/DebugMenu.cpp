#include "OperatingModes.h"
#include "ui_drawing.hpp"

OperatingMode showDebugMenu(const ButtonState buttons, guiContext *cxt) {

#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  // Card theme: start on the three HS-02 pages (tip calibration, current sense, live sensors)
  // and carry on with the standard ones, instead of having them 16 presses away.
  ui_draw_debug_menu((cxt->scratch_state.state1 + 16) % 19);
#else
  ui_draw_debug_menu(cxt->scratch_state.state1);
#endif

  if (buttons == BUTTON_B_SHORT) {
    cxt->transitionMode = TransitionAnimation::Up;
    return OperatingMode::HomeScreen;
  } else if (buttons == BUTTON_F_SHORT) {
    cxt->scratch_state.state1++;
#if defined(LCD_160x80)
    // Item 16: HS-02 factory tip-cal readout, item 17: heater current sense, item 18: live sensors
    cxt->scratch_state.state1 = cxt->scratch_state.state1 % 19;
#elif defined(HALL_SENSOR)
    // Item 16: Hall sensor reading
    cxt->scratch_state.state1 = cxt->scratch_state.state1 % 17;
#else
    cxt->scratch_state.state1 = cxt->scratch_state.state1 % 16;
#endif
  }
  return OperatingMode::DebugMenuReadout; // Stay in debug menu
}
