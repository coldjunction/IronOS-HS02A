#include "ui_drawing.hpp"
#include "configuration.h"
// Boards may turn + and - round by default (see Fnirsi/configuration.h); the menu setting then swaps them back.
#ifndef TEMP_CHANGE_SWAP_DEFAULT
#define TEMP_CHANGE_SWAP_DEFAULT 0
#endif

#ifdef LCD_160x80
void ui_draw_temperature_change(void) {
  Display::setCursor(2, 24);
  Display::print(((getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled) != 0) != (TEMP_CHANGE_SWAP_DEFAULT != 0)) ? LargeSymbolMinus : LargeSymbolPlus, FontStyle::LARGE);

  Display::print(SmallSymbolSpace, FontStyle::SMALL);
  Display::printNumber(getSettingValue(SettingsOptions::SolderingTemp), 3, FontStyle::LARGE);
  Display::printSymbolDeg(FontStyle::EXTRAS);
  Display::print(SmallSymbolSpace, FontStyle::SMALL);
  Display::print(((getSettingValue(SettingsOptions::ReverseButtonTempChangeEnabled) != 0) != (TEMP_CHANGE_SWAP_DEFAULT != 0)) ? LargeSymbolPlus : LargeSymbolMinus, FontStyle::LARGE);
}
#endif
