#include "FS2711.hpp"
#include "HUB238.hpp"
#include "OperatingModes.h"
#include "ui_drawing.hpp"

static bool showStartupWarning(const char *warning, const ButtonState buttons, guiContext *cxt) {
  // GUIThread enables the HS-02 backlight after this warning frame is flushed.
  cxt->scratch_state.state7 = 1;
  return warnUser(warning, buttons);
}

static OperatingMode showWarningsStep(const ButtonState buttons, guiContext *cxt) {
  // Display alert if settings were reset

  switch (cxt->scratch_state.state1) {
  case 0: // Settings reset warning
    if (settingsWereReset) {
      if (showStartupWarning(translatedString(Tr->SettingsResetMessage), buttons, cxt)) {
        settingsWereReset         = false;
        cxt->scratch_state.state1 = 1;
      }
    } else {
      cxt->scratch_state.state1 = 1;
    }
    break;
  case 1: // Device validations
#ifdef DEVICE_HAS_VALIDATION_SUPPORT
    if (getDeviceValidationStatus()) {
      // Warn user this device might be counterfeit
      if (showStartupWarning(translatedString(Tr->DeviceFailedValidationWarning), buttons, cxt)) {
        cxt->scratch_state.state1 = 2;
      }
    } else {
      cxt->scratch_state.state1 = 2;
    }
#else
    cxt->scratch_state.state1 = 2;
#endif
    break;
  case 2: // Accelerometer detection
#ifdef NO_ACCEL
    cxt->scratch_state.state1 = 3;
#else
    if (DetectedAccelerometerVersion == AccelType::Scanning) {
      break;
    }
    // Display alert if accelerometer is not detected
    if (DetectedAccelerometerVersion == AccelType::None) {
      if (getSettingValue(SettingsOptions::AccelMissingWarningCounter) < 2) {

        if (showStartupWarning(translatedString(Tr->NoAccelerometerMessage), buttons, cxt)) {
          cxt->scratch_state.state1 = 3;
          nextSettingValue(SettingsOptions::AccelMissingWarningCounter);
          saveSettings();
        }
      } else {
        cxt->scratch_state.state1 = 3;
      }
    } else {
      cxt->scratch_state.state1 = 3;
    }
#endif

    break;
  case 3:

#ifdef POW_PD
    // We expect pd to be present
    if (!USBPowerDelivery::fusbPresent()) {
      if (getSettingValue(SettingsOptions::PDMissingWarningCounter) < 2) {
        if (showStartupWarning(translatedString(Tr->NoPowerDeliveryMessage), buttons, cxt)) {
          nextSettingValue(SettingsOptions::PDMissingWarningCounter);
          saveSettings();
          cxt->scratch_state.state1 = 4;
        }
      } else {
        cxt->scratch_state.state1 = 4;
      }
    } else {
      cxt->scratch_state.state1 = 4;
    }
#else
#if POW_PD_EXT == 1
    if (!hub238_probe()) {
      if (getSettingValue(SettingsOptions::PDMissingWarningCounter) < 2) {
        if (showStartupWarning(translatedString(Tr->NoPowerDeliveryMessage), buttons, cxt)) {
          cxt->scratch_state.state1 = 4;
          nextSettingValue(SettingsOptions::PDMissingWarningCounter);
          saveSettings();
        }
      } else {
        cxt->scratch_state.state1 = 4;
      }
    } else {
      cxt->scratch_state.state1 = 4;
    }
#else
#if POW_PD_EXT == 2
    if (!FS2711::probe()) {
      if (getSettingValue(SettingsOptions::PDMissingWarningCounter) < 2) {
        if (showStartupWarning(translatedString(Tr->NoPowerDeliveryMessage), buttons, cxt)) {
          cxt->scratch_state.state1 = 4;
          nextSettingValue(SettingsOptions::PDMissingWarningCounter);
          saveSettings();
        }
      } else {
        cxt->scratch_state.state1 = 4;
      }
    } else {
      cxt->scratch_state.state1 = 4;
    }
#else
    cxt->scratch_state.state1 = 4;
#endif /*POW_PD_EXT==1*/
#endif /*POW_PD_EXT==2*/
#endif /*POW_PD*/

    break;
  default:
    // We are off the end, warnings done
    return OperatingMode::StartupLogo;
  }

  return OperatingMode::StartupWarnings; // Stay in warnings
}

// One check per call above. A check with nothing to show goes straight on to the next in the
// same frame, instead of spending a blank frame each (five of them on a unit with no warnings).
// It stops for the frame when a check is waiting (state unchanged) or drew a warning (state7).
OperatingMode showWarnings(const ButtonState buttons, guiContext *cxt) {
  for (;;) {
    const uint16_t      step = cxt->scratch_state.state1;
    const OperatingMode next = showWarningsStep(buttons, cxt);
    if (next != OperatingMode::StartupWarnings || cxt->scratch_state.state1 == step || cxt->scratch_state.state7 != 0) {
      return next;
    }
  }
}
