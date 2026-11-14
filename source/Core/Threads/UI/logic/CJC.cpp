#include "OperatingModes.h"
#include "ui_drawing.hpp"

#ifdef MODEL_HS02
// The HS-02 has no handle sensor (getHandleTemperature() is the MCU's own, warmer, reading), so
// "tip and handle agree" cannot be tested, and until the offset is right a tip of up to some
// 50-60 C reads as 25 C. Two things can be tested: the tip is not plainly hot, and the board is
// not warm from recent use. A warm board does not refuse the calibration, it asks: the MCU's
// sensor is not accurate enough to be the only judge, and the user knows if the iron was on.
constexpr uint16_t kHs02CjcBoardWarmCx10 = 350;

// true: carry on with the calibration. false: return *leaveTo.
static bool hs02CjcMayRun(const ButtonState buttons, guiContext *cxt, OperatingMode *leaveTo) {
  if (isTipDisconnected() || TipThermoModel::getTipInC() >= 40) {
    *leaveTo = OperatingMode::StartupWarnings; // not now; it stays requested for the next start
    return false;
  }
  if (cxt->scratch_state.state5 != 0) {
    return true; // settled on an earlier frame
  }
  if (cxt->scratch_state.state6 == 0 && getHandleTemperature(0) < kHs02CjcBoardWarmCx10) {
    cxt->scratch_state.state5 = 1;
    return true;
  }
  cxt->scratch_state.state6 = 1; // once asked, the question stays up whatever the reading does
  Display::setColorMode(false);
  Display::clearScreen();
  const char *const rows[] = {SmallHs02CjcWarm1, SmallHs02CjcWarm2, SmallHs02CjcWarm3};
  for (uint8_t row = 0; row < 3; row++) {
    Display::setCursor(0, row * 16);
    Display::print(rows[row], FontStyle::SMALL);
  }
  // hint row in button order (DOWN, UP, OK): no / no / yes
  Display::setCursor(0, 64);
  Display::print(SmallSymbolHintNo, FontStyle::SMALL);
  Display::setCursor(54, 64);
  Display::print(SmallSymbolHintNo, FontStyle::SMALL);
  Display::setCursor(108, 64);
  Display::print(SmallSymbolHintYes, FontStyle::SMALL);
#ifdef HS02_THEME_CARD
  LCD::setMonoRegions(0, 0, 255, 64);
#endif
  *leaveTo = OperatingMode::CJCCalibration;
  if (buttons == BUTTON_OK_SHORT) {
    cxt->scratch_state.state5 = 1; // calibrate anyway, from the next frame
  } else if (buttons != BUTTON_NONE) {
    *leaveTo = OperatingMode::StartupWarnings;
  }
  return false;
}
#endif

OperatingMode performCJCC(const ButtonState buttons, guiContext *cxt) {
  // Calibrate Cold Junction Compensation directly at boot, before internal components get warm.

  // While we wait for the pre-start checks to finish, we cant run CJC (as the pre-start checks control the tip)
  if (preStartChecks() == 0) {
    Display::setCursor(0, 0);
    Display::print(translatedString(Tr->CJCCalibrating), FontStyle::SMALL);
    return OperatingMode::CJCCalibration;
  }

#ifdef MODEL_HS02
  OperatingMode leaveTo = OperatingMode::StartupWarnings;
  if (!hs02CjcMayRun(buttons, cxt, &leaveTo)) {
    return leaveTo;
  }
  {
#else
  if (!isTipDisconnected() && abs(int(TipThermoModel::getTipInC() - getHandleTemperature(0) / 10)) < 10) {
#endif
    // Take 16 samples, only sample
    if (cxt->scratch_state.state1 < 16) {
      if ((xTaskGetTickCount() - cxt->scratch_state.state4) > TICKS_100MS) {
        cxt->scratch_state.state3 += getTipRawTemp(1);
        cxt->scratch_state.state1++;
        cxt->scratch_state.state4 = xTaskGetTickCount();
      }
      ui_draw_cjc_sampling(cxt->scratch_state.state1 / 4);
      return OperatingMode::CJCCalibration;
    }

    // If the thermo-couple at the end of the tip, and the handle are at
    // equilibrium, then the output should be zero, as there is no temperature
    // differential.

    uint16_t setOffset = TipThermoModel::convertTipRawADCTouV(cxt->scratch_state.state3 / 16, true);
    setSettingValue(SettingsOptions::CalibrationOffset, setOffset);
    if (warnUser(translatedString(Tr->CalibrationDone), buttons)) {
      // Preventing to repeat calibration at boot automatically (only one shot).
      setSettingValue(SettingsOptions::CalibrateCJC, 0);
      saveSettings();
      return OperatingMode::InitialisationDone;
    }
    return OperatingMode::CJCCalibration;
  }
  // Cant run calibration without the tip and for temps to be close
  return OperatingMode::StartupWarnings;
}
