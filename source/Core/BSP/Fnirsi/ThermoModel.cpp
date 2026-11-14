/*
 * ThermoModel.cpp
 *
 *  Created on: 1 May 2021
 *      Author: Ralim, MrTick
 */
#include "Settings.h"
#include "Setup.h"
#include "TipThermoModel.h"
#include "Types.h"
#include "Utils.hpp"
#include "configuration.h"

extern uint16_t tipSenseResistancex10Ohms;

/*
 * HS-02 factory temperature calibration (from stock firmware reverse engineering).
 *
 * The stock firmware stores three per-unit calibration words in its settings page
 * at flash 0x0801F800 (word offsets 0x16/0x17/0x18 = byte offsets 0x58/0x5C/0x60).
 * Each word minus a fixed bias yields the raw 12-bit ADC count measured at a
 * reference tip temperature during factory calibration:
 *
 *   count(140C) = word[0x16] - 0x7F21
 *   count(240C) = word[0x17] - 0x7E77
 *   count(340C) = word[0x18] - 0x7DC4
 *
 * Stock converts ADC counts to degC with a piecewise-linear curve through these
 * points (the thermocouple's uV/C slope rises with temperature, so a single
 * constant slope reads increasingly high in the soldering range). IronOS keeps
 * its own settings at 0x0801E000-0x0801F000, so the stock page survives flashing
 * IronOS and we can reuse the factory calibration directly.
 */
namespace {
constexpr uint32_t stockSettingsPageAddr = 0x0801F800;
constexpr uint32_t stockCalBias140       = 0x7F21;
constexpr uint32_t stockCalBias240       = 0x7E77;
constexpr uint32_t stockCalBias340       = 0x7DC4;

uint32_t interpolateFallbackTempCx10(uint32_t tipuV, uint32_t loweruV, uint32_t lowerTempCx10, uint32_t upperuV, uint32_t upperTempCx10) {
  return lowerTempCx10 + ((tipuV - loweruV) * (upperTempCx10 - lowerTempCx10)) / (upperuV - loweruV);
}

uint32_t fallbackTipTempCx10(uint32_t tipuV) {
  // Absolute tip temperature from the thermocouple signal, for units without a usable factory
  // calibration.
  //
  // The curve is in SIGNAL microvolts: the amplifier reading minus that unit's zero reading (its
  // reading at no temperature difference). The points from 223 C up were measured by the
  // port's author on one HS-02A, with a zero of 900 uV taken off. The cold end: at no signal
  // the tip is as warm as the point it plugs into, which no sensor on this board measures;
  // 25 C is assumed, as the stock firmware does. The 100 C point (2000 uV) is not measured: it
  // is the straight line from 25 C to 223 C, bent by the 3 % that the stock firmware's own
  // curve is flatter by at the low end.
  //
  // The zero differs from unit to unit by more than the curve can ignore: 100 uV is about 4 C
  // at soldering temperatures. There are two ways to find it:
  //   - "Calibrate CJC" reads the amplifier with a cold tip. That is exact if the zero is above
  //     0. If it is below, the amplifier's output sits at its lower limit, the reading is 0,
  //     and the true zero stays unknown.
  //   - "Calibrate at 100C" reads it with the tip in boiling water. The zero is then that
  //     reading minus the signal expected at 100 C, and it may come out negative. Measured on
  //     one HS-02A: 0 uV cold and about 1300 uV at 100 C, so a zero near -700 uV. Without this
  //     correction that iron showed about 40 C in boiling water and, by the same 1.5 mV or so,
  //     far too little at soldering temperatures (the tip hotter than shown).
  constexpr int32_t kSignalAt100 = 2000;
  constexpr int32_t s223 = 5400, s273 = 6680, s350 = 8370, s400 = 10540, s450 = 12090, s500 = 13640;

  const int32_t offset = getSettingValue(SettingsOptions::CalibrationOffset);
  const int32_t cal100 = getSettingValue(SettingsOptions::Cal100uV);
  const int32_t input  = (int32_t)tipuV + offset; // the amplifier reading; tipuV has the offset taken off

  // The two calibrations each give a figure for the zero: the CJC offset, and the 100 C reading
  // minus the signal expected at 100 C. The LOWER of the two is used. A zero that is too low
  // makes temperatures read high, and the tip is then cooler than shown; a zero that is too
  // high makes them read low and the controller overheats the tip. The ways a calibration goes
  // wrong all push its figure up (CJC run on a tip that was still warm, the 100 C reading taken
  // from a tip hotter than the water), except water that is not boiling, which errs the safe way.
  int32_t zero  = offset;
  int32_t at100 = kSignalAt100; // signal at 100 C
  if (cal100 != 0) {
    const int32_t zeroFrom100 = cal100 - kSignalAt100; // may be negative
    if (zeroFrom100 < zero) {
      zero = zeroFrom100;
    } else if (cal100 - zero < s223) {
      at100 = cal100 - zero; // the zero stands: the 100 C reading shapes the low end of the curve instead
    }
  }

  int32_t signal = input - zero;
  if (zero < offset) {
    // No reading arrives here below the offset: the amplifier cannot go below 0, and the caller
    // cuts readings off at the offset. With the zero below that, a reading at the offset only
    // says "no warmer than this". Rather than show that limit for a cold iron, and jump to it
    // from 25 C with the first count, the first 300 uV above the offset run from 25 C up to
    // where the curve is true again.
    constexpr int32_t kBlend = 300;
    if (input < offset + kBlend) {
      signal = ((input - offset) * (offset + kBlend - zero)) / kBlend;
    }
  }
  if (signal <= 0) {
    return 250;
  }
  const uint32_t x = (uint32_t)signal;
  if (x <= (uint32_t)at100) {
    return interpolateFallbackTempCx10(x, 0, 250, at100, 1000);
  }
  if (x <= (uint32_t)s223) {
    return interpolateFallbackTempCx10(x, at100, 1000, s223, 2230);
  }
  if (x <= (uint32_t)s273) {
    return interpolateFallbackTempCx10(x, s223, 2230, s273, 2730);
  }
  if (x <= (uint32_t)s350) {
    return interpolateFallbackTempCx10(x, s273, 2730, s350, 3500);
  }
  if (x <= (uint32_t)s400) {
    return interpolateFallbackTempCx10(x, s350, 3500, s400, 4000);
  }
  if (x <= (uint32_t)s450) {
    return interpolateFallbackTempCx10(x, s400, 4000, s450, 4500);
  }
  return interpolateFallbackTempCx10(x, s450, 4500, s500, 5000);
}

bool     factoryCalChecked = false;
bool     factoryCalValid   = false;
uint32_t adcCount140       = 0; // Raw 12-bit ADC counts at the three factory reference points
uint32_t adcCount240       = 0;
uint32_t adcCount340       = 0;

void loadFactoryCal() {
  factoryCalChecked    = true;
  const uint32_t *page = (const uint32_t *)stockSettingsPageAddr;
  if (page[0] == 0xFFFFFFFF) {
    return; // Erased page; stock firmware never ran / settings wiped
  }
  const int32_t a140 = (int32_t)(page[0x16] & 0xFFFF) - (int32_t)stockCalBias140;
  const int32_t a240 = (int32_t)(page[0x17] & 0xFFFF) - (int32_t)stockCalBias240;
  const int32_t a340 = (int32_t)(page[0x18] & 0xFFFF) - (int32_t)stockCalBias340;
  // Always expose the decoded counts for the debug menu, even when rejected below.
  adcCount140 = (a140 > 0) ? (uint32_t)a140 : 0;
  adcCount240 = (a240 > 0) ? (uint32_t)a240 : 0;
  adcCount340 = (a340 > 0) ? (uint32_t)a340 : 0;
  // Units that never received a calibration carry the 0x8000 neutral placeholder
  // in all three words (decoding to 223/393/572). Verified on real hardware that
  // the resulting nominal curve over-reads such units by 25-30%, so placeholders
  // must be rejected in favour of the measured fallback slope.
  if ((page[0x16] & 0xFFFF) == 0x8000 && (page[0x17] & 0xFFFF) == 0x8000 && (page[0x18] & 0xFFFF) == 0x8000) {
    return;
  }
  // Sanity: counts must be positive, strictly increasing and within the 12-bit range.
  if (a140 < 50 || a240 <= a140 || a340 <= a240 || a340 > 1500) {
    return;
  }
  factoryCalValid = true;
}
} // namespace

// Debug-menu accessor: returns true when the stock factory calibration was found
// and passed validation; outputs the three raw ADC counts (140C/240C/340C points).
bool hs02GetFactoryTipCal(uint32_t &a140, uint32_t &a240, uint32_t &a340) {
  if (!factoryCalChecked) {
    loadFactoryCal();
  }
  a140 = adcCount140;
  a240 = adcCount240;
  a340 = adcCount340;
  return factoryCalValid;
}

TemperatureType_t TipThermoModel::convertuVToDegCx10(uint32_t tipuVDelta) {
  if (!factoryCalChecked) {
    loadFactoryCal();
  }

  if (factoryCalValid) {
    // Convert IronOS uV back to stock 12-bit ADC counts (x10 for precision) by
    // inverting the exact forward conversion used in convertTipRawADCTouV:
    //   uV = raw * (ADC_VDD_MV * 1000 / ADC_MAX_READING) / OP_AMP_GAIN_STAGE
    // with stockCount = raw / 8 (the x8 oversampling). The gain assumption
    // cancels out since the same constants were used to produce tipuVDelta.
    //   countX10 = uV * OP_AMP_GAIN_STAGE * 4096 * 10 / (ADC_VDD_MV * 1000)
    //            = uV * (233472/33) / 10000 = uV * 0.70749 for the 57x gain stage
    const uint32_t countX10 = (tipuVDelta * ((OP_AMP_GAIN_STAGE * 4096) / 33)) / 10000;

    // Stock piecewise-linear curve; last segment also extrapolates above 340C.
    uint32_t tempX10;
    if (countX10 <= adcCount140 * 10) {
      tempX10 = (countX10 * 140) / adcCount140;
    } else if (countX10 <= adcCount240 * 10) {
      tempX10 = 1400 + ((countX10 - adcCount140 * 10) * 100) / (adcCount240 - adcCount140);
    } else {
      tempX10 = 2400 + ((countX10 - adcCount240 * 10) * 100) / (adcCount340 - adcCount240);
    }
    // The stock curve outputs ABSOLUTE tip temperature (factory calibration was done
    // at ~25C ambient, baked into the curve), but this function must return the
    // delta above the handle; the caller adds the handle temperature back on top.
    return (tempX10 > 250) ? (tempX10 - 250) : 0;
  }

  // The MCU die sensor remains around 41-43C and is not the connector's cold
  // junction temperature. Use the measured absolute-temperature curve, then
  // cancel the handle value which the generic caller adds back.
  // The result may be negative: the MCU reading the caller adds back is usually above 25C, and
  // clamping here made that reading, not the curve, the lowest temperature ever shown.
  const TemperatureType_t handleTempCx10 = getHandleTemperature(0);
  const TemperatureType_t tipTempCx10    = fallbackTipTempCx10(tipuVDelta);
  return tipTempCx10 - handleTempCx10;
}
