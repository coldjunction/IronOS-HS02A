/*
 * settingsGUI.cpp
 *
 *  Created on: 3Sep.,2017
 *      Author: Ben V. Brown
 */

#include "settingsGUI.hpp"
#include "Buttons.hpp"
#include "Font.h"
#include "ScrollMessage.hpp"
#include "TipThermoModel.h"
#include "Translation.h"
#include "cmsis_os.h"
#include "configuration.h"
#include "main.hpp"
#include "ui_drawing.hpp"

// A setting value that the stock menu shows as a single letter. The HS-02 card theme has
// room for the whole word (English), up to six large characters.
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
#define SETTING_VALUE_TEXT(id) LargeCardWord##id
// The stock menu marks a switched-off setting with a "no entry" icon; the card theme says Off.
#define SETTING_VALUE_OFF() Display::print(LargeCardWordOff, FontStyle::LARGE)
#else
#define SETTING_VALUE_TEXT(id) translatedString(Tr->id)
#define SETTING_VALUE_OFF() Display::drawUnavailableIcon()
#endif

#ifdef POW_DC
static void displayInputVRange(void);
static bool showInputVOptions(void);
static void displayInputMinVRange(void);
#endif /* POW_DC */

#ifdef POW_QC
static void displayQCInputV(void);
#endif /* POW_QC */

#ifdef POW_PD
static void displayPDNegTimeout(void);
static void displayUSBPDMode(void);
#endif /* POW_PD */

static void displaySensitivity(void);
static void displayShutdownTime(void);
static bool showSleepOptions(void);

#ifndef NO_SLEEP_MODE
static void setSleepTemp(void);
static void displaySleepTemp(void);
static void displaySleepTime(void);
#endif /* *not* NO_SLEEP_MODE */

static void setTempF(void);
static void displayTempF(void);
static void displayAdvancedSolderingScreens(void);
static void displayAdvancedIDLEScreens(void);
static void displayScrollSpeed(void);
static void displayReverseButtonTempChangeEnabled(void);
#if !(defined(LCD_160x80) && defined(HS02_THEME_CARD))
static void displayReverseButtonSettings(void);
#endif
static void displayPowerLimit(void);

#ifdef BLE_ENABLED
static void displayBluetoothLE(void);
#endif /* BLE_ENABLED */

#ifndef NO_DISPLAY_ROTATE
static void setDisplayRotation(void);
static void displayDisplayRotation(void);
#endif /* *not* NO_DISPLAY_ROTATE */

static void setBoostTemp(void);
static void displayBoostTemp(void);

#ifdef PROFILE_SUPPORT
static void setProfilePreheatTemp();
static void setProfilePhase1Temp();
static void setProfilePhase2Temp();
static void setProfilePhase3Temp();
static void setProfilePhase4Temp();
static void setProfilePhase5Temp();
static void displayProfilePhases(void);
static void displayProfilePreheatTemp(void);
static void displayProfilePreheatSpeed(void);
static void displayProfilePhase1Temp(void);
static void displayProfilePhase1Duration(void);
static void displayProfilePhase2Temp(void);
static void displayProfilePhase2Duration(void);
static void displayProfilePhase3Temp(void);
static void displayProfilePhase3Duration(void);
static void displayProfilePhase4Temp(void);
static void displayProfilePhase4Duration(void);
static void displayProfilePhase5Temp(void);
static void displayProfilePhase5Duration(void);
static void displayProfileCooldownSpeed(void);
static bool showProfileOptions(void);
static bool showProfilePhase2Options(void);
static bool showProfilePhase3Options(void);
static bool showProfilePhase4Options(void);
static bool showProfilePhase5Options(void);
#endif /* PROFILE_SUPPORT */

static void displayAutomaticStartMode(void);
static void displayLockingMode(void);
static void displayCoolingBlinkEnabled(void);
static void setResetSettings(void);
static void setCalibrate(void);
static void displayCalibrate(void);
static void setCalibrateVIN(void);
static void displayTempChangeShortStep(void);
static void displayTempChangeLongStep(void);
static void displayPowerPulse(void);
static bool displayAnimationOptions(void);
static void displayAnimationSpeed(void);
static void displayAnimationLoop(void);
static void displayPowerPulseWait(void);
static bool showPowerPulseOptions(void);
static void displayPowerPulseDuration(void);
static void displayBrightnessLevel(void);
static void displayInvertColor(void);
static void displayLogoTime(void);
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
static void displayBuzzerEnabled(void);
static void displayButtonLight(void);
static void displayLowVoltageAllowed(void);
static void setOpenDebugPages(void);
static void setCalibrate100(void);
static void displayCalibrate100(void);
static void displayColourReady(void);
static void displayColourHeating(void);
static void displayColourBoost(void);
static void displayColourSleep(void);
static void displayColourMenu(void);
#endif

#ifdef HALL_SENSOR
static void displayHallEffect(void);
static void displayHallEffectSleepTime(void);
static bool showHallEffect(void);
#endif /* HALL_SENSOR */

// Tip type selection
#ifdef TIP_TYPE_SUPPORT
static void displaySolderingTipType(void);
static bool showSolderingTipType(void);
#endif /* TIP_TYPE_SUPPORT */

// Menu functions

#if defined(POW_DC) || defined(POW_QC) || defined(POW_PD)
static void displayPowerMenu(void);
#endif /* POW_DC or POW_QC */

static void displaySolderingMenu(void);
static void displayPowerSavingMenu(void);
static void displayUIMenu(void);
static void displayAdvancedMenu(void);

/*
 * Root Settings Menu
 *
 * Power Menu
 *  Power Source
 *  -Minimum Voltage
 *  QC Voltage
 *  PD Timeout
 *  USBPDMode
 *
 * Soldering
 *  Tip Type selection
 *  Boost Mode Temp
 *  Auto Start
 *  Temp Change Short Step
 *  Temp Change Long Step
 *  Locking Mode
 *  Profile Phases
 *  Profile Preheat Temperature
 *  Profile Preheat Max Temperature Change Per Second
 *  Profile Phase 1 Temperature
 *  Profile Phase 1 Duration (s)
 *  Profile Phase 2 Temperature
 *  Profile Phase 2 Duration (s)
 *  Profile Phase 3 Temperature
 *  Profile Phase 3 Duration (s)
 *  Profile Phase 4 Temperature
 *  Profile Phase 4 Duration (s)
 *  Profile Phase 5 Temperature
 *  Profile Phase 5 Duration (s)
 *  Profile Cooldown Max Temperature Change Per Second
 *
 * Power Saving
 *  Motion Sensitivity
 *  -Sleep Temp
 *  -Sleep Time
 *  -Shutdown Time
 *  Hall Sensor Sensitivity
 *  Hall Sensor Sleep Time
 *
 * UI
 *  Temperature Unit
 *  Display Orientation
 *  Cooldown Blink
 *  Scrolling Speed
 *  Swap Temp Change Buttons +/-
 *  Animation Speed
 *  -Animation Loop
 *  OLED Brightness
 *  Invert Screen
 *  Logo Timeout
 *  Detailed IDLE
 *  Detailed Soldering
 *
 * Advanced
 *  BluetoothLE
 *  Power Limit
 *  Calibrate CJC At Next Boot
 *  Calibrate Input V
 *  Power Pulse
 *  -Power Pulse Delay
 *  -Power Pulse Duration
 *  Factory Reset
 *
 */

void noOpDisplay() {}

// HS-02 card theme: a temperature value is followed by its unit letter, C or F.
static void printTemperatureUnit(void) {
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  Display::print(getSettingValue(SettingsOptions::TemperatureInF) ? LargeSymbolDegF : LargeSymbolDegC, FontStyle::LARGE);
#endif
}
/* vvv !!!DISABLE CLANG-FORMAT for menuitems initialization!!! vvv */

/* clang-format off */

/* A lot of suggestions by clang-format can be useful
 * but not when dealing with such menuitems declarations.
 */

const menuitem rootSettingsMenu[] {
  /*
   * Power Menu
   * Soldering Menu
   * Power Saving Menu
   * UI Menu
   * Advanced Menu
   * // Language
   * Exit
   */
#if defined(POW_DC) || defined(POW_QC) || defined(POW_PD)
  /* Power */
  {0, nullptr, displayPowerMenu, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0},
#endif
  /* Soldering */
  {0, nullptr, displaySolderingMenu, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0},
  /* Sleep Options Menu */
  {0, nullptr, displayPowerSavingMenu, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0},
  /* UI Menu */
  {0, nullptr, displayUIMenu, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0},
  /* Advanced Menu */
  {0, nullptr, displayAdvancedMenu, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0},
  /* Language Switch */
  {0, settings_setLanguageSwitch, settings_displayLanguageSwitch, settings_showLanguageSwitch, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0},
  /* vvvv end of menu marker. DO NOT REMOVE vvvv */
  {0, nullptr, nullptr, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0}
  /* ^^^^ end of menu marker. DO NOT REMOVE ^^^^ */
};

#if defined(POW_DC) || defined(POW_QC) || defined(POW_PD)
const menuitem powerMenu[] = {
  /*
   * Power Source
   * -Minimum Voltage
   * QC Voltage
   * PD Timeout
   * USBPDMode
   */
#ifdef POW_DC
  /* Voltage input */
  {SETTINGS_DESC(SettingsItemIndex::DCInCutoff), nullptr, displayInputVRange, nullptr, SettingsOptions::MinDCVoltageCells, SettingsItemIndex::DCInCutoff, 6},
  /* Minimum voltage input */
  {SETTINGS_DESC(SettingsItemIndex::MinVolCell), nullptr, displayInputMinVRange, showInputVOptions, SettingsOptions::MinVoltageCells, SettingsItemIndex::MinVolCell, 5},
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  /* HS-02: accept a DC supply below 9 V */
  {SETTINGS_DESC(SettingsItemIndex::LowVoltageAllowed), nullptr, displayLowVoltageAllowed, nullptr, SettingsOptions::LowVoltageAllowed, SettingsItemIndex::LowVoltageAllowed, 7},
#endif
#endif
#ifdef POW_QC
  /* Voltage input */
  {SETTINGS_DESC(SettingsItemIndex::QCMaxVoltage), nullptr, displayQCInputV, nullptr, SettingsOptions::QCIdealVoltage, SettingsItemIndex::QCMaxVoltage, 4},
#endif
#ifdef POW_PD
  /* PD timeout setup */
  {SETTINGS_DESC(SettingsItemIndex::PDNegTimeout), nullptr, displayPDNegTimeout, nullptr, SettingsOptions::PDNegTimeout, SettingsItemIndex::PDNegTimeout, 6},
  /* Toggle PPS & EPR */
  {SETTINGS_DESC(SettingsItemIndex::USBPDMode), nullptr, displayUSBPDMode, nullptr, SettingsOptions::USBPDMode, SettingsItemIndex::USBPDMode, 4},
#endif
  /* vvvv end of menu marker. DO NOT REMOVE vvvv */
  {0, nullptr, nullptr, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0}
  /* ^^^^ end of menu marker. DO NOT REMOVE ^^^^ */
};
#endif /* POW_DC or POW_QC or POW_PD */

const menuitem solderingMenu[] = {
  /*
   *  Boost Mode Temp
   *  Auto Start
   *  Temp Change Short Step
   *  Temp Change Long Step
   *  Locking Mode
   *  Tip Type
   *  Profile Phases
   *  Profile Preheat Temperature
   *  Profile Preheat Max Temperature Change Per Second
   *  Profile Phase 1 Temperature
   *  Profile Phase 1 Duration (s)
   *  Profile Phase 2 Temperature
   *  Profile Phase 2 Duration (s)
   *  Profile Phase 3 Temperature
   *  Profile Phase 3 Duration (s)
   *  Profile Phase 4 Temperature
   *  Profile Phase 4 Duration (s)
   *  Profile Phase 5 Temperature
   *  Profile Phase 5 Duration (s)
   *  Profile Cooldown Max Temperature Change Per Second
   */
  /* Boost Temp */
  {SETTINGS_DESC(SettingsItemIndex::BoostTemperature), setBoostTemp, displayBoostTemp, nullptr, SettingsOptions::BoostTemp, SettingsItemIndex::BoostTemperature, 5},
  /* Auto start */
  {SETTINGS_DESC(SettingsItemIndex::AutoStart), nullptr, displayAutomaticStartMode, nullptr, SettingsOptions::AutoStartMode, SettingsItemIndex::AutoStart, 7},
  /* Temp change short step */
  {SETTINGS_DESC(SettingsItemIndex::TempChangeShortStep), nullptr, displayTempChangeShortStep, nullptr, SettingsOptions::TempChangeShortStep, SettingsItemIndex::TempChangeShortStep, 6},
  /* Temp change long step */
  {SETTINGS_DESC(SettingsItemIndex::TempChangeLongStep), nullptr, displayTempChangeLongStep, nullptr, SettingsOptions::TempChangeLongStep, SettingsItemIndex::TempChangeLongStep, 6},
  /* Locking Mode */
  {SETTINGS_DESC(SettingsItemIndex::LockingMode), nullptr, displayLockingMode, nullptr, SettingsOptions::LockingMode, SettingsItemIndex::LockingMode, 7},
#ifdef TIP_TYPE_SUPPORT
  /* Tip Type */
  {SETTINGS_DESC(SettingsItemIndex::SolderingTipType), nullptr, displaySolderingTipType, showSolderingTipType, SettingsOptions::SolderingTipType, SettingsItemIndex::SolderingTipType, 5},
#endif /* TIP_TYPE_SUPPORT */
#ifdef PROFILE_SUPPORT
  /* Profile Phases */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePhases), nullptr, displayProfilePhases, nullptr, SettingsOptions::ProfilePhases, SettingsItemIndex::ProfilePhases, 7},
  /* Profile Preheat Temp */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePreheatTemp), setProfilePreheatTemp, displayProfilePreheatTemp, showProfileOptions, SettingsOptions::ProfilePreheatTemp, SettingsItemIndex::ProfilePreheatTemp, 5},
  /* Profile Preheat Speed */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePreheatSpeed), nullptr, displayProfilePreheatSpeed, showProfileOptions, SettingsOptions::ProfilePreheatSpeed, SettingsItemIndex::ProfilePreheatSpeed, 5},
  /* Phase 1 Temp */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePhase1Temp), setProfilePhase1Temp, displayProfilePhase1Temp, showProfileOptions, SettingsOptions::ProfilePhase1Temp, SettingsItemIndex::ProfilePhase1Temp, 5},
  /* Phase 1 Duration */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePhase1Duration), nullptr, displayProfilePhase1Duration, showProfileOptions, SettingsOptions::ProfilePhase1Duration, SettingsItemIndex::ProfilePhase1Duration, 5},
  /* Phase 2 Temp */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePhase1Temp), setProfilePhase2Temp, displayProfilePhase2Temp, showProfilePhase2Options, SettingsOptions::ProfilePhase1Temp, SettingsItemIndex::ProfilePhase2Temp, 5},
  /* Phase 2 Duration */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePhase1Duration), nullptr, displayProfilePhase2Duration, showProfilePhase2Options, SettingsOptions::ProfilePhase2Duration, SettingsItemIndex::ProfilePhase2Duration, 5},
  /* Phase 3 Temp */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePhase1Temp), setProfilePhase3Temp, displayProfilePhase3Temp, showProfilePhase3Options, SettingsOptions::ProfilePhase1Temp, SettingsItemIndex::ProfilePhase3Temp, 5},
  /* Phase 3 Duration */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePhase1Duration), nullptr, displayProfilePhase3Duration, showProfilePhase3Options, SettingsOptions::ProfilePhase3Duration, SettingsItemIndex::ProfilePhase3Duration, 5},
  /* Phase 4 Temp */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePhase1Temp), setProfilePhase4Temp, displayProfilePhase4Temp, showProfilePhase4Options, SettingsOptions::ProfilePhase1Temp, SettingsItemIndex::ProfilePhase4Temp, 5},
  /* Phase 4 Duration */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePhase1Duration), nullptr, displayProfilePhase4Duration, showProfilePhase4Options, SettingsOptions::ProfilePhase4Duration, SettingsItemIndex::ProfilePhase4Duration, 5},
  /* Phase 5 Temp */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePhase1Temp), setProfilePhase5Temp, displayProfilePhase5Temp, showProfilePhase5Options, SettingsOptions::ProfilePhase1Temp, SettingsItemIndex::ProfilePhase5Temp, 5},
  /* Phase 5 Duration */
  {SETTINGS_DESC(SettingsItemIndex::ProfilePhase1Duration), nullptr, displayProfilePhase5Duration, showProfilePhase5Options, SettingsOptions::ProfilePhase5Duration, SettingsItemIndex::ProfilePhase5Duration, 5},
  /* Profile Cooldown Speed */
  {SETTINGS_DESC(SettingsItemIndex::ProfileCooldownSpeed), nullptr, displayProfileCooldownSpeed, showProfileOptions, SettingsOptions::ProfileCooldownSpeed, SettingsItemIndex::ProfileCooldownSpeed, 5},
#endif /* PROFILE_SUPPORT */
  /* vvvv end of menu marker. DO NOT REMOVE vvvv */
  {0, nullptr, nullptr, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0}
  /* ^^^^ end of menu marker. DO NOT REMOVE ^^^^ */
};

const menuitem PowerSavingMenu[] = {
  /*
   *  Motion Sensitivity
   *  -Sleep Temp
   *  -Sleep Time
   *  -Shutdown Time
   *  Hall Sensor Sensitivity
   */
  /* Motion Sensitivity */
  {SETTINGS_DESC(SettingsItemIndex::MotionSensitivity), nullptr, displaySensitivity, nullptr, SettingsOptions::Sensitivity, SettingsItemIndex::MotionSensitivity, 7},
#ifndef NO_SLEEP_MODE
  /* Sleep Temp */
  {SETTINGS_DESC(SettingsItemIndex::SleepTemperature), setSleepTemp, displaySleepTemp, showSleepOptions, SettingsOptions::SleepTemp, SettingsItemIndex::SleepTemperature, 5},
  /* Sleep Time */
  {SETTINGS_DESC(SettingsItemIndex::SleepTimeout), nullptr, displaySleepTime, showSleepOptions, SettingsOptions::SleepTime, SettingsItemIndex::SleepTimeout, 5},
#endif /* *not* NO_SLEEP_MODE */
  /* Shutdown Time */
  {SETTINGS_DESC(SettingsItemIndex::ShutdownTimeout), nullptr, displayShutdownTime, showSleepOptions, SettingsOptions::ShutdownTime, SettingsItemIndex::ShutdownTimeout, 5},
#ifdef HALL_SENSOR
  /* Hall Effect Sensitivity */
  {SETTINGS_DESC(SettingsItemIndex::HallEffSensitivity), nullptr, displayHallEffect, showHallEffect, SettingsOptions::HallEffectSensitivity, SettingsItemIndex::HallEffSensitivity, 7},
  /* Hall Effect Sleep Time */
  {SETTINGS_DESC(SettingsItemIndex::HallEffSleepTimeout), nullptr, displayHallEffectSleepTime, showHallEffect, SettingsOptions::HallEffectSleepTime, SettingsItemIndex::HallEffSleepTimeout, 5},
#endif /* HALL_SENSOR */
  /* vvvv end of menu marker. DO NOT REMOVE vvvv */
  {0, nullptr, nullptr, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0}
  /* ^^^^ end of menu marker. DO NOT REMOVE ^^^^ */
};

const menuitem UIMenu[] = {
  /*
   *  Temperature Unit
   *  Display Orientation
   *  Cooldown Blink
   *  Scrolling Speed
   *  Swap Temp Change Buttons +/-
   *  Animation Speed
   *  -Animation Loop
   *  OLED Brightness
   *  Invert Screen
   *  Logo Timeout
   *  Detailed IDLE
   *  Detailed Soldering
   */
  /* Temperature units, this has to be the first element in the array to work with the logic in enterUIMenu() */
  {SETTINGS_DESC(SettingsItemIndex::TemperatureUnit), setTempF, displayTempF, nullptr, SettingsOptions::TemperatureInF, SettingsItemIndex::TemperatureUnit, 7},
#ifndef NO_DISPLAY_ROTATE
  /* Display Rotation */
  {SETTINGS_DESC(SettingsItemIndex::DisplayRotation), setDisplayRotation, displayDisplayRotation, nullptr, SettingsOptions::OrientationMode, SettingsItemIndex::DisplayRotation, 7},
#endif /* *not* NO_DISPLAY_ROTATE */
  /* Cooling blink warning */
  {SETTINGS_DESC(SettingsItemIndex::CooldownBlink), nullptr, displayCoolingBlinkEnabled, nullptr, SettingsOptions::CoolingTempBlink, SettingsItemIndex::CooldownBlink, 7},
  /* Scroll Speed for descriptions */
  {SETTINGS_DESC(SettingsItemIndex::ScrollingSpeed), nullptr, displayScrollSpeed, nullptr, SettingsOptions::DescriptionScrollSpeed, SettingsItemIndex::ScrollingSpeed, 7},
  /* Reverse Temp change buttons +/- */
  {SETTINGS_DESC(SettingsItemIndex::ReverseButtonTempChange), nullptr, displayReverseButtonTempChangeEnabled, nullptr, SettingsOptions::ReverseButtonTempChangeEnabled, SettingsItemIndex::ReverseButtonTempChange, 7},
#if !(defined(LCD_160x80) && defined(HS02_THEME_CARD))
  /* Reverse Settings menu buttons A/B. Not in the HS-02 card theme: its menu has a fixed
     layout (DOWN, UP, OK, as the hint row shows) and does not read this setting. */
  {SETTINGS_DESC(SettingsItemIndex::ReverseButtonSettings), nullptr, displayReverseButtonSettings, nullptr, SettingsOptions::ReverseButtonSettings, SettingsItemIndex::ReverseButtonSettings, 7},
#endif
  /* Animation Speed adjustment */
  {SETTINGS_DESC(SettingsItemIndex::AnimSpeed), nullptr, displayAnimationSpeed, nullptr, SettingsOptions::AnimationSpeed, SettingsItemIndex::AnimSpeed, 7},
  /* Animation Loop switch */
  {SETTINGS_DESC(SettingsItemIndex::AnimLoop), nullptr, displayAnimationLoop, displayAnimationOptions, SettingsOptions::AnimationLoop, SettingsItemIndex::AnimLoop, 7},
  /* Brightness Level */
  {SETTINGS_DESC(SettingsItemIndex::Brightness), nullptr, displayBrightnessLevel, nullptr, SettingsOptions::DisplayBrightness, SettingsItemIndex::Brightness, 7},
  /* Invert screen colour */
  {SETTINGS_DESC(SettingsItemIndex::ColourInversion), nullptr, displayInvertColor, nullptr, SettingsOptions::DisplayInversion, SettingsItemIndex::ColourInversion, 7},
  /* Set logo duration */
  {SETTINGS_DESC(SettingsItemIndex::LOGOTime), nullptr, displayLogoTime, nullptr, SettingsOptions::LOGOTime, SettingsItemIndex::LOGOTime, 6},
  /* Advanced idle screen */
  {SETTINGS_DESC(SettingsItemIndex::AdvancedIdle), nullptr, displayAdvancedIDLEScreens, nullptr, SettingsOptions::DetailedIDLE, SettingsItemIndex::AdvancedIdle, 7},
  /* Advanced soldering screen */
  {SETTINGS_DESC(SettingsItemIndex::AdvancedSoldering), nullptr, displayAdvancedSolderingScreens, nullptr, SettingsOptions::DetailedSoldering, SettingsItemIndex::AdvancedSoldering, 7},
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  /* HS-02 card theme: buzzer on/off, accent colour */
  {SETTINGS_DESC(SettingsItemIndex::BuzzerEnabled), nullptr, displayBuzzerEnabled, nullptr, SettingsOptions::BuzzerEnabled, SettingsItemIndex::BuzzerEnabled, 7},
  {SETTINGS_DESC(SettingsItemIndex::ButtonLight), nullptr, displayButtonLight, nullptr, SettingsOptions::ButtonLight, SettingsItemIndex::ButtonLight, 7},
  {SETTINGS_DESC(SettingsItemIndex::ColourReady), nullptr, displayColourReady, nullptr, SettingsOptions::ColourReady, SettingsItemIndex::ColourReady, 7},
  {SETTINGS_DESC(SettingsItemIndex::ColourHeating), nullptr, displayColourHeating, nullptr, SettingsOptions::ColourHeating, SettingsItemIndex::ColourHeating, 7},
  {SETTINGS_DESC(SettingsItemIndex::ColourBoost), nullptr, displayColourBoost, nullptr, SettingsOptions::ColourBoost, SettingsItemIndex::ColourBoost, 7},
  {SETTINGS_DESC(SettingsItemIndex::ColourSleep), nullptr, displayColourSleep, nullptr, SettingsOptions::ColourSleep, SettingsItemIndex::ColourSleep, 7},
  {SETTINGS_DESC(SettingsItemIndex::ColourMenu), nullptr, displayColourMenu, nullptr, SettingsOptions::ColourMenu, SettingsItemIndex::ColourMenu, 7},
#endif
  /* vvvv end of menu marker. DO NOT REMOVE vvvv */
  {0, nullptr, nullptr, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0}
  /* ^^^^ end of menu marker. DO NOT REMOVE ^^^^ */
};

const menuitem advancedMenu[] = {
  /*
   *  BluetoothLE
   *  Power Limit
   *  Calibrate CJC At Next Boot
   *  Calibrate Input V
   *  Power Pulse
   *  -Power Pulse Delay
   *  -Power Pulse Duration
   *  Factory Reset
   */
#ifdef BLE_ENABLED
  /* Toggle BLE */
  {SETTINGS_DESC(SettingsItemIndex::BluetoothLE), nullptr, displayBluetoothLE, nullptr, SettingsOptions::BluetoothLE, SettingsItemIndex::BluetoothLE, 7},
#endif /* BLE_ENABLED */
  /* Power limit */
  {SETTINGS_DESC(SettingsItemIndex::PowerLimit), nullptr, displayPowerLimit, nullptr, SettingsOptions::PowerLimit, SettingsItemIndex::PowerLimit, 4},
  /* Calibrate Cold Junktion Compensation at next boot */
  {SETTINGS_DESC(SettingsItemIndex::CalibrateCJC), setCalibrate, displayCalibrate, nullptr, SettingsOptions::CalibrateCJC, SettingsItemIndex::CalibrateCJC, 7},
  /* Voltage input cal */
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  /* HS-02 card theme: a measured 100 C point for the temperature curve (tip in boiling water) */
  {SETTINGS_DESC(SettingsItemIndex::Calibrate100), setCalibrate100, displayCalibrate100, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::Calibrate100, 7},
#endif
  {SETTINGS_DESC(SettingsItemIndex::VoltageCalibration), setCalibrateVIN, noOpDisplay, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::VoltageCalibration, 5},
  /* Power Pulse adjustment */
  {SETTINGS_DESC(SettingsItemIndex::PowerPulsePower), nullptr, displayPowerPulse, nullptr, SettingsOptions::KeepAwakePulse, SettingsItemIndex::PowerPulsePower, 5},
  /* Power Pulse Wait adjustment */
  {SETTINGS_DESC(SettingsItemIndex::PowerPulseWait), nullptr, displayPowerPulseWait, showPowerPulseOptions, SettingsOptions::KeepAwakePulseWait, SettingsItemIndex::PowerPulseWait, 7},
  /* Power Pulse Duration adjustment */
  {SETTINGS_DESC(SettingsItemIndex::PowerPulseDuration), nullptr, displayPowerPulseDuration, showPowerPulseOptions, SettingsOptions::KeepAwakePulseDuration, SettingsItemIndex::PowerPulseDuration, 7},
  /* Resets settings */
  {SETTINGS_DESC(SettingsItemIndex::SettingsReset), setResetSettings, noOpDisplay, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::SettingsReset, 7},
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  /* HS-02 card theme: the debug pages, without having to hold a button on the home screen */
  {SETTINGS_DESC(SettingsItemIndex::DebugPages), setOpenDebugPages, noOpDisplay, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::DebugPages, 7},
#endif
  /* vvvv end of menu marker. DO NOT REMOVE vvvv */
  {0, nullptr, nullptr, nullptr, SettingsOptions::SettingsOptionsLength, SettingsItemIndex::NUM_ITEMS, 0}
  /* ^^^^ end of menu marker. DO NOT REMOVE ^^^^ */
};

/* clang-format on */

const menuitem *subSettingsMenus[]{
#if defined(POW_DC) || defined(POW_QC) || defined(POW_PD)
    powerMenu,
#endif
    solderingMenu, PowerSavingMenu, UIMenu, advancedMenu,
};
/* ^^^ !!!ENABLE CLANG-FORMAT back!!! ^^^ */

/**
 * Prints two small lines (or one line for CJK) of short description for
 * setting items and prepares cursor after it.
 * @param settingsItemIndex Index of the setting item.
 * @param cursorCharPosition Custom cursor char position to set after printing
 * description.
 */
static void printShortDescription(SettingsItemIndex settingsItemIndex, uint16_t cursorCharPosition) {
  // print short description (default single line, explicit double line)
  uint8_t shortDescIndex = static_cast<uint8_t>(settingsItemIndex);
  Display::printWholeScreen(translatedString(Tr->SettingsShortNames[shortDescIndex]));

  // prepare cursor for value
  // make room for scroll indicator
  Display::setCursor(cursorCharPosition * FONT_SMALL_WIDTH - 2, 0);
}

static int userConfirmation(const char *message) {
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  // Card theme: the two prompts become a short text that fits on the screen at once, over
  // a hint row in button order (DOWN, UP, OK): no / no / yes. Other messages still scroll.
  const char *text[4] = {nullptr, nullptr, nullptr, nullptr}; // one small-font row each
  if (message == translatedString(Tr->SettingsCalibrationWarning)) {
    text[0] = SmallCardConfirmCalibrate1;
    text[1] = SmallCardConfirmCalibrate2;
    text[2] = SmallCardConfirmCalibrate3;
    text[3] = SmallCardConfirmCalibrate4;
  } else if (message == translatedString(Tr->SettingsResetWarning)) {
    text[0] = SmallCardConfirmReset1;
    text[1] = SmallCardConfirmReset2;
    text[2] = SmallCardConfirmReset3;
  }
  if (text[0] != nullptr) {
    for (;;) {
      Display::clearScreen();
      for (uint8_t row = 0; row < 4 && text[row] != nullptr; row++) {
        Display::setCursor(0, row * 16);
        Display::print(text[row], FontStyle::SMALL);
      }
      Display::setCursor(0, 64); // DOWN
      Display::print(SmallSymbolHintNo, FontStyle::SMALL);
      Display::setCursor(54, 64); // UP
      Display::print(SmallSymbolHintNo, FontStyle::SMALL);
      Display::setCursor(108, 64); // OK
      Display::print(SmallSymbolHintYes, FontStyle::SMALL);
      LCD::setMonoRegions(0, 0, 255, 64);
      switch (getButtonState()) {
      case BUTTON_OK_SHORT:
        return 1;
      case BUTTON_NONE:
        break;
      default:
        return 0;
      }
      Display::refresh();
      osDelay(40);
    }
  }
#endif
  TickType_t tickStart = xTaskGetTickCount();
  for (;;) {
    drawScrollingText(message, xTaskGetTickCount() - tickStart);
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
    LCD::setMonoRegions(0, 0, 255, 255); // one colour, not the menu's split
#endif

    ButtonState buttons = getButtonState();
    switch (buttons) {
    case BUTTON_F_SHORT:
      // User confirmed
      return 1;

    case BUTTON_NONE:
      break;
    default:
    case BUTTON_BOTH:
    case BUTTON_B_SHORT:
    case BUTTON_F_LONG:
    case BUTTON_B_LONG:
      return 0;
    }

    Display::refresh();
    osDelay(40);
  }
  return 0;
}

#ifdef POW_DC

static void displayInputVRange(void) {
  if (getSettingValue(SettingsOptions::MinDCVoltageCells)) {
    Display::printNumber(2 + getSettingValue(SettingsOptions::MinDCVoltageCells), 1, FontStyle::LARGE);
    Display::print(LargeSymbolCellCount, FontStyle::LARGE);
  } else {
    Display::print(LargeSymbolDC, FontStyle::LARGE);
  }
}

static bool showInputVOptions(void) { return getSettingValue(SettingsOptions::MinDCVoltageCells) > 0; }

static void displayInputMinVRange(void) {
  Display::printNumber(getSettingValue(SettingsOptions::MinVoltageCells) / 10, 1, FontStyle::LARGE);
  Display::print(LargeSymbolDot, FontStyle::LARGE);
  Display::printNumber(getSettingValue(SettingsOptions::MinVoltageCells) % 10, 1, FontStyle::LARGE);
}

#endif /* POW_DC */

#ifdef POW_QC

static void displayQCInputV(void) {
  // These are only used in QC modes
  // Allows setting the voltage negotiated for QC
  auto voltage = getSettingValue(SettingsOptions::QCIdealVoltage);
  Display::printNumber(voltage / 10, 2, FontStyle::LARGE);
  Display::print(LargeSymbolDot, FontStyle::LARGE);
  Display::printNumber(voltage % 10, 1, FontStyle::LARGE);
}

#endif /* POW_QC */

#ifdef POW_PD /* POW_PD */

static void displayPDNegTimeout(void) {
  auto value = getSettingValue(SettingsOptions::PDNegTimeout);
  value ? Display::printNumber(value, 2, FontStyle::LARGE) : SETTING_VALUE_OFF();
}

static void displayUSBPDMode(void) {
  /*
   * Supported PD modes:
   *  DEFAULT,    1 = PPS + EPR + more power request through increasing resistance by 0.5 Ohm to compensate power loss over cable/PCB/etc.
   *  SAFE,       2 = PPS + EPR, without requesting more power
   *  NO_DYNAMIC, 0 = PPS + EPR disabled, fixed PDO only
   */

  switch (getSettingValue(SettingsOptions::USBPDMode)) {
  case usbpdMode_t::DEFAULT:
    Display::print(translatedString(Tr->USBPDModeDefault), FontStyle::SMALL, 255, Display::getCursorX());
    break;
  case usbpdMode_t::SAFE:
    Display::print(translatedString(Tr->USBPDModeSafe), FontStyle::SMALL, 255, Display::getCursorX());
    break;
  case usbpdMode_t::NO_DYNAMIC:
  default:
    Display::print(translatedString(Tr->USBPDModeNoDynamic), FontStyle::SMALL, 255, Display::getCursorX());
    break;
  }
}

#endif /* POW_PD */

static void setBoostTemp(void) {
  uint16_t value = getSettingValue(SettingsOptions::BoostTemp);
  if (getSettingValue(SettingsOptions::TemperatureInF)) {
    if (value == 0) {
      value = MIN_BOOST_TEMP_F; // loop back at 480
    } else {
      value += 20; // Go up 20F at a time
    }

    if (value >= MAX_TEMP_F) {
      value = 0; // jump to off
    }
  } else {
    if (value == 0) {
      value = MIN_BOOST_TEMP_C; // loop back at 250
    } else {
      value += 10; // Go up 10C at a time
    }
    if (value > MAX_TEMP_C) {
      value = 0; // Go to off state
    }
  }
  setSettingValue(SettingsOptions::BoostTemp, value);
}

static void displayBoostTemp(void) {
  if (getSettingValue(SettingsOptions::BoostTemp)) {
    Display::printNumber(getSettingValue(SettingsOptions::BoostTemp), 3, FontStyle::LARGE);
    printTemperatureUnit();
  } else {
    SETTING_VALUE_OFF();
  }
}

static void displayAutomaticStartMode(void) {
  switch (getSettingValue(SettingsOptions::AutoStartMode)) {
  case autoStartMode_t::NO:
    SETTING_VALUE_OFF();
    break;
  case autoStartMode_t::SOLDER:
    Display::print(SETTING_VALUE_TEXT(SettingStartSolderingChar), FontStyle::LARGE);
    break;
  case autoStartMode_t::SLEEP:
    Display::print(SETTING_VALUE_TEXT(SettingStartSleepChar), FontStyle::LARGE);
    break;
  case autoStartMode_t::ZERO:
    Display::print(SETTING_VALUE_TEXT(SettingStartSleepOffChar), FontStyle::LARGE);
    break;
  default:
    SETTING_VALUE_OFF();
    break;
  }
}

static void displayTempChangeShortStep(void) {
  Display::printNumber(getSettingValue(SettingsOptions::TempChangeShortStep), 2, FontStyle::LARGE);
  printTemperatureUnit();
}

static void displayTempChangeLongStep(void) {
  Display::printNumber(getSettingValue(SettingsOptions::TempChangeLongStep), 2, FontStyle::LARGE);
  printTemperatureUnit();
}

static void displayLockingMode(void) {
  switch (getSettingValue(SettingsOptions::LockingMode)) {
  case lockingMode_t::DISABLED:
    SETTING_VALUE_OFF();
    break;
  case lockingMode_t::BOOST:
    Display::print(SETTING_VALUE_TEXT(SettingLockBoostChar), FontStyle::LARGE);
    break;
  case lockingMode_t::FULL:
    Display::print(SETTING_VALUE_TEXT(SettingLockFullChar), FontStyle::LARGE);
    break;
  default:
    SETTING_VALUE_OFF();
    break;
  }
}

#ifdef PROFILE_SUPPORT

static void displayProfilePhases(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePhases), 1, FontStyle::LARGE); }

static void setProfileTemp(const enum SettingsOptions option) {
  // If in C, 5 deg, if in F 10 deg
  uint16_t temp = getSettingValue(option);
  if (getSettingValue(SettingsOptions::TemperatureInF)) {
    temp += 10;
    if (temp > MAX_TEMP_F) {
      temp = MIN_TEMP_F;
    }
  } else {
    temp += 5;
    if (temp > MAX_TEMP_C) {
      temp = MIN_TEMP_C;
    }
  }
  setSettingValue(option, temp);
}

static void setProfilePreheatTemp(void) { return setProfileTemp(SettingsOptions::ProfilePreheatTemp); }
static void setProfilePhase1Temp(void) { return setProfileTemp(SettingsOptions::ProfilePhase1Temp); }
static void setProfilePhase2Temp(void) { return setProfileTemp(SettingsOptions::ProfilePhase2Temp); }
static void setProfilePhase3Temp(void) { return setProfileTemp(SettingsOptions::ProfilePhase3Temp); }
static void setProfilePhase4Temp(void) { return setProfileTemp(SettingsOptions::ProfilePhase4Temp); }
static void setProfilePhase5Temp(void) { return setProfileTemp(SettingsOptions::ProfilePhase5Temp); }

static void displayProfilePreheatTemp(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePreheatTemp), 3, FontStyle::LARGE); }
static void displayProfilePhase1Temp(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePhase1Temp), 3, FontStyle::LARGE); }
static void displayProfilePhase2Temp(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePhase2Temp), 3, FontStyle::LARGE); }
static void displayProfilePhase3Temp(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePhase3Temp), 3, FontStyle::LARGE); }
static void displayProfilePhase4Temp(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePhase4Temp), 3, FontStyle::LARGE); }
static void displayProfilePhase5Temp(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePhase5Temp), 3, FontStyle::LARGE); }
static void displayProfilePreheatSpeed(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePreheatSpeed), 2, FontStyle::LARGE); }
static void displayProfileCooldownSpeed(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfileCooldownSpeed), 2, FontStyle::LARGE); }
static void displayProfilePhase1Duration(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePhase1Duration), 3, FontStyle::LARGE); }
static void displayProfilePhase2Duration(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePhase2Duration), 3, FontStyle::LARGE); }
static void displayProfilePhase3Duration(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePhase3Duration), 3, FontStyle::LARGE); }
static void displayProfilePhase4Duration(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePhase4Duration), 3, FontStyle::LARGE); }
static void displayProfilePhase5Duration(void) { Display::printNumber(getSettingValue(SettingsOptions::ProfilePhase5Duration), 3, FontStyle::LARGE); }

static bool showProfileOptions(void) { return getSettingValue(SettingsOptions::ProfilePhases); }
static bool showProfilePhase2Options(void) { return getSettingValue(SettingsOptions::ProfilePhases) >= 2; }
static bool showProfilePhase3Options(void) { return getSettingValue(SettingsOptions::ProfilePhases) >= 3; }
static bool showProfilePhase4Options(void) { return getSettingValue(SettingsOptions::ProfilePhases) >= 4; }
static bool showProfilePhase5Options(void) { return getSettingValue(SettingsOptions::ProfilePhases) >= 5; }

#endif /* PROFILE_SUPPORT */

static void displaySensitivity(void) {
  if (getSettingValue(SettingsOptions::Sensitivity)) {
    Display::printNumber(getSettingValue(SettingsOptions::Sensitivity), 1, FontStyle::LARGE, false);
  } else {
    SETTING_VALUE_OFF();
  }
}
static bool showSleepOptions(void) { return getSettingValue(SettingsOptions::Sensitivity) > 0; }

#ifndef NO_SLEEP_MODE

static void setSleepTemp(void) {
  // If in C, 10 deg, if in F 20 deg
  uint16_t temp = getSettingValue(SettingsOptions::SleepTemp);
  if (getSettingValue(SettingsOptions::TemperatureInF)) {
    temp += 20;
    if (temp > 580) {
      temp = 60;
    }
  } else {
    temp += 10;
    if (temp > 300) {
      temp = 10;
    }
  }
  setSettingValue(SettingsOptions::SleepTemp, temp);
}

static void displaySleepTemp(void) {
  Display::printNumber(getSettingValue(SettingsOptions::SleepTemp), 3, FontStyle::LARGE);
  printTemperatureUnit();
}

static void displaySleepTime(void) {
  if (getSettingValue(SettingsOptions::SleepTime) == 0) {
    SETTING_VALUE_OFF();
  } else if (getSettingValue(SettingsOptions::SleepTime) < 6) {
    Display::printNumber(getSettingValue(SettingsOptions::SleepTime) * 10, 2, FontStyle::LARGE);
    Display::print(LargeSymbolSeconds, FontStyle::LARGE);
  } else {
    Display::printNumber(getSettingValue(SettingsOptions::SleepTime) - 5, 2, FontStyle::LARGE);
    Display::print(LargeSymbolMinutes, FontStyle::LARGE);
  }
}

#endif /* *not* NO_SLEEP_MODE */

static void displayShutdownTime(void) {
  if (getSettingValue(SettingsOptions::ShutdownTime) == 0) {
    SETTING_VALUE_OFF();
  } else {
    Display::printNumber(getSettingValue(SettingsOptions::ShutdownTime), 2, FontStyle::LARGE);
    Display::print(LargeSymbolMinutes, FontStyle::LARGE);
  }
}

#ifdef HALL_SENSOR
static void displayHallEffect(void) {
  if (getSettingValue(SettingsOptions::HallEffectSensitivity)) {
    Display::printNumber(getSettingValue(SettingsOptions::HallEffectSensitivity), 1, FontStyle::LARGE, false);
  } else {
    SETTING_VALUE_OFF();
  }
}
static bool showHallEffect(void) { return getHallSensorFitted(); }
static void displayHallEffectSleepTime(void) {
  if (getSettingValue(SettingsOptions::HallEffectSleepTime)) {
    Display::printNumber(getSettingValue(SettingsOptions::HallEffectSleepTime) * 5, 2, FontStyle::LARGE, false);
  } else {
    // When sleep time is set to zero, we sleep for 1 second anyways. This is the default.
    Display::printNumber(1, 2, FontStyle::LARGE, false);
  }
  Display::print(LargeSymbolSeconds, FontStyle::LARGE);
}
#endif /* HALL_SENSOR */

#ifdef TIP_TYPE_SUPPORT
static void displaySolderingTipType(void) {
  // TODO wrapping X value
  Display::print(lookupTipName(), FontStyle::SMALL, 255, Display::getCursorX());
}
// If there is no detection, and no options, max is 0
static bool showSolderingTipType(void) { return tipType_t::TIP_TYPE_MAX != 0; }
#endif /* TIP_TYPE_SUPPORT */

static void setTempF(const enum SettingsOptions option) {
  uint16_t Temp = getSettingValue(option);
  if (option == SettingsOptions::BoostTemp && Temp == 0) {
    return; // 0 is "boost off", not a temperature: it stays off in either unit
  }
  if (getSettingValue(SettingsOptions::TemperatureInF)) {
    // Change temp to the F equiv
    // C to F == F= ( (C*9) +160)/5
    Temp = ((Temp * 9) + 160) / 5;
  } else {
    // Change temp to the C equiv
    // F->C == C = ((F-32)*5)/9
    Temp = ((Temp - 32) * 5) / 9;
  }
  // Rescale to be multiples of 10
  Temp = Temp / 10;
  Temp *= 10;
  setSettingValue(option, Temp);
}

static void setTempF(void) {
  nextSettingValue(SettingsOptions::TemperatureInF);
  setTempF(SettingsOptions::BoostTemp);
  setTempF(SettingsOptions::SolderingTemp);
#ifndef NO_SLEEP_MODE
  setTempF(SettingsOptions::SleepTemp);
#endif /* *not* NO_SLEEP_MODE */
#ifdef PROFILE_SUPPORT
  setTempF(SettingsOptions::ProfilePreheatTemp);
  setTempF(SettingsOptions::ProfilePhase1Temp);
  setTempF(SettingsOptions::ProfilePhase2Temp);
  setTempF(SettingsOptions::ProfilePhase3Temp);
  setTempF(SettingsOptions::ProfilePhase4Temp);
  setTempF(SettingsOptions::ProfilePhase5Temp);
#endif /* PROFILE_SUPPORT */
}

static void displayTempF(void) { Display::printSymbolDeg(FontStyle::LARGE); }

#ifndef NO_DISPLAY_ROTATE

static void setDisplayRotation(void) {
  nextSettingValue(SettingsOptions::OrientationMode);
  switch (getSettingValue(SettingsOptions::OrientationMode)) {
  case orientationMode_t::RIGHT:
    Display::setRotation(false);
    break;
  case orientationMode_t::LEFT:
    Display::setRotation(true);
    break;
  case orientationMode_t::AUTO:
    // do nothing on auto
    break;
  default:
    break;
  }
}

static void displayDisplayRotation(void) {
  switch (getSettingValue(SettingsOptions::OrientationMode)) {
  case orientationMode_t::RIGHT:
    Display::print(SETTING_VALUE_TEXT(SettingRightChar), FontStyle::LARGE);
    break;
  case orientationMode_t::LEFT:
    Display::print(SETTING_VALUE_TEXT(SettingLeftChar), FontStyle::LARGE);
    break;
  case orientationMode_t::AUTO:
    Display::print(SETTING_VALUE_TEXT(SettingAutoChar), FontStyle::LARGE);
    break;
  default:
    Display::print(SETTING_VALUE_TEXT(SettingRightChar), FontStyle::LARGE);
    break;
  }
}

#endif /* NO_DISPLAY_ROTATE */

static void displayCoolingBlinkEnabled(void) { Display::drawCheckbox(getSettingValue(SettingsOptions::CoolingTempBlink)); }

#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
static void displayBuzzerEnabled(void) { Display::drawCheckbox(getSettingValue(SettingsOptions::BuzzerEnabled)); }
static void displayButtonLight(void) {
  if (getSettingValue(SettingsOptions::ButtonLight)) {
    Display::printNumber(getSettingValue(SettingsOptions::ButtonLight), 2, FontStyle::LARGE);
  } else {
    SETTING_VALUE_OFF();
  }
}
static void displayLowVoltageAllowed(void) { Display::drawCheckbox(getSettingValue(SettingsOptions::LowVoltageAllowed)); }

// "Debug pages" menu entry: a menu handler cannot change screen itself, so it leaves a request
// that the card menu (SettingsMenu.cpp) picks up straight after calling it.
bool        hs02DebugPagesRequested = false;
static void setOpenDebugPages(void) { hs02DebugPagesRequested = true; }

// ---- "Calibrate at 100C" --------------------------------------------------------------------
// The curve in Fnirsi/ThermoModel.cpp has no measured point between the cold end (25 C) and
// 223 C. This stores one: the tip amplifier's reading while the tip is held in boiling water,
// with the heater off (it is off everywhere in the menu). Water boils about 1 C lower for every
// 300 m above sea level; 100 C is assumed.
// Any reading that carries a signal is stored: what an iron reads at 100 C is the thing being
// measured, and it differs too much between irons to be second-guessed (the curves put it at
// roughly 1700 to 2400 uV; the first real HS-02A tried read 1300). Two readings are refused
// because they cannot be used: one within 50 uV of the zero reading (a few ADC counts: the tip
// is not in anything hot, or the amplifier is still at its lower limit), and one above 6000 uV,
// the most the Cal100uV setting holds.
constexpr uint32_t kCal100MinAboveZerouV = 50, kCal100MaxuV = 6000;

// A screen of small-font rows, 16 pixels apart; with a question, "no / no / yes" under it in
// button order (DOWN, UP, OK).
static void cal100DrawRows(const char *const *rows, uint8_t count, bool question) {
  Display::clearScreen();
  for (uint8_t row = 0; row < count; row++) {
    Display::setCursor(0, row * 16);
    Display::print(rows[row], FontStyle::SMALL);
  }
  if (question) {
    Display::setCursor(0, 64);
    Display::print(SmallSymbolHintNo, FontStyle::SMALL);
    Display::setCursor(54, 64);
    Display::print(SmallSymbolHintNo, FontStyle::SMALL);
    Display::setCursor(108, 64);
    Display::print(SmallSymbolHintYes, FontStyle::SMALL);
  }
  LCD::setMonoRegions(0, 0, 255, 64);
}

extern bool hs02MenuWaitForRelease; // SettingsMenu.cpp: the card menu ignores buttons until all are up

static void setCalibrate100(void) {
  const char *const ask[] = {SmallCal100Ask1, SmallCal100Ask2, SmallCal100Ask3};
  for (;;) { // OK is yes, any other button leaves
    cal100DrawRows(ask, 3, true);
    const ButtonState answer = getButtonState();
    if (answer == BUTTON_OK_SHORT) {
      break;
    }
    if (answer != BUTTON_NONE) {
      hs02MenuWaitForRelease = true; // a held button must not carry on into the menu
      return;
    }
    Display::refresh();
    osDelay(40);
  }
  // 16 samples, 100 ms apart, as the CJC calibration takes them
  uint32_t          sum    = 0;
  const char *const busy[] = {SmallCal100Busy};
  for (uint8_t sample = 0; sample < 16; sample++) {
    cal100DrawRows(busy, 1, false);
    Display::refresh();
    osDelay(TICKS_100MS);
    sum += getTipRawTemp(1);
  }
  const uint32_t readinguV = TipThermoModel::convertTipRawADCTouV(sum / 16, true);
  const uint32_t zerouV    = getSettingValue(SettingsOptions::CalibrationOffset);
  const bool plausible = readinguV >= zerouV + kCal100MinAboveZerouV && readinguV <= kCal100MaxuV;
  if (plausible) {
    setSettingValue(SettingsOptions::Cal100uV, (uint16_t)readinguV);
    saveSettings();
  }
  const char *const saved[]  = {SmallCal100Saved};
  const char *const refuse[] = {SmallCal100Bad1, SmallCal100Bad2};
  for (;;) { // the result, with the reading, until a button is pressed
    cal100DrawRows(plausible ? saved : refuse, plausible ? 1 : 2, false);
    Display::setCursor(0, plausible ? 16 : 32);
    Display::printNumber(readinguV, 5, FontStyle::SMALL);
    Display::print(SmallSymbolLiveMicrovolts, FontStyle::SMALL);
    if (getButtonState() != BUTTON_NONE) {
      hs02MenuWaitForRelease = true;
      return;
    }
    Display::refresh();
    osDelay(40);
  }
}

// A tick once a 100 C point is stored
static void displayCalibrate100(void) { Display::drawCheckbox(getSettingValue(SettingsOptions::Cal100uV) != 0); }

// Colour name for one of the accent colour settings; 0 is "Auto" (the theme's own colour).
static void displayCardColour(SettingsOptions option) {
  const char *const names[] = {LargeCardColour0, LargeCardColour1, LargeCardColour2, LargeCardColour3, LargeCardColour4, LargeCardColour5, LargeCardColour6, LargeCardColour7, LargeCardColour8};
  uint16_t          choice  = getSettingValue(option);
  if (choice >= sizeof(names) / sizeof(names[0])) {
    choice = 0;
  }
  Display::print(names[choice], FontStyle::LARGE);
}
static void displayColourReady(void) { displayCardColour(SettingsOptions::ColourReady); }
static void displayColourHeating(void) { displayCardColour(SettingsOptions::ColourHeating); }
static void displayColourBoost(void) { displayCardColour(SettingsOptions::ColourBoost); }
static void displayColourSleep(void) { displayCardColour(SettingsOptions::ColourSleep); }
static void displayColourMenu(void) { displayCardColour(SettingsOptions::ColourMenu); }
#endif

static void displayScrollSpeed(void) { Display::print(((getSettingValue(SettingsOptions::DescriptionScrollSpeed)) ? SETTING_VALUE_TEXT(SettingFastChar) : SETTING_VALUE_TEXT(SettingSlowChar)), FontStyle::LARGE); }

static void displayReverseButtonTempChangeEnabled(void) { Display::drawCheckbox(getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled)); }

#if !(defined(LCD_160x80) && defined(HS02_THEME_CARD))
static void displayReverseButtonSettings(void) { Display::drawCheckbox(getSettingValue(SettingsOptions::ReverseButtonSettings)); }
#endif

static void displayAnimationSpeed(void) {
  switch (getSettingValue(SettingsOptions::AnimationSpeed)) {
  case settingOffSpeed_t::SLOW:
    Display::print(SETTING_VALUE_TEXT(SettingSlowChar), FontStyle::LARGE);
    break;
  case settingOffSpeed_t::MEDIUM:
    Display::print(SETTING_VALUE_TEXT(SettingMediumChar), FontStyle::LARGE);
    break;
  case settingOffSpeed_t::FAST:
    Display::print(SETTING_VALUE_TEXT(SettingFastChar), FontStyle::LARGE);
    break;
  default:
    SETTING_VALUE_OFF();
    break;
  }
}

static bool displayAnimationOptions(void) { return getSettingValue(SettingsOptions::AnimationSpeed) > 0; }
static void displayAnimationLoop(void) { Display::drawCheckbox(getSettingValue(SettingsOptions::AnimationLoop)); }

static void displayBrightnessLevel(void) {
  Display::printNumber((getSettingValue(SettingsOptions::DisplayBrightness) / BRIGHTNESS_STEP + 1), 1, FontStyle::LARGE);
  // While not optimal to apply this here, it is _very_ convenient
  Display::setBrightness(getSettingValue(SettingsOptions::DisplayBrightness));
}

static void displayInvertColor(void) {
  Display::drawCheckbox(getSettingValue(SettingsOptions::DisplayInversion));
  // While not optimal to apply this here, it is _very_ convenient
  Display::setInverseDisplay(getSettingValue(SettingsOptions::DisplayInversion));
}

static void displayLogoTime(void) {
  switch (getSettingValue(SettingsOptions::LOGOTime)) {
  case logoMode_t::SKIP:
    SETTING_VALUE_OFF();
    break;
  case logoMode_t::ONETIME:
    Display::drawRepeatOnceIcon();
    break;
  case logoMode_t::INFINITY:
    Display::drawRepeatInfIcon();
    break;
  default:
    Display::printNumber(getSettingValue(SettingsOptions::LOGOTime), 1, FontStyle::LARGE);
    Display::print(LargeSymbolSeconds, FontStyle::LARGE);
    break;
  }
}

static void displayAdvancedIDLEScreens(void) { Display::drawCheckbox(getSettingValue(SettingsOptions::DetailedIDLE)); }

static void displayAdvancedSolderingScreens(void) { Display::drawCheckbox(getSettingValue(SettingsOptions::DetailedSoldering)); }

#ifdef BLE_ENABLED
static void displayBluetoothLE(void) { Display::drawCheckbox(getSettingValue(SettingsOptions::BluetoothLE)); }
#endif /* BLE_ENABLED */

static void displayPowerLimit(void) {
  if (getSettingValue(SettingsOptions::PowerLimit) == 0) {
    SETTING_VALUE_OFF();
  } else {
    Display::printNumber(getSettingValue(SettingsOptions::PowerLimit), 3, FontStyle::LARGE);
    Display::print(LargeSymbolWatts, FontStyle::LARGE);
  }
}

static void setCalibrate(void) {
  if (getSettingValue(SettingsOptions::CalibrateCJC) < 1) {
    if (userConfirmation(translatedString(Tr->SettingsCalibrationWarning))) {
      // User confirmed
      // So we now set the tick
      setSettingValue(SettingsOptions::CalibrateCJC, 1);
      saveSettings(); // at once: the next thing the user does is unplug the iron
    }
  } else {
    setSettingValue(SettingsOptions::CalibrateCJC, 0);
    saveSettings();
  }
}

static void displayCalibrate(void) { Display::drawCheckbox(getSettingValue(SettingsOptions::CalibrateCJC)); }

static void setCalibrateVIN(void) {
  // Jump to the voltage calibration subscreen
  Display::clearScreen();

  for (;;) {
    Display::setCursor(DISPLAY_WIDTH - 5 * FONT_LARGE_WIDTH, 8);
    uint16_t voltage = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
    Display::printNumber(voltage / 10, 2, FontStyle::LARGE);
    Display::print(LargeSymbolDot, FontStyle::LARGE);
    Display::printNumber(voltage % 10, 1, FontStyle::LARGE, false);
    Display::print(LargeSymbolVolts, FontStyle::LARGE);
    Display::setCursor(DISPLAY_WIDTH - 3 * FONT_SMALL_WIDTH, 48);
    Display::printNumber(getSettingValue(SettingsOptions::VoltageDiv), 3, FontStyle::SMALL);
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
    // Card theme: one colour for the whole reading (the menu's colour split would cut the
    // large digits in half), and the usual hint row: DOWN lowers the voltage shown, UP raises
    // it, OK saves.
    Display::setCursor(18, 64);
    Display::print(SmallSymbolHintLess, FontStyle::SMALL);
    Display::setCursor(54 + 18, 64);
    Display::print(SmallSymbolHintMore, FontStyle::SMALL);
    Display::setCursor(108, 64);
    Display::print(SmallSymbolHintDone, FontStyle::SMALL);
    LCD::setMonoRegions(0, 0, 255, 64);
#endif

    switch (getButtonState()) {
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
    case BUTTON_F_SHORT: // DOWN: a larger divider shows a lower voltage
      nextSettingValue(SettingsOptions::VoltageDiv);
      break;
    case BUTTON_B_SHORT: // UP
      prevSettingValue(SettingsOptions::VoltageDiv);
      break;
    case BUTTON_OK_SHORT:
    case BUTTON_OK_LONG:
#else
    case BUTTON_F_SHORT:
      prevSettingValue(SettingsOptions::VoltageDiv);
      break;
    case BUTTON_B_SHORT:
      nextSettingValue(SettingsOptions::VoltageDiv);
      break;
#endif
    case BUTTON_BOTH:
    case BUTTON_F_LONG:
    case BUTTON_B_LONG:
      saveSettings();
      Display::clearScreen();
      Display::setCursor(0, 0);
      warnUser(translatedString(Tr->CalibrationDone), getButtonState());
      Display::refresh();
      waitForButtonPressOrTimeout(0.5 * TICKS_SECOND);
      return;
    case BUTTON_NONE:
    default:
      break;
    }

    Display::refresh();
    osDelay(40);
  }
}

static void displayPowerPulse(void) {
  if (getSettingValue(SettingsOptions::KeepAwakePulse)) {
    Display::printNumber(getSettingValue(SettingsOptions::KeepAwakePulse) / 10, 1, FontStyle::LARGE);
    Display::print(LargeSymbolDot, FontStyle::LARGE);
    Display::printNumber(getSettingValue(SettingsOptions::KeepAwakePulse) % 10, 1, FontStyle::LARGE);
  } else {
    SETTING_VALUE_OFF();
  }
}

static bool showPowerPulseOptions(void) { return getSettingValue(SettingsOptions::KeepAwakePulse) > 0; }

static void displayPowerPulseWait(void) { Display::printNumber(getSettingValue(SettingsOptions::KeepAwakePulseWait), 1, FontStyle::LARGE); }

static void displayPowerPulseDuration(void) { Display::printNumber(getSettingValue(SettingsOptions::KeepAwakePulseDuration), 1, FontStyle::LARGE); }

static void setResetSettings(void) {
  if (userConfirmation(translatedString(Tr->SettingsResetWarning))) {
    resetSettings();
    Display::clearScreen();
    while (!warnUser(translatedString(Tr->ResetOKMessage), getButtonState())) {
      Display::refresh();
      vTaskDelay(TICKS_100MS);
      Display::clearScreen();
    }
    reboot();
  }
}

// Indicates whether a menu transition is in progress, so that the menu icon
// animation is paused during the transition.
static bool animOpenState = false;

static void displayMenu(size_t index) {
  // Call into the menu
  // Draw title
  Display::printWholeScreen(translatedString(Tr->SettingsMenuEntries[index]));
  static TickType_t menuSwitchLoopTick = 0;
  static size_t     menuCurrentIndex   = sizeof(rootSettingsMenu) + 1;
  TickType_t        step               = TICKS_100MS * 5;
  switch (getSettingValue(SettingsOptions::AnimationSpeed)) {
  case settingOffSpeed_t::FAST:
    step = TICKS_100MS * 3;
    break;
  case settingOffSpeed_t::MEDIUM:
    step = TICKS_100MS * 4;
    break;
  default: // SLOW or off - defaulted above
    break;
  }
  size_t currentFrame;
  if (!animOpenState && (getSettingValue(SettingsOptions::AnimationSpeed) != settingOffSpeed_t::OFF)) {
    if (menuCurrentIndex != index) {
      menuCurrentIndex   = index;
      menuSwitchLoopTick = xTaskGetTickCount();
    }
    currentFrame = ((xTaskGetTickCount() - menuSwitchLoopTick) / step);
    if (getSettingValue(SettingsOptions::AnimationLoop)) {
      currentFrame %= 3;
    } else if (currentFrame > 2) {
      currentFrame = 2;
    }
  } else {
    // We want the animation to restart after completing the transition.
    menuCurrentIndex = sizeof(rootSettingsMenu) + 1;
    // Always draw the last frame if icon animation is disabled.
    currentFrame = getSettingValue(SettingsOptions::AnimationSpeed) == settingOffSpeed_t::OFF ? 2 : 0;
  }
  // Draw symbol
  // 16 pixel wide image
  // less 2 pixel wide scrolling indicator

  Display::drawArea(DISPLAY_WIDTH - SETTINGS_ICON_WIDTH - 2, 0, SETTINGS_ICON_WIDTH, SETTINGS_ICON_HEIGHT,
                    (&SettingsMenuIcons[index][(SETTINGS_ICON_WIDTH * (SETTINGS_ICON_HEIGHT / 8)) * currentFrame]));
}

#if defined(POW_DC) || defined(POW_QC) || defined(POW_PD)
static void displayPowerMenu(void) { displayMenu(0); }

#endif /* POW_DC or POW_QC */

static void displaySolderingMenu(void) { displayMenu(1); }

static void displayPowerSavingMenu(void) { displayMenu(2); }

static void displayUIMenu(void) { displayMenu(3); }

static void displayAdvancedMenu(void) { displayMenu(4); }
