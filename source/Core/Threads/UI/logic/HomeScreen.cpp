
#include "Buttons.hpp"
#include "OperatingModes.h"
#include "ui_drawing.hpp"

bool showExitMenuTransition = false;

#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
// Easter egg, see utils/Hs02Tune.cpp
void    hs02TuneStart(uint8_t tune);
uint8_t hs02TuneCount(void);
void hs02TuneStop(void);
bool hs02TunePlaying(void);

extern bool hs02MenuWaitForRelease; // SettingsMenu.cpp

// Card theme home screen buttons:
//   UP / DOWN short   set temperature up / down by the "Temp change short" setting
//   UP held           settings menu
//   DOWN held         set temperature down by the "Temp change long" setting, repeating,
//                     faster the longer it is held: the settings menu's pacing. The number
//                     changes on this screen. (The full-screen temperature screen of the port
//                     is not opened from anywhere in this theme.)
//   OK                start soldering
// A changed set temperature is written to flash two seconds after the last press, or when
// leaving the screen, so that stepping through values does not wear the flash.
// The heating screen (Soldering.cpp) steps the target with the same two functions.
static TickType_t hs02SetTempSaveAt = 0;

// large: the step of a held button (the "Temp change long" setting) instead of that of a press
// (the "Temp change short" setting). Both are used as set; this board's defaults are 5 and 10.
void hs02StepSetTemperature(bool up, bool large) {
  const bool fahrenheit = getSettingValue(SettingsOptions::TemperatureInF);
  int16_t    step       = getSettingValue(large ? SettingsOptions::TempChangeLongStep : SettingsOptions::TempChangeShortStep);
  if (step < 1) {
    step = 1; // the settings cannot be 0, but a division follows
  }
  const int16_t lowest  = fahrenheit ? MIN_TEMP_F : MIN_TEMP_C;
  const int16_t highest = fahrenheit ? MAX_TEMP_F : MAX_TEMP_C;
  int16_t       target  = getSettingValue(SettingsOptions::SolderingTemp);
  // to the next multiple of the step in that direction (321 with step 5: up 325, down 320)
  target = up ? (target / step + 1) * step : ((target + step - 1) / step - 1) * step;
  if (target < lowest) {
    target = lowest;
  } else if (target > highest) {
    target = highest;
  }
  setSettingValue(SettingsOptions::SolderingTemp, (uint16_t)target);
  hs02SetTempSaveAt = xTaskGetTickCount() + 2 * TICKS_SECOND;
}

void hs02SaveSetTemperatureIfDue(bool now) {
  if (hs02SetTempSaveAt != 0 && (now || (int32_t)(xTaskGetTickCount() - hs02SetTempSaveAt) >= 0)) {
    hs02SetTempSaveAt = 0;
    saveSettings();
  }
}
#endif

OperatingMode handleHomeButtons(const ButtonState buttons, guiContext *cxt) {
  if (buttons != BUTTON_NONE && cxt->scratch_state.state1 == 0) {
    return OperatingMode::HomeScreen; // Ignore button press
  } else {
    cxt->scratch_state.state1 = 1;
  }
#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
  // UP and DOWN held together for five seconds: a tune. Nothing happens before that, so a brief
  // press of both does not set it off. Each time it is the next of the tunes, starting with
  // the first after power-on.
  // (BUTTON_BOTH_LONG is first reported 400 ms into the hold, hence 4.6 s from then.)
  static TickType_t bothHeldSince = 0;
  static bool       bothFired     = false;
  static uint8_t    nextTune      = 0;
  if (buttons == BUTTON_BOTH_LONG) {
    const TickType_t now = xTaskGetTickCount();
    if (bothHeldSince == 0) {
      bothHeldSince = now | 1;
      bothFired     = false;
    } else if (!bothFired && (now - bothHeldSince) >= 5 * TICKS_SECOND - 4 * TICKS_100MS) {
      bothFired = true;
      hs02TuneStart(nextTune);
      nextTune = (uint8_t)((nextTune + 1) % hs02TuneCount());
    }
    return OperatingMode::HomeScreen;
  }
  bothHeldSince = 0;
  if (buttons != BUTTON_NONE && hs02TunePlaying()) { // any press stops the tune, and does nothing else
    hs02TuneStop();
    return OperatingMode::HomeScreen;
  }
  hs02SaveSetTemperatureIfDue(false);
  static uint32_t repeatTimer = 0, repeatAcceleration = 0; // as in the card menu (SettingsMenu.cpp)
  if (buttons != BUTTON_F_LONG) {
    repeatTimer        = 0;
    repeatAcceleration = 0;
  } else { // DOWN held: it is "-" unless "Swap + - keys" is on
    if (xTaskGetTickCount() + repeatAcceleration > repeatTimer + PRESS_ACCEL_INTERVAL_MAX) {
      repeatTimer = xTaskGetTickCount();
      repeatAcceleration += PRESS_ACCEL_STEP;
      if ((PRESS_ACCEL_INTERVAL_MAX - repeatAcceleration) < PRESS_ACCEL_INTERVAL_MIN) {
        repeatAcceleration = PRESS_ACCEL_INTERVAL_MAX - PRESS_ACCEL_INTERVAL_MIN;
      }
      hs02StepSetTemperature(getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled) != 0, true);
    }
    return OperatingMode::HomeScreen;
  }
  if (buttons == BUTTON_B_SHORT || buttons == BUTTON_F_SHORT) {
    // UP is "+" unless "Swap + - keys" is on, as on the temperature screen
    hs02StepSetTemperature((buttons == BUTTON_B_SHORT) != (getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled) != 0), false);
    return OperatingMode::HomeScreen;
  }
  if (buttons == BUTTON_B_LONG) { // UP held: settings menu
    hs02SaveSetTemperatureIfDue(true);
    hs02MenuWaitForRelease = true; // the button is still down when the menu takes over
    cxt->transitionMode    = TransitionAnimation::None;
    return OperatingMode::SettingsMenu;
  }
  if (buttons != BUTTON_NONE) {
    hs02SaveSetTemperatureIfDue(true); // about to leave for soldering
  }
#endif
  switch (buttons) {
  case BUTTON_NONE:
    // Do nothing
    break;
  case BUTTON_BOTH:
    break;

  case BUTTON_B_LONG: // (card theme: handled above, it opens the settings menu)
#if defined(LCD_160x80)
    // The colour gauge screen and the mono menu system share LCD buffer storage (see LCD.hpp).
    // Their slide transition routines interpret frames differently, so every crossing between
    // the two representations uses a hard cut.
    cxt->transitionMode = TransitionAnimation::None;
#else
    cxt->transitionMode = TransitionAnimation::Down;
#endif
    return OperatingMode::DebugMenuReadout;
    break;
  case BUTTON_F_LONG:
#ifdef PROFILE_SUPPORT
    if (!isTipDisconnected()) {
#if defined(LCD_160x80)
      cxt->transitionMode = TransitionAnimation::None;
#else
      cxt->transitionMode = TransitionAnimation::Left;
#endif
      return OperatingMode::SolderingProfile;
    } else {
      return OperatingMode::HomeScreen;
    }
#else
#if defined(LCD_160x80)
    cxt->transitionMode = TransitionAnimation::None;
#else
    cxt->transitionMode = TransitionAnimation::Left;
#endif
    return OperatingMode::TemperatureAdjust;
#endif
    break;
  case BUTTON_OK_SHORT: // Dedicated OK button enters soldering (same as front-press)
  case BUTTON_F_SHORT:
    if (!isTipDisconnected()) {
#if defined(LCD_160x80)
      cxt->transitionMode = TransitionAnimation::None; // Staying within the colour gauge screen family.
#else
      bool detailedView   = getSettingValue(SettingsOptions::DetailedIDLE) && getSettingValue(SettingsOptions::DetailedSoldering);
      cxt->transitionMode = detailedView ? TransitionAnimation::None : TransitionAnimation::Left;
#endif
      return OperatingMode::Soldering;
    }
    break;
  case BUTTON_B_SHORT:
#if defined(LCD_160x80)
    cxt->transitionMode = TransitionAnimation::None;
#else
    cxt->transitionMode = TransitionAnimation::Right;
#endif
    return OperatingMode::SettingsMenu;
    break;
  default:
    break;
  }
  return OperatingMode::HomeScreen;
}

OperatingMode drawHomeScreen(const ButtonState buttons, guiContext *cxt) {

  currentTempTargetDegC = 0; // ensure tip is off
  getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  uint32_t tipTemp = TipThermoModel::getTipInC();

#if defined(LCD_160x80)
  ui_draw_home_gauge_idle(tipTemp);
#else
  // Setup LCD Cursor location
  if (Display::getRotation()) {
    Display::setCursor(50, 0);
  } else {
    Display::setCursor(-1, 0);
  }
  if (getSettingValue(SettingsOptions::DetailedIDLE)) {
    ui_draw_homescreen_detailed(tipTemp);
  } else {
    ui_draw_homescreen_simplified(tipTemp);
  }
#endif
  return handleHomeButtons(buttons, cxt);
}
