/*
 * GUIThread.cpp
 *
 *  Created on: 19 Aug 2019
 *      Author: ralim
 */
extern "C" {
#include "FreeRTOSConfig.h"
}
#include "BootLogo.h"
#include "Buttons.hpp"
#include "Display.hpp"
#include "I2CBB2.hpp"
#include "LIS2DH12.hpp"
#include "MMA8652FC.hpp"
#include "OperatingModeUtilities.h"
#include "OperatingModes.h"
#include "Settings.h"
#include "TipThermoModel.h"
#include "Translation.h"
#include "cmsis_os.h"
#include "configuration.h"
#include "history.hpp"
#include "main.hpp"
#include "power.hpp"
#include "settingsGUI.hpp"
#include "stdlib.h"
#include "string.h"
#include "ui_drawing.hpp"
#ifdef POW_PD
#include "USBPD.h"
#include "pd.h"
#endif

// File local variables
#define MOVEMENT_INACTIVITY_TIME (60 * configTICK_RATE_HZ)
#define BUTTON_INACTIVITY_TIME   (60 * configTICK_RATE_HZ)

ButtonState   buttonsAtDeviceBoot;                                      // We record button state at startup, incase of jumping to debug modes
OperatingMode currentOperatingMode = OperatingMode::InitialisationDone; // Current mode we are rendering
guiContext    context;                                                  // Context passed to functions to aid in state during render passes

OperatingMode handle_post_init_state();

#if defined(LCD_160x80)
// The HS-02 colour home/soldering/sleep screens share one physical LCD buffer with the mono menu
// system (see LCD.hpp) rather than paying for two buffers -- pick the right interpretation for
// whichever mode is about to draw this frame, once, in this single dispatch point.
static bool isColorScreenMode(OperatingMode mode) {
  switch (mode) {
  case OperatingMode::HomeScreen:
  case OperatingMode::Soldering:
  case OperatingMode::Sleeping:
  case OperatingMode::Hibernating:
    return true;
  default:
    return false;
  }
}
#endif

#ifdef HS02_THEME_CARD
uint16_t hs02CardAccent(SettingsOptions setting, uint16_t ownColour); // draw_home_gauge.cpp
#endif

OperatingMode guiHandleDraw(void) {
#if defined(LCD_160x80)
#ifdef HS02_THEME_CARD
  LCD::setMonoAccentColour(hs02CardAccent(SettingsOptions::ColourMenu, 0x3DFF)); // menus: blue unless a menu colour is chosen
  LCD::setMonoRegions(0, 0, 255, 255); // all white unless the screen drawn below asks otherwise
#endif
  const bool colorScreen = isColorScreenMode(currentOperatingMode);
  Display::setColorMode(colorScreen);
  if (colorScreen) {
    Display::clearScreenColor();
  } else {
    Display::clearScreen();
  }
#else
  Display::clearScreen(); // Clear ready for render pass
#endif
  // Read button state
  ButtonState buttons = getButtonState();
  // Enforce screen on if buttons pressed, movement, hot tip etc
  if (buttons != BUTTON_NONE) {
    Display::setDisplayState(Display::DisplayState::ON);
  } else {
    // Buttons are none; check if we can sleep display
    uint32_t tipTemp = TipThermoModel::getTipInC();
    if ((tipTemp < 50) && getSettingValue(SettingsOptions::Sensitivity) &&
        (((xTaskGetTickCount() - lastMovementTime) > MOVEMENT_INACTIVITY_TIME) && ((xTaskGetTickCount() - lastButtonTime) > BUTTON_INACTIVITY_TIME))) {
      Display::setDisplayState(Display::DisplayState::OFF);
      setStatusLED(LED_OFF);
    } else {
      Display::setDisplayState(Display::DisplayState::ON);
    }
    if (currentOperatingMode != OperatingMode::Soldering && currentOperatingMode != OperatingMode::SolderingProfile && currentOperatingMode != OperatingMode::Hs02Maze) {
      // Not in soldering mode, so set this based on temp (the first-person game drives the LED itself)
      if (tipTemp > 55) {
        setStatusLED(LED_COOLING_STILL_HOT);
      } else {
        setStatusLED(LED_STANDBY);
      }
    }
  }
  // Dispatch button state to gui mode
  OperatingMode newMode = currentOperatingMode;
  switch (currentOperatingMode) {
  case OperatingMode::StartupWarnings:
    newMode = showWarnings(buttons, &context);
    break;
  case OperatingMode::UsbPDDebug:
#ifdef HAS_POWER_DEBUG_MENU
    newMode = showPDDebug(buttons, &context);
    break;
#else
    newMode = OperatingMode::InitialisationDone;
#endif
  case OperatingMode::StartupLogo:
    showBootLogo();

    if (getSettingValue(SettingsOptions::AutoStartMode) == autoStartMode_t::SLEEP) {
      lastMovementTime = lastButtonTime = 0; // We mask the values so that sleep goes until user moves again or presses a button
      newMode                           = OperatingMode::Sleeping;
    } else if (getSettingValue(SettingsOptions::AutoStartMode) == autoStartMode_t::SOLDER) {
      lastMovementTime = lastButtonTime = xTaskGetTickCount(); // Move forward so we dont go to sleep
      newMode                           = OperatingMode::Soldering;
    } else if (getSettingValue(SettingsOptions::AutoStartMode) == autoStartMode_t::ZERO) {
      lastMovementTime = lastButtonTime = 0; // We mask the values so that sleep goes until user moves again or presses a button
      newMode                           = OperatingMode::Hibernating;
    } else {
      newMode = OperatingMode::HomeScreen;
    }

    break;
  default:
    /* Fallthrough */
  case OperatingMode::HomeScreen:
    newMode = drawHomeScreen(buttons, &context);
    break;
  case OperatingMode::Soldering:
    context.scratch_state.state4 = 0;
    newMode                      = gui_solderingMode(buttons, &context);
    break;
  case OperatingMode::SolderingProfile:
    newMode = gui_solderingProfileMode(buttons, &context);
    break;
  case OperatingMode::Sleeping:
    newMode = gui_SolderingSleepingMode(buttons, &context);
    break;
  case OperatingMode::TemperatureAdjust:
    newMode = gui_solderingTempAdjust(buttons, &context);
    break;
  case OperatingMode::DebugMenuReadout:
    newMode = showDebugMenu(buttons, &context);
    break;
#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
  case OperatingMode::Hs02Blocks:
    newMode = hs02BlocksFrame(buttons, &context);
    break;
  case OperatingMode::Hs02Maze:
    newMode = hs02MazeFrame(buttons, &context);
    break;
#endif
  case OperatingMode::CJCCalibration:
    newMode = performCJCC(buttons, &context);
    break;
  case OperatingMode::SettingsMenu:
    newMode = gui_SettingsMenu(buttons, &context);
    break;
  case OperatingMode::InitialisationDone:
    newMode = handle_post_init_state();
    break;
  case OperatingMode::Hibernating:
    context.scratch_state.state4 = 1;
    gui_SolderingSleepingMode(buttons, &context);
    if (lastButtonTime > 0 || lastMovementTime > 0) {
      newMode = OperatingMode::Soldering;
    }
    break;
  case OperatingMode::ThermalRunaway:
    /*TODO*/
    newMode = OperatingMode::HomeScreen;
    break;
  };
  return newMode;
}
#ifdef MODEL_HS02
void hs02CurrentSenseSample(void); // Core/BSP/Fnirsi/BSP.cpp, read-only statistics for debug page 18
void hs02ApplyButtonLight(void);   // Core/BSP/Fnirsi/BSP.cpp
#endif
#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
void hs02TuneTick(void); // UI/logic/utils/Hs02Tune.cpp
void hs02TuneStop(void);
void hs02OverrideButtonLight(uint8_t level); // Core/BSP/Fnirsi/BSP.cpp
#endif

void guiRenderLoop(void) {
#ifdef MODEL_HS02
  hs02CurrentSenseSample();
  hs02ApplyButtonLight();
#endif
#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
  if (currentOperatingMode != OperatingMode::HomeScreen && currentOperatingMode != OperatingMode::Hs02Blocks) {
    hs02TuneStop(); // a tune only plays on the home screen and in the block game
  }
  if (currentOperatingMode != OperatingMode::Hs02Maze) {
    hs02OverrideButtonLight(0xFF); // only that game flashes the button light
  }
  hs02TuneTick();
#endif
#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
  // (true for the two games as well: they paint on the panel themselves, and the frame buffer,
  // which they leave empty, must not be sent over it)
  const bool nativeStartupLogo = currentOperatingMode == OperatingMode::StartupLogo || currentOperatingMode == OperatingMode::Hs02Blocks
                              || currentOperatingMode == OperatingMode::Hs02Maze;
#else
  const bool    nativeStartupLogo = currentOperatingMode == OperatingMode::StartupLogo;
#endif
  OperatingMode newMode           = guiHandleDraw(); // This does the screen drawing

  // Post draw we handle any state transitions

  if (newMode != currentOperatingMode) {
#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
    if (currentOperatingMode == OperatingMode::Soldering) {
      setBuzzer(false); // a beep that started just before leaving would otherwise never be ended
    }
#endif
    context.viewEnterTime = xTaskGetTickCount();
    context.previousMode  = currentOperatingMode;
    // If the previous mode is the startup logo; we dont want to return to it, but instead dispatch out to either home or soldering
    if (currentOperatingMode == OperatingMode::StartupLogo) {
      if (getSettingValue(SettingsOptions::AutoStartMode)) {
        context.previousMode = OperatingMode::Soldering;
      } else {
        newMode = OperatingMode::HomeScreen;
      }
    }
#if defined(LCD_160x80)
    // Menus use the legacy 1bpp buffer while home/soldering/sleep use the
    // 2bpp colour interpretation. The transition routines only understand
    // two 1bpp framebuffers, so never animate across that representation
    // boundary (for example SettingsMenu -> HomeScreen).
    if (isColorScreenMode(currentOperatingMode) != isColorScreenMode(newMode)) {
      context.transitionMode = TransitionAnimation::None;
    }
#endif
    memset(&context.scratch_state, 0, sizeof(context.scratch_state));
    currentOperatingMode = newMode;
  }

  // If the transition marker is set, we need to make the next draw occur to the secondary buffer so we have something to transition to
  if (context.transitionMode != TransitionAnimation::None) {
    Display::useSecondaryFramebuffer(true);
    // Now we need to fill the secondary buffer with the _next_ frame to transistion to
    guiHandleDraw();
    Display::useSecondaryFramebuffer(false);
    // Now dispatch the transition
    switch (context.transitionMode) {
    case TransitionAnimation::Down:
      Display::transitionScrollDown(context.viewEnterTime);
      break;
    case TransitionAnimation::Left:
      Display::transitionSecondaryFramebuffer(false, context.viewEnterTime);
      break;
    case TransitionAnimation::Right:
      Display::transitionSecondaryFramebuffer(true, context.viewEnterTime);
      break;
    case TransitionAnimation::Up:
      Display::transitionScrollUp(context.viewEnterTime);

    case TransitionAnimation::None:
    default:
      break; // Do nothing on unknown
    }

    context.transitionMode = TransitionAnimation::None; // Clear transition flag
  }
  // The native boot logo bypasses the framebuffer. Do not overwrite it with
  // the cleared framebuffer before the following home-screen render.
  if (!nativeStartupLogo) {
    Display::refresh();
  }
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  // The card theme shows no boot logo, which is what normally switches the backlight on.
  // Do it here instead, after the first home / soldering / sleep frame has been sent.
  static bool backlightRevealed = false;
  if (!backlightRevealed && !nativeStartupLogo && isColorScreenMode(currentOperatingMode)) {
    Display::setBrightness(getSettingValue(SettingsOptions::DisplayBrightness));
    backlightRevealed = true;
  }
#endif

#if defined(LCD_160x80)
  // Startup warnings are rendered into the framebuffer. Reveal the panel only
  // after that completed frame is on the LCD, never while it contains a blank
  // initialization frame.
  if (currentOperatingMode == OperatingMode::StartupWarnings && context.scratch_state.state7) {
    Display::setBrightness(getSettingValue(SettingsOptions::DisplayBrightness));
    context.scratch_state.state7 = 0;
  }
#endif
}

OperatingMode handle_post_init_state() {
#ifdef HAS_POWER_DEBUG_MENU
#ifdef DEBUG_POWER_MENU_BUTTON_B
  if (buttonsAtDeviceBoot == BUTTON_B_LONG || buttonsAtDeviceBoot == BUTTON_B_SHORT) {
#else
  if (buttonsAtDeviceBoot == BUTTON_F_LONG || buttonsAtDeviceBoot == BUTTON_F_SHORT) {
#endif
    buttonsAtDeviceBoot = BUTTON_NONE;
    return OperatingMode::UsbPDDebug;
  }
#endif

  if (getSettingValue(SettingsOptions::CalibrateCJC) > 0) {
    return OperatingMode::CJCCalibration;
  }

  return OperatingMode::StartupWarnings;
}

/* StartGUITask function */
void startGUITask(void const *argument) {
  (void)argument;
  prepareTranslations();

#if defined(LCD_160x80)
  Display::setBrightness(0);
#endif
  Display::initialize(); // start up the LCD
  Display::setInverseDisplay(getSettingValue(SettingsOptions::DisplayInversion));

  bool buttonLockout = false;
  ui_pre_render_assets();
  getTipRawTemp(1); // reset filter
  memset(&context, 0, sizeof(context));

  Display::setRotation(getSettingValue(SettingsOptions::OrientationMode) & 1, false);

  // Read boot button state
  if (getButtonA()) {
    buttonsAtDeviceBoot = BUTTON_F_LONG;
  }
  if (getButtonB()) {
    buttonsAtDeviceBoot = BUTTON_B_LONG;
  }

  TickType_t startRender = xTaskGetTickCount();
  for (;;) {
#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
    // Start-up steps that hand over to the next one (warnings with nothing to show, the logo
    // step, which shows no logo in this theme) do not wait out the rest of their frame.
    const OperatingMode before = currentOperatingMode;
    guiRenderLoop();
    resetWatchdog();
    if (currentOperatingMode != before && (before == OperatingMode::InitialisationDone || before == OperatingMode::StartupWarnings || before == OperatingMode::StartupLogo)) {
      startRender = xTaskGetTickCount();
      continue;
    }
#else
    guiRenderLoop();
    resetWatchdog();
#endif
    vTaskDelayUntil(&startRender, TICKS_100MS * 4 / 10); // Try and maintain 20-25fps ish update rate, way to fast but if we can its nice
  }
}
