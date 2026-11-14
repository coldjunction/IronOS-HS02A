#include "Hs02CardDigits.hpp"
#include "Hs02InterSemiBoldFont.hpp"
#include "Hs02WashFont.hpp"
#include "OperatingModes.h"
#include "TipThermoModel.h"
#include "power.hpp"
#include "ui_drawing.hpp"

#ifdef LCD_160x80

#ifdef HS02_THEME_CARD
// A card theme accent colour (RGB565): the theme's own colour for that screen, or the one
// picked with its colour setting. Also used for the menu (GUIThread.cpp).
uint16_t hs02CardAccent(SettingsOptions setting, uint16_t ownColour) {
  // Auto, Blue, Green, Red, Orange, Yellow, Purple, Pink, White
  static const uint16_t choices[] = {0, 0x3DFF, 0x262B, 0xEA28, 0xF4E1, 0xFE62, 0xA45F, 0xF396, 0xFFFF};
  const uint16_t        choice    = getSettingValue(setting);
  return (choice == 0 || choice >= sizeof(choices) / sizeof(choices[0])) ? ownColour : choices[choice];
}
#endif

namespace {
// The 2bpp framebuffer stores semantic colour indices.  Each page supplies its
// own four-colour mapping, so the Wash UI stays legible without gradients.
constexpr uint8_t  kBg = 0, kInk = 1, kAccent = 2, kMuted = 3;
constexpr uint8_t  kScreenWidth                           = 160;
constexpr uint16_t kIdlePalette[4]                        = {0x0862, 0xF77C, 0x45B8, 0x536A};
constexpr uint16_t kSolderPalette[4]                      = {0x28A0, 0xF77C, 0xFBC5, 0x9B48};
constexpr uint16_t kBoostPalette[4]                       = {0x30C0, 0xF77C, 0xFBC5, 0xB344};
constexpr uint16_t kSleepPalette[4]                       = {0x0924, 0xF77C, 0x45B8, 0x4B8F};
constexpr uint8_t  kWashGlyphI[Hs02WashFont::kGlyphBytes] = {4, 4, 0xFC, 4, 4, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0};
constexpr uint8_t  kWashGlyphU[Hs02WashFont::kGlyphBytes] = {0xFC, 0, 0, 0, 0xFC, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0};
constexpr uint8_t  kWashCustomGlyphAdvance                = 6;
constexpr uint8_t  kReadyRightEdge                        = 147;

void drawTemperatureNumber(TemperatureType_t temperature, uint8_t x, uint8_t y, uint8_t color) {
  uint8_t digits[3] = {};
  for (uint8_t i = 3; i > 0; --i) {
    digits[i - 1] = temperature % 10;
    temperature /= 10;
  }
  for (uint8_t i = 0; i < 3; ++i) {
    Display::drawBitmapColor(Hs02InterSemiBoldFont::kDigits[digits[i]], Hs02InterSemiBoldFont::kDigitWidth, Hs02InterSemiBoldFont::kDigitHeight, x + i * Hs02InterSemiBoldFont::kDigitWidth, y, color);
  }
}

// The labels are translated UTF-8 (see Hs02WashFont.hpp); everything else is ASCII.
uint8_t washGlyphAdvance(uint16_t codepoint) {
  const int16_t index = Hs02WashFont::glyphIndex(codepoint);
  return index < 0 ? 0 : Hs02WashFont::kAdvance[index];
}

uint8_t washGlyphLeftBearing(uint16_t codepoint) {
  const int16_t index = Hs02WashFont::glyphIndex(codepoint);
  if (index < 0) {
    return 0;
  }
  for (uint8_t x = 0; x < Hs02WashFont::kGlyphWidth; ++x) {
    for (uint8_t strip = 0; strip < Hs02WashFont::kGlyphBytes / Hs02WashFont::kGlyphWidth; ++strip) {
      if (Hs02WashFont::kGlyphs[index][strip * Hs02WashFont::kGlyphWidth + x]) {
        return x;
      }
    }
  }
  return 0;
}

uint8_t measureWashText(const char *text) {
  uint8_t width = 0;
  while (*text) {
    width += washGlyphAdvance(Hs02WashFont::nextCodepoint(text));
  }
  return width;
}

// y is the top of the capitals; the cell starts kTopPad rows higher for accent marks.
uint8_t drawWashText(const char *text, uint8_t x, uint8_t y, uint8_t color) {
  while (*text) {
    const uint16_t codepoint = Hs02WashFont::nextCodepoint(text);
    const int16_t  index     = Hs02WashFont::glyphIndex(codepoint);
    if (index >= 0) {
      Display::drawBitmapColor(Hs02WashFont::kGlyphs[index], Hs02WashFont::kGlyphWidth, Hs02WashFont::kGlyphHeight, x - washGlyphLeftBearing(codepoint), y - Hs02WashFont::kTopPad, color);
    }
    x += washGlyphAdvance(codepoint);
  }
  return x;
}

// The important bottom-line telemetry uses a 6:5 integer scale of the native
// 10px Fusion Pixel glyphs: a true 12px visual size without another font blob.
uint8_t drawWashTextEmphasis(const char *text, uint8_t x, uint8_t y, uint8_t color) {
  while (*text) {
    const uint16_t codepoint = Hs02WashFont::nextCodepoint(text);
    const int16_t  index     = Hs02WashFont::glyphIndex(codepoint);
    if (index >= 0) {
      const uint8_t bearing = washGlyphLeftBearing(codepoint);
      for (uint8_t gy = 0; gy < Hs02WashFont::kGlyphHeight; ++gy) {
        // Scale from the capitals' top row so accent marks land above y.
        const int8_t row = gy - Hs02WashFont::kTopPad;
        for (uint8_t gx = bearing; gx < Hs02WashFont::kGlyphWidth; ++gx) {
          const uint8_t bits = Hs02WashFont::kGlyphs[index][(gy / 8) * Hs02WashFont::kGlyphWidth + gx];
          if (bits & (1u << (gy % 8))) {
            const uint8_t sx        = (gx - bearing) * 6 / 5;
            const int8_t  sy        = row * 6 / 5;
            const uint8_t rawWidth  = ((gx - bearing + 1) * 6 / 5) - sx;
            const uint8_t rawHeight = ((row + 1) * 6 / 5) - sy;
            const uint8_t sw        = rawWidth ? rawWidth : 1;
            const uint8_t sh        = rawHeight ? rawHeight : 1;
            Display::fillRectColor(x + sx, y + sy, sw, sh, color);
          }
        }
      }
    }
    x += (washGlyphAdvance(codepoint) * 6 + 2) / 5;
  }
  return x;
}

uint8_t measureWashTextEmphasis(const char *text) {
  uint8_t width = 0;
  while (*text) {
    width += (washGlyphAdvance(Hs02WashFont::nextCodepoint(text)) * 6 + 2) / 5;
  }
  return width;
}

uint8_t digitCount(uint32_t value) {
  uint8_t count = 1;
  while (value >= 10) {
    value /= 10;
    ++count;
  }
  return count;
}

uint8_t measureWashUnsigned(uint32_t value) {
  char          digits[5];
  const uint8_t count = digitCount(value);
  for (uint8_t i = count; i > 0; --i) {
    digits[i - 1] = '0' + value % 10;
    value /= 10;
  }
  uint8_t width = 0;
  for (uint8_t i = 0; i < count; ++i) {
    width += washGlyphAdvance(digits[i]);
  }
  return width;
}

uint8_t drawWashUnsigned(uint32_t value, uint8_t x, uint8_t y, uint8_t color) {
  char    digits[5];
  uint8_t count = digitCount(value);
  for (uint8_t i = count; i > 0; --i) {
    digits[i - 1] = '0' + value % 10;
    value /= 10;
  }
  for (uint8_t i = 0; i < count; ++i) {
    char glyph[] = {digits[i], '\0'};
    x            = drawWashText(glyph, x, y, color);
  }
  return x;
}

// Value is expressed in tenths (for example 200 = 20.0).
uint8_t drawWashTenths(uint32_t value, uint8_t x, uint8_t y, uint8_t color, char unit) {
  const uint32_t whole = value / 10;
  x                    = drawWashUnsigned(whole, x, y, color);
  char tail[]          = {static_cast<char>('0' + value % 10), unit, '\0'};
  x                    = drawWashText(".", x, y, color);
  return drawWashText(tail, x, y, color);
}

// Formats a tenths value with its unit, e.g. 200 -> "20.0V". text must hold 10 chars.
void formatWashTenths(uint32_t value, char unit, char *text) {
  uint8_t  pos   = 0;
  uint32_t whole = value / 10;
  char     reversed[5];
  uint8_t  count = digitCount(whole);
  for (uint8_t i = 0; i < count; ++i) {
    reversed[i] = '0' + whole % 10;
    whole /= 10;
  }
  while (count) {
    text[pos++] = reversed[--count];
  }
  text[pos++] = '.';
  text[pos++] = '0' + value % 10;
  text[pos++] = unit;
  text[pos]   = '\0';
}

uint8_t drawWashTenthsEmphasis(uint32_t value, uint8_t x, uint8_t y, uint8_t color, char unit) {
  char text[10];
  formatWashTenths(value, unit, text);
  return drawWashTextEmphasis(text, x, y, color);
}

uint8_t measureWashTenthsEmphasis(uint32_t value, char unit) {
  char text[10];
  formatWashTenths(value, unit, text);
  return measureWashTextEmphasis(text);
}

void drawSleepCountdown() {
#ifndef NO_SLEEP_MODE
  if (!getSettingValue(SettingsOptions::Sensitivity) || !getSettingValue(SettingsOptions::SleepTime)) {
    return;
  }
  const TickType_t lastEventTime = lastButtonTime < lastMovementTime ? lastMovementTime : lastButtonTime;
  const TickType_t downCount     = getSleepTimeout() - xTaskGetTickCount() + lastEventTime;
  const bool       minutes       = downCount >= TICKS_MIN;
  const uint32_t   amount        = minutes ? (downCount + TICKS_MIN - 1) / TICKS_MIN : downCount / 1000 + 1;
  const char       unit[]        = {minutes ? 'm' : 's', '\0'};
  char             number[5]     = {};
  uint8_t          digits        = digitCount(amount);
  uint32_t         remaining     = amount;
  for (uint8_t i = digits; i > 0; --i) {
    number[i - 1] = '0' + remaining % 10;
    remaining /= 10;
  }
  const char   *label  = Hs02WashFont::kLabels.sleep;
  const uint8_t width  = measureWashTextEmphasis(label) + measureWashTextEmphasis(" ") + measureWashTextEmphasis(number) + measureWashTextEmphasis(unit);
  const uint8_t x      = kScreenWidth - 6 - width;
  uint8_t       cursor = drawWashTextEmphasis(" ", drawWashTextEmphasis(label, x, 64, kInk), 64, kInk);
  cursor               = drawWashTextEmphasis(number, cursor, 64, kInk);
  drawWashTextEmphasis(unit, cursor, 64, kInk);
#endif
}

void drawWashTopVoltage() {
  const uint32_t voltage = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  const uint8_t  cursor  = drawWashText("DC ", 6, 5, kMuted);
  drawWashTenths(voltage, cursor, 5, kMuted, 'V');
}

void drawWashTipMicrovolts() {
  const uint32_t tipMicrovolts = TipThermoModel::convertTipRawADCTouV(getTipRawTemp(0));
  const uint8_t  width         = measureWashText("T") + kWashCustomGlyphAdvance + measureWashText("P ") + measureWashUnsigned(tipMicrovolts) + kWashCustomGlyphAdvance + measureWashText("V");
  const uint8_t  x             = kScreenWidth - 6 - width;
  uint8_t        cursor        = drawWashText("T", x, 5, kMuted);
  Display::drawBitmapColor(kWashGlyphI, Hs02WashFont::kGlyphWidth, Hs02WashFont::kGlyphHeight, cursor, 5, kMuted);
  cursor += kWashCustomGlyphAdvance;
  cursor = drawWashText("P ", cursor, 5, kMuted);
  cursor = drawWashUnsigned(tipMicrovolts, cursor, 5, kMuted);
  Display::drawBitmapColor(kWashGlyphU, Hs02WashFont::kGlyphWidth, Hs02WashFont::kGlyphHeight, cursor, 5, kMuted);
  drawWashText("V", cursor + kWashCustomGlyphAdvance, 5, kMuted);
}

// "SET 320", centred.
void drawWashSetTemperature(TemperatureType_t target) {
  const char   *set   = Hs02WashFont::kLabels.set;
  const uint8_t width = measureWashText(set) + measureWashText(" ") + measureWashUnsigned(target);
  drawWashUnsigned(target, drawWashText(" ", drawWashText(set, (kScreenWidth - width) / 2, 47, kMuted), 47, kMuted), 47, kMuted);
}

void drawWashTopTelemetry() {
  drawWashTopVoltage();
  drawWashTipMicrovolts();
}

void drawWashHeating(TemperatureType_t current, TemperatureType_t target, uint32_t x10Watt, bool boostModeOn) {
  drawWashTopTelemetry();
  drawTemperatureNumber(current, 80 - (3 * Hs02InterSemiBoldFont::kDigitWidth) / 2, 18, kInk);

  if (boostModeOn) {
    const char *boost = Hs02WashFont::kLabels.boost;
    drawWashText(boost, (kScreenWidth - measureWashText(boost)) / 2, 47, kAccent);
  } else {
    drawWashSetTemperature(target);
  }

  drawWashTenthsEmphasis(x10Watt, 6, 64, kAccent, 'W');
  drawSleepCountdown();
}

#ifdef HS02_THEME_CARD
// ---- Card theme ---------------------------------------------------------------------------
// Each page still has only four colours: background, white, one accent, grey.
constexpr uint16_t kCardReadyPalette[4]  = {0x0042, 0xFFFF, 0x262B, 0x9D37}; // green
constexpr uint16_t kCardSolderPalette[4] = {0x0042, 0xFFFF, 0xEA28, 0x9D37}; // red
constexpr uint16_t kCardBoostPalette[4]  = {0x0042, 0xFFFF, 0xF4E1, 0x9D37}; // amber
constexpr uint16_t kCardSleepPalette[4]  = {0x0042, 0xFFFF, 0x3DFF, 0x9D37}; // blue
uint16_t cardPalette[4];

// A mode's palette with the accent entry replaced by the colour chosen for that mode.
const uint16_t *cardPaletteFor(const uint16_t *modePalette, SettingsOptions setting) {
  cardPalette[kBg]     = modePalette[kBg];
  cardPalette[kInk]    = modePalette[kInk];
  cardPalette[kAccent] = hs02CardAccent(setting, modePalette[kAccent]);
  cardPalette[kMuted]  = modePalette[kMuted];
  return cardPalette;
}

constexpr uint8_t  kCardColumn = 108, kCardNumberY = 25, kCardBarX = 13, kCardBarY = 67, kCardBarWidth = 100, kCardFooterX = 120, kCardFooterY = 63;

// The large reading, without leading zeros, followed by a degree ring.
void drawCardTemperature(TemperatureType_t temperature) {
  const uint8_t digits[3] = {(uint8_t)(temperature / 100 % 10), (uint8_t)(temperature / 10 % 10), (uint8_t)(temperature % 10)};
  uint8_t       x         = 13;
  bool          started   = false;
  for (uint8_t i = 0; i < 3; ++i) {
    if (i < 2 && !started && digits[i] == 0) {
      continue;
    }
    started = true;
    Display::drawBitmapColor(Hs02CardDigits::kDigits[digits[i]], Hs02CardDigits::kDigitWidth, Hs02CardDigits::kDigitHeight, x, kCardNumberY, kInk);
    x += Hs02CardDigits::kDigitWidth;
  }
  x += 2;
  Display::fillRectColor(x + 2, kCardNumberY, 3, 2, kInk);
  Display::fillRectColor(x + 2, kCardNumberY + 5, 3, 2, kInk);
  Display::fillRectColor(x, kCardNumberY + 2, 2, 3, kInk);
  Display::fillRectColor(x + 5, kCardNumberY + 2, 2, 3, kInk);
}

void drawCardFrame(const char *label) {
  Display::fillRectColor(4, 5, 3, 70, kAccent);
  drawWashTextEmphasis(label, 13, 9, kAccent);
}

// Right column: a grey caption over the target temperature, in medium digits in the accent
// colour, without leading zeros and followed by a small degree ring.
void drawCardTarget(const char *caption, TemperatureType_t target) {
  constexpr uint8_t kLeft = 104, kDigitsY = 23;
  drawWashTextEmphasis(caption, kLeft, 8, kMuted);
  const uint8_t digits[3] = {(uint8_t)(target / 100 % 10), (uint8_t)(target / 10 % 10), (uint8_t)(target % 10)};
  uint8_t       x         = kLeft;
  bool          started   = false;
  for (uint8_t i = 0; i < 3; ++i) {
    if (i < 2 && !started && digits[i] == 0) {
      continue;
    }
    started = true;
    Display::drawBitmapColor(Hs02CardDigits::kMediumDigits[digits[i]], Hs02CardDigits::kMediumWidth, Hs02CardDigits::kMediumHeight, x, kDigitsY, kAccent);
    x += Hs02CardDigits::kMediumWidth;
  }
  Display::fillRectColor(x + 1, kDigitsY, 2, 1, kAccent);
  Display::fillRectColor(x + 1, kDigitsY + 3, 2, 1, kAccent);
  Display::fillRectColor(x, kDigitsY + 1, 1, 2, kAccent);
  Display::fillRectColor(x + 3, kDigitsY + 1, 1, 2, kAccent);
}

// Along the bottom: heater power as a share of the power limit, with the reading beside it.
void drawCardPowerBar(uint32_t x10Watt) {
  uint32_t limit = (uint32_t)getSettingValue(SettingsOptions::PowerLimit) * 10;
  if (limit == 0) {
    limit = 1000;
  }
  if (x10Watt > limit) {
    x10Watt = limit;
  }
  Display::fillRectColor(kCardBarX, kCardBarY + 1, kCardBarWidth, 2, kMuted);
  const uint8_t filled = x10Watt * kCardBarWidth / limit;
  if (filled) {
    Display::fillRectColor(kCardBarX, kCardBarY, filled, 4, kAccent);
  }
}

uint32_t cardVolts() { return (getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0) + 5) / 10; }

// A whole number and its unit in the small grey text, e.g. "20V" or "65W".
void drawCardValue(uint32_t value, char unit, uint8_t x, uint8_t y) {
  const char text[] = {unit, '\0'};
  drawWashText(text, drawWashUnsigned(value, x, y, kMuted), y, kMuted);
}

// Bottom right, beside the bar.
void drawCardFooter(uint32_t value, char unit) { drawCardValue(value, unit, kCardFooterX, kCardFooterY); }

// The reading as shown: a change of one degree is only taken over once it has lasted half a
// second, so a reading on the edge between two values does not flicker. Two degrees or more
// are shown at once, so heating up and cooling down are not slowed.
TemperatureType_t cardSteadyTemperature(TemperatureType_t reading) {
  static TemperatureType_t shown        = 0;
  static TickType_t        differsSince = 0;
  const TickType_t         now          = xTaskGetTickCount();
  const int32_t            delta        = (int32_t)reading - (int32_t)shown;
  if (delta >= 2 || delta <= -2 || (delta != 0 && (now - differsSince) >= TICKS_SECOND / 2)) {
    shown        = reading;
    differsSince = now;
  } else if (delta == 0) {
    differsSince = now;
  }
  return shown;
}

// Heater power for the number beside the bar, in tenths of a watt: a second, slower average
// (about half a second) on top of x10WattHistory, kept with 8 extra bits so it does not stop
// short of a steady value. The bar itself stays on the fast value.
uint32_t cardSteadyWatts(uint32_t x10Watt) {
  static int32_t    shownX256 = 0;
  static TickType_t last      = 0;
  const TickType_t  now       = xTaskGetTickCount();
  if ((now - last) > TICKS_SECOND / 2) {
    shownX256 = (int32_t)x10Watt * 256; // first frame of a heating session: no history to average
  } else {
    shownX256 += ((int32_t)x10Watt * 256 - shownX256) / 12;
  }
  last = now;
  return (uint32_t)(shownX256 + 128) / 256;
}


// Home screen with no tip fitted: a tip sliding into the handle from the left, over and over.
// Buttons work as on the normal home screen, so the settings menu stays reachable.
void drawCardNoTip() {
  drawCardFrame("NO TIP");
  // 1.6 s loop: the tip slides in for 1 s, then sits in the handle for 0.6 s
  const uint32_t t      = xTaskGetTickCount() % (TICKS_SECOND * 8 / 5);
  const uint8_t  travel = 24;
  const uint8_t  gap    = t < TICKS_SECOND ? (uint8_t)(travel - travel * t / TICKS_SECOND) : 0;
  // handle on the right: collar, then grip
  Display::fillRectColor(88, 35, 8, 10, kMuted);
  Display::fillRectColor(96, 32, 50, 16, kMuted);
  // tip on the left, point outwards: stepped point, then shaft up to the collar
  const uint8_t x = 38 - gap;
  Display::fillRectColor(x, 39, 3, 1, kInk);
  Display::fillRectColor(x + 3, 39, 4, 2, kInk);
  Display::fillRectColor(x + 7, 38, 5, 4, kInk);
  Display::fillRectColor(x + 12, 38, 38, 4, kInk);
  if (gap == 0) {
    Display::fillRectColor(88, 33, 2, 14, kAccent); // seated
  }
  drawWashText("INSERT TIP", 13, 58, kMuted);
  drawCardFooter(cardVolts(), 'V');
}

void drawCardIdle(TemperatureType_t tipTemp) {
  Display::setColorPalette(cardPaletteFor(kCardReadyPalette, SettingsOptions::ColourReady));
  if (isTipDisconnected()) {
    drawCardNoTip();
    return;
  }
  drawCardFrame(Hs02WashFont::kLabels.ready);
  // the caller passes degrees C; the target beside it is in the unit the user chose
  drawCardTemperature(cardSteadyTemperature(getSettingValue(SettingsOptions::TemperatureInF) ? TipThermoModel::getTipInF() : tipTemp));
  drawCardTarget(Hs02WashFont::kLabels.set, getSettingValue(SettingsOptions::SolderingTemp));
  drawCardPowerBar(0);
  drawCardFooter(cardVolts(), 'V');
}

void drawCardSoldering(bool boostModeOn) {
  Display::setColorPalette(boostModeOn ? cardPaletteFor(kCardBoostPalette, SettingsOptions::ColourBoost) : cardPaletteFor(kCardSolderPalette, SettingsOptions::ColourHeating));
  const uint32_t x10Watt = x10WattHistory.average();
  drawCardFrame(boostModeOn ? Hs02WashFont::kLabels.boost : "HEATING");
  drawCardTemperature(cardSteadyTemperature(getSettingValue(SettingsOptions::TemperatureInF) ? TipThermoModel::getTipInF() : TipThermoModel::getTipInC()));
  drawCardTarget("TARGET", getSettingValue(boostModeOn ? SettingsOptions::BoostTemp : SettingsOptions::SolderingTemp));
  drawCardPowerBar(x10Watt);
  drawCardFooter((cardSteadyWatts(x10Watt) + 5) / 10, 'W');
  drawCardValue(cardVolts(), 'V', 104, 46); // supply voltage, under the target
}

void drawCardSleep(TemperatureType_t tipTemp) {
  Display::setColorPalette(cardPaletteFor(kCardSleepPalette, SettingsOptions::ColourSleep));
  drawCardFrame(Hs02WashFont::kLabels.sleep);
  drawCardTemperature(cardSteadyTemperature(tipTemp));
  drawWashTextEmphasis("Zzz", kCardColumn, 9, kAccent);
  drawWashText("IDLE", kCardColumn, 32, kMuted);
  drawCardFooter(cardVolts(), 'V');
}
#endif // HS02_THEME_CARD
} // namespace

#ifdef HS02_THEME_CARD
void ui_draw_home_gauge_idle(TemperatureType_t tipTemp) { drawCardIdle(tipTemp); }
void ui_draw_home_gauge_soldering(bool boostModeOn) { drawCardSoldering(boostModeOn); }
void ui_draw_home_gauge_sleep(TemperatureType_t tipTemp) { drawCardSleep(tipTemp); }
#else
void ui_draw_home_gauge_idle(TemperatureType_t tipTemp) {
  Display::setColorPalette(kIdlePalette);
  if (isTipDisconnected()) {
    return;
  }
  drawWashTopTelemetry();
  drawTemperatureNumber(tipTemp, 80 - (3 * Hs02InterSemiBoldFont::kDigitWidth) / 2, 18, kInk);

  drawWashSetTemperature(getSettingValue(SettingsOptions::SolderingTemp));
  // Right-aligned to where the English "READY" has always ended.
  const char *ready = Hs02WashFont::kLabels.ready;
  drawWashTextEmphasis(ready, kReadyRightEdge - measureWashTextEmphasis(ready), 64, kAccent);
}

void ui_draw_home_gauge_soldering(bool boostModeOn) {
  Display::setColorPalette(boostModeOn ? kBoostPalette : kSolderPalette);
  const TemperatureType_t current = TipThermoModel::getTipInC();
  const TemperatureType_t target  = getSettingValue(boostModeOn ? SettingsOptions::BoostTemp : SettingsOptions::SolderingTemp);
  drawWashHeating(current, target, x10WattHistory.average(), boostModeOn);
}

void ui_draw_home_gauge_sleep(TemperatureType_t tipTemp) {
  Display::setColorPalette(kSleepPalette);
  drawTemperatureNumber(tipTemp, 80 - (3 * Hs02InterSemiBoldFont::kDigitWidth) / 2, 18, kInk);
  const uint32_t voltage    = getInputVoltageX10(getSettingValue(SettingsOptions::VoltageDiv), 0);
  const uint8_t  valueWidth = measureWashTenthsEmphasis(voltage, 'V');
  const char    *label      = Hs02WashFont::kLabels.sleep;
  const uint8_t  labelWidth = measureWashTextEmphasis(label) + measureWashTextEmphasis(" ");
  const uint8_t  x          = (kScreenWidth - labelWidth - valueWidth) / 2;
  drawWashTenthsEmphasis(voltage, drawWashTextEmphasis(" ", drawWashTextEmphasis(label, x, 64, kMuted), 64, kMuted), 64, kMuted, 'V');
}
#endif // HS02_THEME_CARD

#endif
