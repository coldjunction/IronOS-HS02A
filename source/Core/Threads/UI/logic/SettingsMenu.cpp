#include "Display.hpp"
#include "OperatingModes.h"
#include "ScrollMessage.hpp"

#define HELP_TEXT_TIMEOUT_TICKS (TICKS_SECOND * 3)
/*
 * The settings menu is the most complex bit of GUI code we have
 * The menu consists of a two tier menu
 * Main menu -> Categories
 * Secondary menu -> Settings
 *
 * For each entry in the menu
 */

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
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  if (settingsItemIndex == SettingsItemIndex::BlockGame) {
    // "Super High Intensity Block Simulator" does not fit the usual two rows; the entry has no
    // value to show, so its name takes the value's rows as well: four rows of the small font.
    const char *const rows[] = {SmallBlocksName1, SmallBlocksName2, SmallBlocksName3, SmallBlocksName4};
    for (uint8_t row = 0; row < 4; row++) {
      Display::setCursor(0, row * 16);
      Display::print(rows[row], FontStyle::SMALL);
    }
  } else
#endif
    Display::printWholeScreen(translatedString(Tr->SettingsShortNames[shortDescIndex]));

// prepare cursor for value
// make room for scroll indicator
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  Display::setCursor(4, 32); // card theme: the bottom 16 rows are the button hint row
#elif defined(LCD_160x80)
  Display::setCursor(4, 40);
#else
  Display::setCursor(cursorCharPosition * FONT_LARGE_WIDTH - 2, 0);
#endif
}

// Render a menu, based on the position given
// This will either draw the menu item, or the help text depending on how long its been since button press
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
// ---- Card theme menu ------------------------------------------------------------------------
// Three rows: the setting's name (accent), its value, and a grey hint row naming what the
// three buttons do, left to right in the order they sit on the iron: DOWN, UP, OK.
//   browsing : OK opens a group / starts editing / runs an action, UP and DOWN move
//   editing  : DOWN is -, UP is + (hold to repeat), OK finishes
//   hold OK  : back one level
// The value turns to the accent colour while it is being edited. The scrolling help text of
// the stock layout is not shown.
extern bool       hs02DebugPagesRequested; // settingsGUI.cpp
extern bool       hs02BlocksRequested;     // settingsGUI.cpp
// Set by the home screen when the menu is opened by HOLDING a button: the menu then ignores the
// buttons until they have been let go, or the held button would start stepping through entries.
bool              hs02MenuWaitForRelease = false;
constexpr uint8_t kCardValueRow = 32, kCardHintRow = 64;
uint16_t          getMenuLength(const menuitem *menu, const uint16_t stop);

static const menuitem *cardCurrentMenu(guiContext *cxt) { return cxt->scratch_state.state2 == 0 ? rootSettingsMenu : subSettingsMenus[cxt->scratch_state.state1]; }

// Moves to the next (+1) or previous (-1) visible entry of the current menu, wrapping round.
static void cardStepEntry(guiContext *cxt, int8_t direction) {
  uint16_t       *mainEntry = &(cxt->scratch_state.state1);
  uint16_t       *subEntry  = &(cxt->scratch_state.state2);
  const menuitem *menu      = cardCurrentMenu(cxt);
  uint16_t        count     = 0;
  while (menu[count].draw != nullptr) {
    count++;
  }
  if (count == 0) {
    return;
  }
  uint16_t       index = (*subEntry == 0) ? *mainEntry : (*subEntry) - 1;
  const uint16_t start = index;
  for (uint16_t tries = 0; tries < count; tries++) {
    index = (index + count + direction) % count;
    if (menu[index].isVisible == nullptr || menu[index].isVisible()) {
      break;
    }
  }
  if (index == start) {
    return; // the only visible entry: nothing to move to, so no slide animation either
  }
  if (*subEntry == 0) {
    *mainEntry = index;
  } else {
    *subEntry = index + 1;
  }
  cxt->scratch_state.state5 = 0; // menu length is recounted
  cxt->transitionMode       = direction > 0 ? TransitionAnimation::Down : TransitionAnimation::Up;
}

// Sleep and boost temperature are edited to the degree in the card menu: a press is one degree,
// a held button goes to the next multiple of ten. (Their own step functions only count upwards,
// in steps of 10 C / 20 F.) Boost has an "off" position, 0, below its lowest and above its
// highest temperature; the sleep temperature wraps round.
static bool cardItemCycles(const menuitem *item) {
  return item->incrementHandler != nullptr && (item->autoSettingOption == SettingsOptions::SleepTemp || item->autoSettingOption == SettingsOptions::BoostTemp);
}

static void cardStepTemperature(const menuitem *item, bool up, bool held) {
  const SettingsOptions option     = item->autoSettingOption;
  const bool            fahrenheit = getSettingValue(SettingsOptions::TemperatureInF);
  const bool            boost      = option == SettingsOptions::BoostTemp;
  const int16_t         lowest     = boost ? (fahrenheit ? MIN_BOOST_TEMP_F : MIN_BOOST_TEMP_C) : (fahrenheit ? 50 : 10);
  const int16_t         highest    = boost ? (fahrenheit ? MAX_TEMP_F : MAX_TEMP_C) : (fahrenheit ? 580 : 300);
  int16_t               value      = getSettingValue(option);
  if (boost && value == 0) { // from "off"
    value = up ? lowest : highest;
  } else {
    if (!held) {
      value += up ? 1 : -1;
    } else if (up) {
      value = (value / 10 + 1) * 10;
    } else {
      value = ((value + 9) / 10 - 1) * 10;
    }
    if (value > highest) {
      value = boost ? 0 : lowest;
    } else if (value < lowest) {
      value = boost ? 0 : highest;
    }
  }
  setSettingValue(option, (uint16_t)value);
}

static bool cardItemHasValue(guiContext *cxt, const menuitem *item) {
  return cxt->scratch_state.state2 != 0 && (item->incrementHandler == nullptr || cardItemCycles(item)) && (int)item->autoSettingOption < (int)SettingsOptions::SettingsOptionsLength;
}

static void cardDrawHints(guiContext *cxt, const menuitem *item) {
  const bool  editing = cxt->scratch_state.state7 != 0;
  const char *ok      = SmallSymbolHintRun;
  if (editing) {
    ok = SmallSymbolHintDone;
  } else if (cxt->scratch_state.state2 == 0 && item->incrementHandler == nullptr) {
    ok = SmallSymbolHintOpen;
  } else if (cardItemHasValue(cxt, item)) {
    ok = SmallSymbolHintEdit;
  }
  // Slots left to right, as the buttons sit on the iron: DOWN, UP, OK.
  LCD::drawFilledRect(0, kCardHintRow, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, true);
  Display::setCursor(108, kCardHintRow);
  Display::print(ok, FontStyle::SMALL);
  if (!editing && getMenuLength(cardCurrentMenu(cxt), 128) < 2) {
    return; // a single entry: there is no previous or next
  }
  // While editing the two symbols sit in the middle of their slots.
  Display::setCursor(editing ? 18 : 0, kCardHintRow);
  Display::print(editing ? SmallSymbolHintLess : SmallSymbolHintNext, FontStyle::SMALL);
  Display::setCursor(editing ? 54 + 18 : 54, kCardHintRow);
  Display::print(editing ? SmallSymbolHintMore : SmallSymbolHintPrev, FontStyle::SMALL);
}

static OperatingMode cardHandleButtons(const ButtonState buttons, guiContext *cxt, const menuitem *item) {
  uint16_t *subEntry               = &(cxt->scratch_state.state2);
  uint32_t *autoRepeatAcceleration = &(cxt->scratch_state.state3);
  uint32_t *autoRepeatTimer        = &(cxt->scratch_state.state4);
  uint32_t *editing                = &(cxt->scratch_state.state7);
  // A held button repeats, faster the longer it is held (same pacing as the stock menu).
  auto repeatDue = [&]() {
    if (xTaskGetTickCount() + (*autoRepeatAcceleration) > (*autoRepeatTimer) + PRESS_ACCEL_INTERVAL_MAX) {
      (*autoRepeatTimer) = xTaskGetTickCount();
      (*autoRepeatAcceleration) += PRESS_ACCEL_STEP;
      return true;
    }
    return false;
  };
  if (hs02MenuWaitForRelease) {
    if (buttons == BUTTON_NONE) {
      hs02MenuWaitForRelease = false;
    }
    return OperatingMode::SettingsMenu;
  }
  bool up = false, down = false, held = false;
  switch (buttons) {
  case BUTTON_NONE:
    (*autoRepeatAcceleration) = 0;
    (*autoRepeatTimer)        = 0;
    break;
  case BUTTON_OK_LONG: // back one level
    *editing = 0;
    if (*subEntry == 0) {
      saveSettings();
      cxt->transitionMode = TransitionAnimation::Left;
      return OperatingMode::HomeScreen;
    }
    cxt->transitionMode       = TransitionAnimation::Left;
    *subEntry                 = 0;
    cxt->scratch_state.state5 = 0;
    break;
  case BUTTON_OK_SHORT:
    cxt->scratch_state.state5 = 0;
    if (*editing) {
      *editing = 0;
    } else if (cardItemHasValue(cxt, item)) {
      *editing = 1;
    } else if (item->incrementHandler != nullptr) {
      item->incrementHandler();
      if (hs02DebugPagesRequested) { // the "Debug pages" entry
        hs02DebugPagesRequested = false;
        saveSettings();
        cxt->transitionMode = TransitionAnimation::None;
        return OperatingMode::DebugMenuReadout;
      }
      if (hs02BlocksRequested) { // the "Block game" entry
        hs02BlocksRequested = false;
        cxt->transitionMode = TransitionAnimation::None;
        return OperatingMode::Hs02Blocks;
      }
    } else if (*subEntry == 0) {
      *subEntry           = 1;
      cxt->transitionMode = TransitionAnimation::Right;
    }
    break;
  case BUTTON_B_LONG:
    up   = repeatDue();
    held = true;
    break;
  case BUTTON_B_SHORT:
    up = true;
    break;
  case BUTTON_F_LONG:
    down = repeatDue();
    held = true;
    break;
  case BUTTON_F_SHORT:
    down = true;
    break;
  default:
    break;
  }
  if (up || down) {
    if (*editing) {
      // DOWN (the left-hand button) is -, UP (the middle one) is +, as on the temperature screen.
      if (cardItemCycles(item)) {
        cardStepTemperature(item, up, held);
      } else if (up) {
        nextSettingValue(item->autoSettingOption);
      } else {
        prevSettingValue(item->autoSettingOption);
      }
      cxt->scratch_state.state5 = 0; // a changed value can show or hide other entries
    } else {
      cardStepEntry(cxt, up ? -1 : 1);
    }
  }
  if ((PRESS_ACCEL_INTERVAL_MAX - (*autoRepeatAcceleration)) < PRESS_ACCEL_INTERVAL_MIN) {
    (*autoRepeatAcceleration) = PRESS_ACCEL_INTERVAL_MAX - PRESS_ACCEL_INTERVAL_MIN;
  }
  return OperatingMode::SettingsMenu;
}
#endif

void render_menu(const menuitem *item, guiContext *cxt) {
#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  if (item->shortDescriptionSize > 0) {
    printShortDescription(item->shortDescriptionIndex, item->shortDescriptionSize);
  }
  item->draw();
  cardDrawHints(cxt, item);
  if (cxt->scratch_state.state7) {
    LCD::setMonoRegions(kCardValueRow, kCardHintRow, DISPLAY_WIDTH - 4, kCardHintRow); // editing: value in accent
  } else {
    // browsing: name in accent. The block game's name is four rows long (see
    // printShortDescription), down to the hint row.
    LCD::setMonoRegions(0, item->shortDescriptionIndex == SettingsItemIndex::BlockGame ? kCardHintRow : kCardValueRow, DISPLAY_WIDTH - 4, kCardHintRow);
  }
  return;
#endif
  // If recent interaction or not help text draw the entry
  if ((xTaskGetTickCount() - lastButtonTime < HELP_TEXT_TIMEOUT_TICKS) || item->description == 0) {

    if (item->shortDescriptionSize > 0) {
      printShortDescription(item->shortDescriptionIndex, item->shortDescriptionSize);
    }
    item->draw();
  } else {

    uint16_t *isRenderingHelp = &(cxt->scratch_state.state6);
    *isRenderingHelp          = 1;
    // Draw description
    const char *description = translatedString(Tr->SettingsDescriptions[item->description - 1]);
    drawScrollingText(description, (xTaskGetTickCount() - lastButtonTime) - HELP_TEXT_TIMEOUT_TICKS);
  }
}

uint16_t getMenuLength(const menuitem *menu, const uint16_t stop) {
  // walk this menu to find the length
  uint16_t counter = 0;
  for (uint16_t pos = 0; pos < stop; pos++) {
    // End of list
    if (menu[pos].draw == nullptr) {
      return counter;
    }
    // Otherwise increment for each visible item (null == always, or if not check function)
    if (menu[pos].isVisible == nullptr || menu[pos].isVisible()) {
      counter++;
    }
  }
  return counter;
}

OperatingMode moveToNextEntry(guiContext *cxt) {
  uint16_t *mainEntry         = &(cxt->scratch_state.state1);
  uint16_t *subEntry          = &(cxt->scratch_state.state2);
  uint16_t *currentMenuLength = &(cxt->scratch_state.state5);
  uint16_t *isRenderingHelp   = &(cxt->scratch_state.state6);

  if (*isRenderingHelp) {
    *isRenderingHelp = 0;
  } else {
    *currentMenuLength = 0; // Reset menu length
    // Scroll down
    // We can increment freely _once_
    cxt->transitionMode = TransitionAnimation::Down;
    if (*subEntry == 0) {
      (*mainEntry) += 1;

      if (rootSettingsMenu[*mainEntry].draw == nullptr) {
        // We are off the end of the menu now
        saveSettings();
        cxt->transitionMode = TransitionAnimation::Left;
        return OperatingMode::HomeScreen;
      }
      // Check if visible
      if (rootSettingsMenu[*mainEntry].isVisible != nullptr && !rootSettingsMenu[*mainEntry].isVisible()) {
        // We need to move on as this one isn't visible
        return moveToNextEntry(cxt);
      }
    } else {
      (*subEntry) += 1;

      // If the new entry is null, we need to exit
      if (subSettingsMenus[*mainEntry][(*subEntry) - 1].draw == nullptr) {
        (*subEntry)         = 0; // Reset back to the main menu
        cxt->transitionMode = TransitionAnimation::Left;
        // Have to break early to avoid the below check underflowing
        return OperatingMode::SettingsMenu;
      }
      // Check if visible
      if (subSettingsMenus[*mainEntry][(*subEntry) - 1].isVisible != nullptr && !subSettingsMenus[*mainEntry][(*subEntry) - 1].isVisible()) {
        // We need to move on as this one isn't visible
        return moveToNextEntry(cxt);
      }
    }
  }
  return OperatingMode::SettingsMenu;
}

OperatingMode gui_SettingsMenu(const ButtonState buttons, guiContext *cxt) {
  // Render out the current settings menu
  // State 1 -> Root menu
  // State 2 -> Sub entry
  // Draw main entry if sub-entry is 0, otherwise draw sub-entry

  uint16_t *mainEntry              = &(cxt->scratch_state.state1);
  uint16_t *subEntry               = &(cxt->scratch_state.state2);
  uint32_t *autoRepeatAcceleration = &(cxt->scratch_state.state3);
  uint32_t *autoRepeatTimer        = &(cxt->scratch_state.state4);
  uint16_t *currentMenuLength      = &(cxt->scratch_state.state5);
  uint16_t *isRenderingHelp        = &(cxt->scratch_state.state6);

  const menuitem *currentMenu;
  // Draw the currently on screen item
  uint16_t currentScreen;
  if (*subEntry == 0) {
    // Drawing main menu
    currentMenu   = rootSettingsMenu;
    currentScreen = *mainEntry;
  } else {
    // Drawing sub menu
    currentMenu   = subSettingsMenus[*mainEntry];
    currentScreen = (*subEntry) - 1;
  }
  render_menu(&(currentMenu[currentScreen]), cxt);

  // Update the cached menu length if unknown
  if (*currentMenuLength == 0) {
    // We walk the current menu to find the length
    *currentMenuLength = getMenuLength(currentMenu, 128 /* Max length of any menu*/);
  }

  if (*isRenderingHelp == 0) {
    //  Draw scroll

    // Get virtual pos by counting entries from start to _here_
    uint16_t currentVirtualPosition = getMenuLength(currentMenu, currentScreen + 1);
    if (currentVirtualPosition > 0) {
      currentVirtualPosition--;
    }

    // The height of the indicator is screen res height / total menu entries
    uint8_t indicatorHeight = DISPLAY_HEIGHT / *currentMenuLength;
    if (indicatorHeight == 0) {
      indicatorHeight = 1; // always at least 1 pixel
    }

    uint16_t position = (DISPLAY_HEIGHT * (uint16_t)currentVirtualPosition) / *currentMenuLength;

    bool showScrollbar = true;

    // Store if its the last option for this setting
    bool isLastOptionForSetting = false;
    if ((int)currentMenu[currentScreen].autoSettingOption < (int)SettingsOptions::SettingsOptionsLength) {
      isLastOptionForSetting = isLastSettingValue(currentMenu[currentScreen].autoSettingOption);
    }

    // Last settings menu entry, reset scroll show back so it flashes
    if (isLastOptionForSetting) {
      showScrollbar = false;
    }

    // Or Flash it
    showScrollbar |= (xTaskGetTickCount() % (TICKS_SECOND / 4) < (TICKS_SECOND / 8));

#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
    // Card theme: a track on the right edge, above the hint row, with a thumb showing where
    // this entry sits in its menu.
    const uint8_t trackHeight = kCardHintRow - 2;
    const uint8_t thumb       = (trackHeight / *currentMenuLength) < 6 ? 6 : (trackHeight / *currentMenuLength);
    uint8_t       top         = (trackHeight * (uint16_t)currentVirtualPosition) / *currentMenuLength;
    if (top + thumb > trackHeight) {
      top = trackHeight - thumb;
    }
    LCD::drawFilledRect(DISPLAY_WIDTH - 4, 0, DISPLAY_WIDTH - 1, trackHeight - 1, true);
    LCD::drawFilledRect(DISPLAY_WIDTH - 2, 0, DISPLAY_WIDTH - 2, trackHeight - 1, false);
    LCD::drawFilledRect(DISPLAY_WIDTH - 3, top, DISPLAY_WIDTH - 1, top + thumb - 1, false);
    (void)showScrollbar;
    (void)position;
    (void)indicatorHeight;
#else
    if (showScrollbar) {
      Display::drawScrollIndicator((uint8_t)position, indicatorHeight);
    }
#endif
  }

#if defined(LCD_160x80) && defined(HS02_THEME_CARD)
  return cardHandleButtons(buttons, cxt, &(currentMenu[currentScreen]));
#endif

  // Now handle user button input
  auto callIncrementHandler = [&]() {
    if (currentMenu[currentScreen].incrementHandler != nullptr) {
      currentMenu[currentScreen].incrementHandler();
    } else if ((int)currentMenu[currentScreen].autoSettingOption < (int)SettingsOptions::SettingsOptionsLength) {
      nextSettingValue(currentMenu[currentScreen].autoSettingOption);
    }
    return false;
  };

  // Modify a button value before processing a key press if setting to swap buttons is enabled
  bool    swapButtonSettings = getSettingValue(SettingsOptions::ReverseButtonSettings);
  uint8_t buttonPress;
  switch (buttons) {
  case BUTTON_F_LONG:
    buttonPress = swapButtonSettings ? BUTTON_B_LONG : BUTTON_F_LONG;
    break;
  case BUTTON_F_SHORT:
    buttonPress = swapButtonSettings ? BUTTON_B_SHORT : BUTTON_F_SHORT;
    break;
  case BUTTON_B_LONG:
    buttonPress = swapButtonSettings ? BUTTON_F_LONG : BUTTON_B_LONG;
    break;
  case BUTTON_B_SHORT:
    buttonPress = swapButtonSettings ? BUTTON_F_SHORT : BUTTON_B_SHORT;
    break;
  case BUTTON_OK_SHORT:
    buttonPress = BUTTON_F_SHORT; // Dedicated OK button selects/enters, ignoring button swap
    break;
  case BUTTON_OK_LONG:
    buttonPress = BUTTON_BOTH; // OK long-press goes back one level (submenu -> root -> home)
    break;
  default:
    buttonPress = buttons;
    break;
  }

  OperatingMode newMode = OperatingMode::SettingsMenu;
  switch (buttonPress) {
  case BUTTON_NONE:
    (*autoRepeatAcceleration) = 0; // reset acceleration
    (*autoRepeatTimer)        = 0; // reset acceleration
    break;
  case BUTTON_BOTH:
    if (*subEntry == 0) {
      saveSettings();
      cxt->transitionMode = TransitionAnimation::Left;
      return OperatingMode::HomeScreen;
    } else {
      cxt->transitionMode = TransitionAnimation::Left;
      *subEntry           = 0;
      return OperatingMode::SettingsMenu;
    }
    break;
  case BUTTON_F_LONG:
    if (xTaskGetTickCount() + (*autoRepeatAcceleration) > (*autoRepeatTimer) + PRESS_ACCEL_INTERVAL_MAX) {
      callIncrementHandler();
      // Update the check for if its the last version
      bool isLastOptionForSetting = false;
      if ((int)currentMenu[currentScreen].autoSettingOption < (int)SettingsOptions::SettingsOptionsLength) {
        isLastOptionForSetting = isLastSettingValue(currentMenu[currentScreen].autoSettingOption);
      }

      if (isLastOptionForSetting) {
        (*autoRepeatTimer) = TICKS_SECOND * 2;
      } else {
        (*autoRepeatTimer) = 0;
      }
      (*autoRepeatTimer) += xTaskGetTickCount();
      (*autoRepeatAcceleration) += PRESS_ACCEL_STEP;
      *currentMenuLength = 0; // Reset incase menu visible changes
    }
    break;
  case BUTTON_F_SHORT:
    // Increment setting
    if (*isRenderingHelp) {
      *isRenderingHelp = 0;
    } else {
      *currentMenuLength = 0; // Reset incase menu visible changes
      if (*subEntry == 0) {
        // In a root menu, if its null handler we enter the menu
        if (currentMenu[currentScreen].incrementHandler != nullptr) {
          currentMenu[currentScreen].incrementHandler();
        } else {
          (*subEntry) += 1;
          cxt->transitionMode = TransitionAnimation::Right;
        }
      } else {
        callIncrementHandler();
      }
    }
    break;
  case BUTTON_B_LONG:
    if (xTaskGetTickCount() + (*autoRepeatAcceleration) > (*autoRepeatTimer) + PRESS_ACCEL_INTERVAL_MAX) {
      (*autoRepeatTimer) = xTaskGetTickCount();
      (*autoRepeatAcceleration) += PRESS_ACCEL_STEP;
    } else {
      break;
    }
    /* Fall through*/
  case BUTTON_B_SHORT:
    // Increment menu item
    newMode = moveToNextEntry(cxt);
    break;
  default:
    break;
  }

  if ((PRESS_ACCEL_INTERVAL_MAX - (*autoRepeatAcceleration)) < PRESS_ACCEL_INTERVAL_MIN) {
    (*autoRepeatAcceleration) = PRESS_ACCEL_INTERVAL_MAX - PRESS_ACCEL_INTERVAL_MIN;
  }

  // Otherwise we stay put for next render iteration
  return newMode;
}
