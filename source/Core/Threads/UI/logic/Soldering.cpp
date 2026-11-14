
#include "OperatingModes.h"
#include "SolderingCommon.h"
#include "ui_drawing.hpp"
// State 1 = button locking  (0:unlocked+released, 1:unlocked, 2:locked, 3:locked+released)
// State 2 = boost mode
// State 3 = buzzer timer

#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
void hs02StepSetTemperature(bool up, bool large); // HomeScreen.cpp
void hs02SaveSetTemperatureIfDue(bool now); // HomeScreen.cpp
#endif

OperatingMode handleSolderingButtons(const ButtonState buttons, guiContext *cxt) {
  if (cxt->scratch_state.state1 >= 2) {
    // Buttons are currently locked
    if (buttons == BUTTON_BOTH_LONG) {
      if (cxt->scratch_state.state1 == 3) {
        // Unlocking
        if (warnUser(translatedString(Tr->UnlockingKeysString), buttons)) {
          cxt->scratch_state.state1 = 1;
          cxt->scratch_state.state7 = 0;
        }
      } else {
        warnUser(translatedString(Tr->LockingKeysString), buttons);
      }
      return OperatingMode::Soldering;
    }
    if (cxt->scratch_state.state7 != 0) {
      // show locked until timer is up
      if (xTaskGetTickCount() >= cxt->scratch_state.state7) {
        cxt->scratch_state.state7 = 0;
      } else {
        warnUser(translatedString(Tr->WarningKeysLockedString), buttons);
        return OperatingMode::Soldering;
      }
    }
    switch (buttons) {
    case BUTTON_NONE:
      cxt->scratch_state.state1 = 3;
#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
      // BUTTON_OK_LONG is reported once per hold (Buttons.cpp), then BUTTON_NONE: boost goes on
      // for as long as the button itself is down.
      if (!(cxt->scratch_state.state2 && getButtonOK())) {
        cxt->scratch_state.state2 = 0;
      }
#else
      cxt->scratch_state.state2 = 0;
#endif
      break;
#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
    case BUTTON_OK_LONG: // card theme: boost is OK held
#else
    case BUTTON_F_LONG:
#endif
      if (getSettingValue(SettingsOptions::BoostTemp) && (getSettingValue(SettingsOptions::LockingMode) == lockingMode_t::BOOST)) {
        cxt->scratch_state.state2 = 1;
        break;
      }
    /*Fall through*/
    default: // Set timer for and display a lock warning
      cxt->scratch_state.state7 = xTaskGetTickCount() + TICKS_SECOND;
      warnUser(translatedString(Tr->WarningKeysLockedString), buttons);
      break;
    }
    return OperatingMode::Soldering;
  }

  // otherwise we are unlocked
#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
  // Card theme, to match the home screen:
  //   UP / DOWN short   target up / down by the "Temp change short" setting
  //   UP / DOWN held    target up / down by the "Temp change long" setting, repeating,
  //                     faster the longer it is held: the settings menu's pacing. The number
  //                     changes on this screen; no other screen opens.
  //   OK short          stop heating, back to the home screen
  //   OK held           boost for as long as it is held
  //   UP + DOWN held    lock the buttons, if locking is allowed
  hs02SaveSetTemperatureIfDue(false);
  static uint32_t repeatTimer = 0, repeatAcceleration = 0; // as in the card menu (SettingsMenu.cpp)
  if (buttons != BUTTON_B_LONG && buttons != BUTTON_F_LONG) {
    repeatTimer        = 0;
    repeatAcceleration = 0;
  }
  switch (buttons) {
  case BUTTON_NONE:
    // BUTTON_OK_LONG is reported once per hold (Buttons.cpp), then BUTTON_NONE: boost goes on
    // for as long as the button itself is down.
    if (!(cxt->scratch_state.state2 && getButtonOK())) {
      cxt->scratch_state.state2 = 0;
    }
    cxt->scratch_state.state1 = 0;
    break;
  case BUTTON_OK_SHORT:
  case BUTTON_BOTH:
    hs02SaveSetTemperatureIfDue(true);
    cxt->transitionMode = TransitionAnimation::None; // colour screen to colour screen: a hard cut
    return OperatingMode::HomeScreen;
  case BUTTON_OK_LONG:
    if (getSettingValue(SettingsOptions::BoostTemp)) {
      cxt->scratch_state.state2 = 1;
    }
    break;
  case BUTTON_B_SHORT:
  case BUTTON_F_SHORT:
    // UP is "+" unless "Swap + - keys" is on
    hs02StepSetTemperature((buttons == BUTTON_B_SHORT) != (getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled) != 0), false);
    break;
  case BUTTON_B_LONG:
  case BUTTON_F_LONG:
    if (xTaskGetTickCount() + repeatAcceleration > repeatTimer + PRESS_ACCEL_INTERVAL_MAX) {
      repeatTimer = xTaskGetTickCount();
      repeatAcceleration += PRESS_ACCEL_STEP;
      if ((PRESS_ACCEL_INTERVAL_MAX - repeatAcceleration) < PRESS_ACCEL_INTERVAL_MIN) {
        repeatAcceleration = PRESS_ACCEL_INTERVAL_MAX - PRESS_ACCEL_INTERVAL_MIN;
      }
      hs02StepSetTemperature((buttons == BUTTON_B_LONG) != (getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled) != 0), true);
    }
    break;
  case BUTTON_BOTH_LONG:
    if (getSettingValue(SettingsOptions::LockingMode)) {
      if (cxt->scratch_state.state1 == 0) {
        if (warnUser(translatedString(Tr->LockingKeysString), buttons)) {
          cxt->scratch_state.state1 = 2;
        }
      } else {
        warnUser(translatedString(Tr->UnlockingKeysString), buttons);
      }
    }
    break;
  default:
    break;
  }
  return OperatingMode::Soldering;
#endif
  switch (buttons) {
  case BUTTON_NONE:
    cxt->scratch_state.state2 = 0;
    cxt->scratch_state.state1 = 0;
    break;
  case BUTTON_OK_LONG: // Dedicated OK long-press returns to the home screen
  /*Fall through*/
  case BUTTON_BOTH:
  /*Fall through*/
  case BUTTON_B_LONG:
#if defined(LCD_160x80)
    // The colour gauge screen and the mono menu system share one physical LCD buffer (see
    // LCD.hpp) and can't slide-transition between each other; HomeScreen stays in the gauge
    // family too, so this is always a hard cut here.
    cxt->transitionMode = TransitionAnimation::None;
#else
  {
    bool detailedView   = getSettingValue(SettingsOptions::DetailedIDLE) && getSettingValue(SettingsOptions::DetailedSoldering);
    cxt->transitionMode = detailedView ? TransitionAnimation::None : TransitionAnimation::Right;
  }
#endif
    return OperatingMode::HomeScreen;
  case BUTTON_F_LONG:
    // if boost mode is enabled turn it on
    if (getSettingValue(SettingsOptions::BoostTemp)) {
      cxt->scratch_state.state2 = 1;
    }
    break;
  case BUTTON_OK_SHORT: // Dedicated OK button opens temperature adjust
  case BUTTON_F_SHORT:
  case BUTTON_B_SHORT:
#if defined(LCD_160x80)
    // TemperatureAdjust is a mono screen; hard cut, see the note above.
    cxt->transitionMode = TransitionAnimation::None;
#else
    cxt->transitionMode = TransitionAnimation::Left;
#endif
    return OperatingMode::TemperatureAdjust;
  case BUTTON_BOTH_LONG:
    if (getSettingValue(SettingsOptions::LockingMode)) {
      // Lock buttons
      if (cxt->scratch_state.state1 == 0) {
        if (warnUser(translatedString(Tr->LockingKeysString), buttons)) {
          cxt->scratch_state.state1 = 2;
        }
      } else {
        // FIXME should be WarningKeysUnlockedString
        warnUser(translatedString(Tr->UnlockingKeysString), buttons);
      }
    }
    break;
  default:
    break;
  }
  return OperatingMode::Soldering;
}

OperatingMode gui_solderingMode(const ButtonState buttons, guiContext *cxt) {
  /*
   * * Soldering (gui_solderingMode)
   * -> Main loop where we draw temp, and animations
   * --> User presses buttons and they goto the temperature adjust screen
   * ---> Display the current setpoint temperature
   * ---> Use buttons to change forward and back on temperature
   * ---> Both buttons or timeout for exiting
   * --> Long hold front button to enter boost mode
   * ---> Just temporarily sets the system into the alternate temperature for
   * PID control
   * --> Long hold back button to exit
   * --> Double button to exit
   * --> Long hold double button to toggle key lock
   */

  // Update the setpoints for the temperature
  if (cxt->scratch_state.state2) {
    if (getSettingValue(SettingsOptions::TemperatureInF)) {
      currentTempTargetDegC = TipThermoModel::convertFtoC(getSettingValue(SettingsOptions::BoostTemp));
    } else {
      currentTempTargetDegC = (getSettingValue(SettingsOptions::BoostTemp));
    }
  } else {
    if (getSettingValue(SettingsOptions::TemperatureInF)) {
      currentTempTargetDegC = TipThermoModel::convertFtoC(getSettingValue(SettingsOptions::SolderingTemp));
    } else {
      currentTempTargetDegC = (getSettingValue(SettingsOptions::SolderingTemp));
    }
  }

  // Update status
  int error = currentTempTargetDegC - TipThermoModel::getTipInC();
#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
  // Card theme: one beep when the tip first reaches the target after heating starts, and one
  // more only when a clearly higher target (20 C or more above the one it beeped for: boost, or
  // the set temperature turned well up) is reached. The reading drifting out of the +-10 C
  // window and back, as it does under load, does not beep again.
  // state5: has beeped; state6: the target it beeped for.
  if (error >= -10 && error <= 10) {
    if (!cxt->scratch_state.state5 || currentTempTargetDegC >= (TemperatureType_t)cxt->scratch_state.state6 + 20) {
      setBuzzer(true);
      cxt->scratch_state.state3 = xTaskGetTickCount() + TICKS_SECOND / 3;
      cxt->scratch_state.state5 = true;
      cxt->scratch_state.state6 = (uint16_t)currentTempTargetDegC;
    }
    setStatusLED(LED_HOT);
  } else {
    setStatusLED(LED_HEATING);
  }
#else
  if (error >= -10 && error <= 10) {
    // converged
    if (!cxt->scratch_state.state5) {
      setBuzzer(true);
      cxt->scratch_state.state3 = xTaskGetTickCount() + TICKS_SECOND / 3;
      cxt->scratch_state.state5 = true;
    }
    setStatusLED(LED_HOT);
  } else {
    setStatusLED(LED_HEATING);
    cxt->scratch_state.state5 = false;
  }
#endif
  if (cxt->scratch_state.state3 != 0 && xTaskGetTickCount() >= cxt->scratch_state.state3) {
    setBuzzer(false);
  }

  // Draw in the screen details
#if defined(LCD_160x80)
  ui_draw_home_gauge_soldering(cxt->scratch_state.state2);
#else
  if (getSettingValue(SettingsOptions::DetailedSoldering)) {
    ui_draw_soldering_power_status(cxt->scratch_state.state2);
  } else {
    ui_draw_soldering_basic_status(cxt->scratch_state.state2);
  }
#endif

#if !defined(LCD_160x80)
  bool detailedView = getSettingValue(SettingsOptions::DetailedIDLE) && getSettingValue(SettingsOptions::DetailedSoldering);
#endif
  // Check if we should bail due to undervoltage for example
  if (checkExitSoldering()) {
    setBuzzer(false);
#if defined(LCD_160x80)
    cxt->transitionMode = TransitionAnimation::None; // Staying within the colour gauge screen family.
#else
    cxt->transitionMode = detailedView ? TransitionAnimation::None : TransitionAnimation::Right;
#endif
    return OperatingMode::HomeScreen;
  }
#ifdef NO_SLEEP_MODE

  if (shouldShutdown()) {
    // shutdown
    currentTempTargetDegC = 0;
#if defined(LCD_160x80)
    cxt->transitionMode = TransitionAnimation::None;
#else
    cxt->transitionMode = detailedView ? TransitionAnimation::None : TransitionAnimation::Right;
#endif
    return OperatingMode::HomeScreen;
  }
#endif
  if (shouldBeSleeping()) {
    return OperatingMode::Sleeping;
  }

  if (heaterThermalRunawayCounter > 8) {
    currentTempTargetDegC       = 0; // heater control off
    heaterThermalRunawayCounter = 0;
#if defined(LCD_160x80)
    cxt->transitionMode = TransitionAnimation::None; // ThermalRunaway is a mono screen; hard cut, see above.
#else
    cxt->transitionMode = TransitionAnimation::Right;
#endif
    return OperatingMode::ThermalRunaway;
  }
  return handleSolderingButtons(buttons, cxt);
}
