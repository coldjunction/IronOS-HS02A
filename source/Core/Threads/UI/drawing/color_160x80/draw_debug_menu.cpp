#include "OperatingModes.h"
#include "TipThermoModel.h"
#include "main.hpp"
#include "power.hpp"
#include "ui_drawing.hpp"

#ifdef LCD_160x80
extern osThreadId GUITaskHandle;
extern osThreadId MOVTaskHandle;
extern osThreadId PIDTaskHandle;

// Item 17: raw readings of the heater current sense input, collected by
// hs02CurrentSenseSample() in Core/BSP/Fnirsi/BSP.cpp. Five rows of the small font:
//   I sense raw
//   off  NNNN     heater off
//   on   NNNN     at the highest heater level seen since power-on
//   peak NNNN     highest single sample while heating
//   pwm NNN VV.V  heater level (%) and input voltage "on" was taken at
static void drawCurrentSensePage(void) {
  extern void hs02GetCurrentSense(uint16_t &offMean, uint16_t &onMean, uint16_t &peak, uint8_t &onPwmPercent, uint16_t &onVoltsX10);
  uint16_t    offMean, onMean, peak, onVoltsX10;
  uint8_t     onPwmPercent;
  hs02GetCurrentSense(offMean, onMean, peak, onPwmPercent, onVoltsX10);
  const uint8_t valueColumn = 5 * FONT_SMALL_WIDTH;
  Display::setCursor(0, 0);
  Display::print(SmallSymbolCurrentSense, FontStyle::SMALL);
  Display::setCursor(0, 16);
  Display::print(SmallSymbolCurrentOff, FontStyle::SMALL);
  Display::setCursor(valueColumn, 16);
  Display::printNumber(offMean, 4, FontStyle::SMALL);
  Display::setCursor(0, 32);
  Display::print(SmallSymbolCurrentOn, FontStyle::SMALL);
  Display::setCursor(valueColumn, 32);
  Display::printNumber(onMean, 4, FontStyle::SMALL);
  Display::setCursor(0, 48);
  Display::print(SmallSymbolCurrentPeak, FontStyle::SMALL);
  Display::setCursor(valueColumn, 48);
  Display::printNumber(peak, 4, FontStyle::SMALL);
  Display::setCursor(0, 64);
  Display::print(SmallSymbolCurrentPwm, FontStyle::SMALL);
  Display::setCursor(4 * FONT_SMALL_WIDTH, 64);
  Display::printNumber(onPwmPercent, 3, FontStyle::SMALL);
  Display::setCursor(8 * FONT_SMALL_WIDTH, 64);
  Display::printNumber(onVoltsX10 / 10, 2, FontStyle::SMALL);
  Display::print(SmallSymbolDot, FontStyle::SMALL);
  Display::printNumber(onVoltsX10 % 10, 1, FontStyle::SMALL);
  Display::print(SmallSymbolVolts, FontStyle::SMALL);
}

// Item 18: what the temperature reading is made from, live. Five rows of the small font:
//   raw  NNNN     tip sensor, 12-bit ADC counts (0..4095)
//   uV   NNNNN    the same as microvolts at the amplifier input, before the offset is removed
//   ofs  NNNN     the offset that is removed (the CJC calibration value), microvolts
//   mcu  NN.N     the MCU's own temperature, degrees C
//   pwr  NN.NW    heater power, watts
static void drawLiveSensorPage(void) {
  const uint8_t  valueColumn = 5 * FONT_SMALL_WIDTH;
  const uint16_t raw         = getTipRawTemp(0);
  Display::setCursor(0, 0);
  Display::print(SmallSymbolLiveRaw, FontStyle::SMALL);
  Display::setCursor(valueColumn, 0);
  Display::printNumber(raw / 8, 5, FontStyle::SMALL);
  Display::setCursor(0, 16);
  Display::print(SmallSymbolLiveMicrovolts, FontStyle::SMALL);
  Display::setCursor(valueColumn, 16);
  Display::printNumber(TipThermoModel::convertTipRawADCTouV(raw, true), 5, FontStyle::SMALL);
  Display::setCursor(0, 32);
  Display::print(SmallSymbolLiveOffset, FontStyle::SMALL);
  Display::setCursor(valueColumn, 32);
  Display::printNumber(getSettingValue(SettingsOptions::CalibrationOffset), 5, FontStyle::SMALL);
  const uint16_t mcu = getHandleTemperature(0);
  Display::setCursor(0, 48);
  Display::print(SmallSymbolLiveMcu, FontStyle::SMALL);
  Display::setCursor(valueColumn, 48);
  Display::printNumber(mcu / 10, 3, FontStyle::SMALL);
  Display::print(SmallSymbolDot, FontStyle::SMALL);
  Display::printNumber(mcu % 10, 1, FontStyle::SMALL);
  const uint32_t watts = x10WattHistory.average();
  Display::setCursor(0, 64);
  Display::print(SmallSymbolLivePower, FontStyle::SMALL);
  Display::setCursor(valueColumn, 64);
  Display::printNumber(watts / 10, 3, FontStyle::SMALL);
  Display::print(SmallSymbolDot, FontStyle::SMALL);
  Display::printNumber(watts % 10, 1, FontStyle::SMALL);
  Display::print(SmallSymbolWatts, FontStyle::SMALL);
}

void ui_draw_debug_menu(const uint8_t item_number) {
  if (item_number == 18) { // no entry in DebugMenu[] either
    drawLiveSensorPage();
    return;
  }
  if (item_number == 17) { // has no entry in DebugMenu[], so it must not reach the lookup below
    drawCurrentSensePage();
    return;
  }
  Display::setCursor(0, 0);                                   // Position the cursor at the 0,0 (top left)
  Display::print(SmallSymbolVersionNumber, FontStyle::SMALL); // Print version number
  Display::setCursor(0, 24);                                  // second line
  Display::print(DebugMenu[item_number], FontStyle::SMALL);
  Display::setCursor(LCD_WIDTH - 6 * FONT_SMALL_WIDTH, 48); // third line
  uint16_t tmp;
  switch (item_number) {
  case 0: // Build Date, and under it the number of this custom build
    Display::setCursor(0, 48);
    Display::print(SmallSymbolCustomBuild, FontStyle::SMALL);
    break;
  case 1: // Device ID
  {
    uint64_t id = getDeviceID();
#ifdef DEVICE_HAS_VALIDATION_CODE
    // If device has validation code; then we want to take over both lines of the screen
    Display::clearScreen();   // Ensure the buffer starts clean
    Display::setCursor(0, 0); // Position the cursor at the 0,0 (top left)
    Display::print(DebugMenu[item_number], FontStyle::SMALL);
    Display::drawHex(getDeviceValidation(), FontStyle::SMALL, 8);
    Display::setCursor(0, 16); // second line
#endif
    Display::setCursor(36, 24);
    Display::drawHex((uint32_t)(id >> 32), FontStyle::SMALL, 8);
    Display::setCursor(36, 48);
    Display::drawHex((uint32_t)(id & 0xFFFFFFFF), FontStyle::SMALL, 8);
  } break;
  case 2: // ACC Type
    Display::setCursor(0, 48);
    Display::print(AccelTypeNames[(int)DetectedAccelerometerVersion], FontStyle::SMALL);
    break;
  case 3: // Power Negotiation Status
    Display::setCursor(0, 48);
    Display::print(PowerSourceNames[getPowerSourceNumber()], FontStyle::SMALL);
    break;
  case 4: // Input Voltage
    Display::print(SmallSymbolSpace, FontStyle::SMALL);
    Display::print(SmallSymbolSpace, FontStyle::SMALL);
    printVoltage();
    break;
  case 5: // Temp in °C
    Display::printNumber(TipThermoModel::getTipInC(), 5, FontStyle::SMALL);
    break;
  case 6: // Handle Temp in °C
    tmp = getHandleTemperature(0);
    Display::printNumber(tmp / 10, 4, FontStyle::SMALL);
    Display::print(SmallSymbolDot, FontStyle::SMALL);
    Display::printNumber(tmp % 10, 1, FontStyle::SMALL);
    break;
  case 7: // Max Temp Limit in °C
    Display::printNumber(TipThermoModel::getTipMaxInC(), 5, FontStyle::SMALL);
    break;
  case 8: // System Uptime
    Display::printNumber(xTaskGetTickCount() / TICKS_100MS, 5, FontStyle::SMALL);
    break;
  case 9: // Movement Timestamp
    Display::printNumber(lastMovementTime / TICKS_100MS, 5, FontStyle::SMALL);
    break;
  case 10: // Tip Resistance in Ω
    Display::printNumber(getTipResistanceX10() / 10, 4, FontStyle::SMALL);
    Display::print(SmallSymbolDot, FontStyle::SMALL);
    Display::printNumber(getTipResistanceX10() % 10, 1, FontStyle::SMALL);
    break;
  case 11: // Raw Tip in µV
    Display::printNumber(TipThermoModel::convertTipRawADCTouV(getTipRawTemp(0), true), 5, FontStyle::SMALL);
    break;
  case 12: // Tip Cold Junction Compensation Offset in µV
    Display::printNumber(getSettingValue(SettingsOptions::CalibrationOffset), 5, FontStyle::SMALL);
    break;
  case 13: // High Water Mark for GUI
    Display::printNumber(uxTaskGetStackHighWaterMark(GUITaskHandle), 5, FontStyle::SMALL);
    break;
  case 14: // High Water Mark for Movement Task
    Display::printNumber(uxTaskGetStackHighWaterMark(MOVTaskHandle), 5, FontStyle::SMALL);
    break;
  case 15: // High Water Mark for PID Task
    Display::printNumber(uxTaskGetStackHighWaterMark(PIDTaskHandle), 5, FontStyle::SMALL);
    break;
    break;
#ifdef HALL_SENSOR
  case 16: // Raw Hall Effect Value
  {
    int16_t hallEffectStrength = getRawHallEffect();
    if (hallEffectStrength < 0) {
      hallEffectStrength = -hallEffectStrength;
    }
    Display::printNumber(hallEffectStrength, 6, FontStyle::SMALL);
  } break;
#else
  case 16: // HS-02 factory tip calibration readout
  {
    // Fnirsi BSP: three stock calibration ADC counts (140C/240C/340C reference
    // points) decoded from the stock settings page; see Core/BSP/Fnirsi/ThermoModel.cpp.
    // "In Use" means the piecewise curve built from them drives the tip readout;
    // "Custom Curve" means they were absent/neutral placeholders and the measured
    // absolute-temperature fallback is active instead.
    extern bool hs02GetFactoryTipCal(uint32_t &a140, uint32_t &a240, uint32_t &a340);
    uint32_t    a140, a240, a340;
    const bool  calValid = hs02GetFactoryTipCal(a140, a240, a340);
    Display::clearScreen();
    Display::setCursor(0, 0);
    Display::print(SmallSymbolTipCal, FontStyle::SMALL); // "Tip Cal"
    Display::setCursor(0, 24);
    Display::printNumber(a140, 3, FontStyle::SMALL, true);
    Display::print(SmallSymbolSpace, FontStyle::SMALL);
    Display::printNumber(a240, 3, FontStyle::SMALL, true);
    Display::print(SmallSymbolSpace, FontStyle::SMALL);
    Display::printNumber(a340, 4, FontStyle::SMALL, true);
    Display::setCursor(0, 48);
    Display::print(calValid ? SmallSymbolTipCalInUse : SmallSymbolTipCalUnused, FontStyle::SMALL);
  } break;
#endif

  default:
    break;
  }
}
#endif
