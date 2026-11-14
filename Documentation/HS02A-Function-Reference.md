# HS-02A fork: function reference

This document lists every function, method, compile-time switch, tuning constant and setting that this fork adds to or changes in the bugparty/IronOS port for the FNIRSI HS-02A (upstream commit `93da587`). It describes only what differs from upstream; untouched upstream functions appear only where a changed function cannot be understood without naming its caller. Each description follows the current source and its comments.

Terms used below:

- **Card theme**: this fork's screens and menus, the code guarded by `HS02_THEME_CARD`. The Makefile defines `HS02_THEME_CARD` for every HS02 build (see "Build switches"), so this code is always compiled.
- **Buttons**: the iron has three, left to right DOWN, UP, OK. In `ButtonState` values DOWN is `BUTTON_F_*`, UP is `BUTTON_B_*`, OK is `BUTTON_OK_*`, and DOWN and UP held together is `BUTTON_BOTH_LONG`.
- **GUI frame**: one pass of the loop in `startGUITask()`, which runs every 40 ms (`TICKS_100MS * 4 / 10`).
- **Mono screen**: a screen drawn into the 1 bit per pixel buffer (menus, warnings, debug pages). **Colour screen**: home, soldering and sleep, drawn into the 2 bit per pixel buffer with a four-entry palette.
- Colours are RGB565 unless stated otherwise.

## Build switches

### Theme define

One firmware is built. `source/Makefile` adds `-DHS02_THEME_CARD` to `GLOBAL_DEFINES` whenever `model` is `HS02`. The comment in `source/Core/BSP/Fnirsi/configuration.h` says the define has to come from the compiler command line and not from that header, because some files test it before they include the header.

| Define | Set by | What it selects |
| --- | --- | --- |
| `HS02_THEME_CARD` | `source/Makefile`, for every HS02 build | The card theme: card-style home, soldering and sleep screens (`draw_home_gauge.cpp`), a three-row settings menu with a button hint row (`SettingsMenu.cpp`, `settingsGUI.cpp`), coloured output of mono screens and a partial menu slide (`LCD.hpp`, `LCD.cpp`, `Display.cpp`), no boot logo (`BSP.cpp`, `GUIThread.cpp`), the home screen and soldering screen button layouts and the buzzer tunes (`HomeScreen.cpp`, `Soldering.cpp`, `Hs02Tune.cpp`), the HS-02 debug pages shown first (`DebugMenu.cpp`), a larger GUI task stack (`main.cpp`), `BUZZER_VOLUME` 6 and `TEMP_CHANGE_SWAP_DEFAULT` 1. Most uses are combined with `LCD_160x80` or `MODEL_HS02`. |

The port's own layout (the "wash" screens) is still in the source, in the branches that are compiled when `HS02_THEME_CARD` is not defined. It is not built, and the comment in `configuration.h` calls it untested in this fork. This document describes the code that is built; where it says "in the card theme" or "with `HS02_THEME_CARD`", it names the guard around that code.

### PID constants (`configuration.h`)

There is one set of PID constants. The PID output is in tenths of a watt. `TIP_PID_KP` is in tenths of a watt per °C and `TIP_PID_KD` in tenths of a watt times milliseconds per °C. The integral accumulates error times `TIP_PID_KI` times seconds, and the sum is divided by 100 before it is added to the output (see `PID::update`).

| Constant | Value | Upstream | What it controls |
| --- | --- | --- | --- |
| `TIP_PID_KP` | 20 | 40 | Proportional gain. |
| `TIP_PID_KI` | 700 | 700 | Integral gain. |
| `TIP_PID_KD` | 12000 | 8000 | Derivative gain. |
| `TIP_PID_INTEGRAL_LIMIT_SCALE` | 60 | 30 | Clamp on the integral sum, as a multiple of the current maximum output. |
| `TIP_PID_INTEGRAL_ZONE` | 30 | not present | The integral only accumulates while the tip is within this many °C of the set point. 0 means always. |
| `TIP_PID_INTEGRAL_MAX_RATE` | 8 | not present | The integral only accumulates while the reading approaches the set point more slowly than this many °C per second. 0 means always. |
| `TIP_PID_INPUT_SCALE` | 10 | not present | Steps per °C of the temperatures handed to the PID: 1 is whole degrees, 10 is the 0.1 °C reading. |

`TIP_PID_INTEGRAL_ZONE`, `TIP_PID_INTEGRAL_MAX_RATE` and `TIP_PID_INPUT_SCALE` are new in this fork. `PIDThread.cpp` gives them the defaults 0, 0 and 1 when a board does not define them, so other boards behave as before. The comment in `configuration.h` states that the values were chosen on a tip model (`tools/hs02-pid-sim`), that without `TIP_PID_INTEGRAL_MAX_RATE` the iron overshot by 4 °C at 320 °C and by 6 °C on a boost to 450 °C, and that the effect of that constant has not yet been measured on the iron.

### Other constants in `configuration.h`

| Constant | Value | Upstream | What it controls |
| --- | --- | --- | --- |
| `BUZZER_VOLUME` | 6 | 0 | Compare value of the buzzer timer (TIM2) while the buzzer sounds. Must be below 20 (`static_assert` in `setBuzzer()`). 0 is silent. |
| `TEMP_CHANGE_SWAP_DEFAULT` | 1 | not present | Turns + and − round on the temperature adjust screen by default. The setting `ReverseButtonTempChangeEnabled` swaps them back. `TemperatureAdjust.cpp` and `draw_temperature_change.cpp` default it to 0 when a board does not define it. |
| `CALIBRATION_OFFSET` | 0 | 900 | Default of the `CalibrationOffset` setting: the tip amplifier's reading at zero temperature difference, in microvolts. The comment says that on the HS-02A it measured as 0 (a cold tip reads 0 ADC counts), that the stock firmware's curve starts there too, and that with 900 everything from room temperature to about 55 °C read as 25 °C. It adds that a unit whose amplifier does have a positive offset reads a cold tip too warm until the CJC calibration has been run. |
| `MIN_CALIBRATION_OFFSET` | 0 | 100 | Lower limit of the `CalibrationOffset` setting, in microvolts. |
| `DC_MINIMUM_VOLTAGE_X10` | 40 | not present | Undervoltage floor in tenths of a volt (4.0 V) for a DC supply when the setting `LowVoltageAllowed` is on. Read by `lookupVoltageLevel()`. |
| `ANIMATION_SPEED` | `settingOffSpeed_t::FAST` | `settingOffSpeed_t::MEDIUM` | Default of the `AnimationSpeed` setting. |
| `POWER_LIMIT` | 100 | 60 | Default of the `PowerLimit` setting, in watts. |
| `WATT_HISTORY_WEIGHT` | 96 | not present (24 in `power.hpp`) | Weight, out of 256, given to each new sample in the moving average of heater power (`x10WattHistory`) that the screens display. A larger value follows the real power faster. |
| `SLEW_LIMIT` | not defined | not defined | Left off. The fork added a commented-out `#define SLEW_LIMIT 100` and a note that heater power is applied without a soft start. |

### Constants outside `configuration.h`

| Constant | File | Value | Upstream | What it controls |
| --- | --- | --- | --- | --- |
| `hs02HighTempRampStartDegC` | `source/Core/Threads/PIDThread.cpp` | 500 | 350 | Set point in °C above which `startPIDTask()` walks the target up in steps. The target is capped at 450 °C before this test, so the ramp no longer applies and boost and high set points take effect at once. `hs02HighTempRampStepTicks` is unchanged and unused while the start is above `MAX_TEMP_C`. |
| `GUITaskStackSize` | `source/Core/Src/main.cpp` | `(1024 + 256) / 2` | `1024 / 2` | Length of `GUITaskBuffer`, the GUI task's stack, in 32-bit words. |
| `lcdInitCmds` delays | `source/Core/Drivers/LCD.cpp` | 120 ms after `ST7735_SWRESET`, 120 ms after `ST7735_SLPOUT`, 10 ms after `ST7735_DISPON` | 150, 200, 100 ms | Waits in the LCD initialisation command chain. |
| `KXJT3_ORIENTATION_THRESHOLD` | `source/Core/Drivers/KXTJ3_defines.h` | 2900 with `MODEL_HS02`, otherwise 2000 | 2000 | Accelerometer reading, in counts of about 4096 per g, beyond which the iron counts as held left- or right-handed. See the section on that file. |
| `kCal100MinAboveZerouV`, `kCal100MaxuV` | `source/Core/Src/settingsGUI.cpp` | 50, 6000 | not present | Limits in microvolts for the reading that `setCalibrate100()` stores as the 100 °C point: at least 50 above the `CalibrationOffset` setting (below that there is no signal to use) and at most 6000, the most the `Cal100uV` setting holds. Any reading in between is stored. |
| `kHs02CjcBoardWarmCx10` | `source/Core/Threads/UI/logic/CJC.cpp` | 350 | not present | MCU temperature in tenths of a °C (35.0 °C) at or above which the CJC calibration asks before it runs. |
| `kCardValueRow`, `kCardHintRow` | `source/Core/Threads/UI/logic/SettingsMenu.cpp` | 32, 64 | not present | Pixel rows where the value and the hint row of the card menu start. |
| `CUSTOM_BUILD` | `Translations/make_translation.py` | 42 | not present | Number of this custom build. It becomes the text of `SmallSymbolCustomBuild`, shown on the first debug page. |

## Settings added

`source/Core/Inc/Settings.h` adds nine entries to `SettingsOptions` and moves the end marker `SettingsOptionsLength` from 56 to 65. Their limits are in `settingsConstants[]` in `source/Core/Src/Settings.cpp`; each has an increment of 1.

| No. | Name | Range | Default | Meaning | Read by |
| --- | --- | --- | --- | --- | --- |
| 56 | `BuzzerEnabled` | 0–1 | 1 | 1 lets the buzzer sound. | `setBuzzer()`, `setBuzzerTone()` (`BSP.cpp`), `hs02TuneStart()` (`Hs02Tune.cpp`), `displayBuzzerEnabled()` |
| 57 | `ColourReady` | 0–8 | 0 | Accent colour of the home screen while idle. | `drawCardIdle()` through `cardPaletteFor()`, `displayColourReady()` |
| 58 | `ColourHeating` | 0–8 | 0 | Accent colour of the soldering screen. | `drawCardSoldering()`, `displayColourHeating()` |
| 59 | `ColourBoost` | 0–8 | 0 | Accent colour of the soldering screen while boosting. | `drawCardSoldering()`, `displayColourBoost()` |
| 60 | `ColourSleep` | 0–8 | 0 | Accent colour of the sleep screen. | `drawCardSleep()`, `displayColourSleep()` |
| 61 | `ColourMenu` | 0–8 | 0 | Accent colour of the settings menu and other mono screens. | `guiHandleDraw()` (`GUIThread.cpp`), `displayColourMenu()` |
| 62 | `LowVoltageAllowed` | 0–1 | 0 | 1 accepts a DC supply below 9 V, down to `DC_MINIMUM_VOLTAGE_X10`. | `lookupVoltageLevel()` (`Settings.cpp`), `displayLowVoltageAllowed()` |
| 63 | `ButtonLight` | 0–1 | 1 | 1 switches on the white light behind the DOWN and UP buttons. | `hs02ApplyButtonLight()` (`BSP.cpp`), `displayButtonLight()` |
| 64 | `Cal100uV` | 0–6000 | 0 | The tip amplifier's reading in microvolts with the tip at 100 °C. 0 means not calibrated. | `fallbackTipTempCx10()` (`ThermoModel.cpp`), `displayCalibrate100()` |

Colour settings: 0 is "Auto", the theme's own colour for that screen. 1 to 8 select a fixed colour from the table in `hs02CardAccent()`: 1 Blue `0x3DFF`, 2 Green `0x262B`, 3 Red `0xEA28`, 4 Orange `0xF4E1`, 5 Yellow `0xFE62`, 6 Purple `0xA45F`, 7 Pink `0xF396`, 8 White `0xFFFF`.

The menu entries for these settings, and the code that reads the five colour settings, are compiled only with `LCD_160x80` and `HS02_THEME_CARD`. The code that reads `BuzzerEnabled`, `LowVoltageAllowed` and `ButtonLight` is outside that guard.

`Cal100uV` is not edited as a value in the menu. `setCalibrate100()`, the handler of the "Calibrate at 100C" menu entry, measures and writes it; that entry is inside the same guard. `fallbackTipTempCx10()`, which reads the setting, is outside it.

One existing row changed: `MinVoltageCells` now ranges from 25 to 35 (2.5 V to 3.5 V per cell); upstream allows 24 to 38. The `CalibrationOffset` row is unchanged, but it takes its lower limit and its default from `MIN_CALIBRATION_OFFSET` and `CALIBRATION_OFFSET`, which are both 0 for this board (see "Other constants in `configuration.h`").

## `source/Core/BSP/Fnirsi/Setup.cpp`

#### `static void gpioInit(void)`

Configures the GPIO pins at boot. The fork removed the line that switched the button LEDs (`LED1_Pin`) on here when `BUTTON_BACKLIGHT != 0`. The LEDs now stay off until `hs02ApplyButtonLight()` runs in the GUI loop, after the settings are loaded.

## `source/Core/BSP/Fnirsi/BSP.cpp`

#### `void hs02CurrentSenseSample(void)`

New. Collects statistics of the heater current sense input for a debug page and changes nothing about heating. Called once per GUI frame from `guiRenderLoop()`. It takes the eight buffered current sense samples from `ADCReadings` (index 1 and every `ADC_CHANNELS`-th entry after it), computes their mean and their highest value, and reads the heater level from `pendingPWM` (0 to 127). With the heater off it stores the mean as the "off" value. With the heater on it raises the stored peak if needed and, when the level is at least the highest level seen so far, stores the level, the mean and the input voltage. All values are raw ADC counts; the comment gives their range as 0 to 4095. State kept in the file-level statics `currentSenseOffMean`, `currentSenseOnMean`, `currentSensePeak`, `currentSenseOnVoltsX10` and `currentSenseOnLevel`, which are never reset after power-on.

#### `void hs02GetCurrentSense(uint16_t &offMean, uint16_t &onMean, uint16_t &peak, uint8_t &onPwmPercent, uint16_t &onVoltsX10)`

New. Returns the statistics collected by `hs02CurrentSenseSample()`. `offMean`, `onMean` and `peak` are raw ADC counts, `onPwmPercent` is the heater level at which `onMean` was taken (level × 100 / 127), and `onVoltsX10` is the input voltage at that moment in tenths of a volt. Called by `drawCurrentSensePage()` while that debug page is shown.

#### `void hs02ApplyButtonLight(void)`

New. Sets or clears `LED1_Pin` on `LED_Port` according to the `ButtonLight` setting. Called once per GUI frame from `guiRenderLoop()`. Compiles to nothing when `BUTTON_BACKLIGHT` is 0.

#### `void setBuzzer(bool on)`

Changed. Sets the TIM2 compare value to `BUZZER_VOLUME` when `on` is true and to 0 otherwise. The fork added the condition that the `BuzzerEnabled` setting must be non-zero for the buzzer to sound. Called from `Soldering.cpp` (a beep of a third of a second when the tip reaches the target; see `gui_solderingMode()`) and from `SolderingProfile.cpp`.

#### `void setBuzzerTone(uint16_t hz)`

New. Sounds the buzzer at a pitch given in Hz by changing the period of TIM2. A value below 100, or `BuzzerEnabled` being 0, silences the buzzer and restores the period 127 that `setBuzzer()` expects. Otherwise the period is `256000 / hz` timer ticks, the counter is reset, and the compare value is `period * BUZZER_VOLUME / 128`. The comment notes that the 256 kHz tick rate is taken from `Setup.cpp`, and that every tone sounds an octave lower if the timer clock is really half that. Called from `Hs02Tune.cpp`.

#### `void showBootLogo(void)`

Changed. With `HS02_THEME_CARD` it returns at once: no logo is drawn and the backlight stays off. `guiRenderLoop()` switches the backlight on later. Called from `guiHandleDraw()` in the `StartupLogo` mode.

## `source/Core/BSP/Fnirsi/ThermoModel.cpp`

#### `uint32_t fallbackTipTempCx10(uint32_t tipuV)`

Changed (anonymous namespace). Converts the tip sensor voltage to an absolute tip temperature in tenths of a °C, by linear interpolation along a curve. `tipuV` is in microvolts with the `CalibrationOffset` setting already removed. Called by `TipThermoModel::convertuVToDegCx10()` when no valid factory calibration is found.

The curve is in signal microvolts: the amplifier reading minus the unit's zero, which is its reading at no temperature difference. Its nodes are:

| Signal in microvolts | Temperature | Source, as the comment gives it |
| --- | --- | --- |
| 0 | 25.0 °C | Assumed, as the stock firmware does. At no signal the tip is as warm as the point it plugs into, which no sensor on this board measures. |
| 2000 (`kSignalAt100`), or the measured value described below | 100.0 °C | Not measured: the straight line from 25 °C to 223 °C, bent by the 3 % by which the stock firmware's own curve is flatter at the low end. |
| 5400, 6680, 8370, 10540, 12090, 13640 | 223.0, 273.0, 350.0, 400.0, 450.0, 500.0 °C | Measured by the port's author on one HS-02A, with a zero of 900 microvolts taken off. |

The function works in these steps:

- It adds the `CalibrationOffset` setting back to `tipuV`, which gives the amplifier reading.
- It chooses the zero and the 100 °C node from the settings `CalibrationOffset` and `Cal100uV` (the amplifier reading with the tip at 100 °C, 0 if not calibrated):
  - `Cal100uV` is 0: the zero is `CalibrationOffset` and the node is at 2000 microvolts.
  - `Cal100uV` is not 0 and `CalibrationOffset` is 0: the zero is `Cal100uV` − 2000 microvolts, which can be negative. The node stays at 2000 microvolts.
  - Both are above 0: the zero is `CalibrationOffset`. If `Cal100uV` is above `CalibrationOffset`, the node moves to `Cal100uV` − `CalibrationOffset`, so the 100 °C point shapes the low end of the curve; otherwise `Cal100uV` is not used.
- The signal is the reading minus the zero. If the zero is negative and the reading is below 300 microvolts (`kBlend`), the signal is instead the reading × (300 − zero) / 300, so the first 300 microvolts of reading run from 25 °C up to the place where the curve applies again. The comment gives the reason: the amplifier cannot go below 0, so readings near 0 only say that the tip is no warmer than that.
- A signal of 0 or less returns 250 (25.0 °C). Otherwise the result is interpolated within the segment that contains the signal; a signal above the 450 °C node uses the last segment, also beyond 500 °C.

The comment explains the two settings. The zero differs from unit to unit by more than the curve can ignore: 100 microvolts is about 4 °C at soldering temperatures. The CJC calibration reads the amplifier with a cold tip, which gives the zero exactly if it is above 0; if it is below 0 the amplifier's output sits at its lower limit, the reading is 0 and the true zero stays unknown. The 100 °C calibration (`setCalibrate100()`) reads the amplifier with the tip in boiling water, and the zero is then that reading minus the signal expected at 100 °C. The comment reports one HS-02A that read 0 microvolts cold and about 1300 microvolts at 100 °C, a zero near −700 microvolts.

Compared with upstream, the fork changed the following. Upstream compares `tipuV` with the points directly; the fork derives the zero from the two settings as described. The cold end is 25.0 °C; upstream uses 30.5 °C. The 100 °C node and the handling of a negative zero are new. The points from 223 °C up are the upstream values.

#### `TemperatureType_t TipThermoModel::convertuVToDegCx10(uint32_t tipuVDelta)`

Changed. Returns the tip temperature above the handle temperature in tenths of a °C; the generic caller adds the handle temperature back. The factory-calibration branch is unchanged. In the fallback branch the fork removed the clamp at zero: the function now returns `fallbackTipTempCx10(tipuVDelta) - getHandleTemperature(0)` as a signed value, which can be negative. The comment explains that the clamp made the MCU reading, not the curve, the lowest temperature ever shown.

## `source/Core/Drivers/LCD.hpp` and `source/Core/Drivers/LCD.cpp`

Everything in this section except the delays in `lcdInitCmds` (see "Constants outside `configuration.h`") is compiled only with `HS02_THEME_CARD`.

New static members of `LCD`: `monoRegions[4]` and `monoRegionsPending[4]`, both initialised to `{0, 0, 255, 255}`, and `monoAccent` and `monoAccentPending`, both initialised to `0x3DFF`. The "pending" copies are what screens ask for while drawing; `LCD::refresh()` takes them over.

#### `static void LCD::initialize()`

Changed. After the initialisation commands, the card theme fills the whole panel memory with the card background colour `0x0042`, sent as its bitwise inverse in 32-byte chunks, and sets `displayChecksum` to `~0U` so that the first real frame is always sent. This is done because the card theme shows no boot logo and the panel's power-on contents would otherwise be visible when the backlight comes on. Called once from `startGUITask()` through `Display::initialize()`.

#### `static void LCD::refresh(const bool force = false)`

Changed (inline in `LCD.hpp`). The colour-mode path is unchanged. In mono mode the card theme first copies the pending accent colour and regions into the active ones, then calls `refreshMonoThemed()` if the buffer checksum changed, `force` is set, or the accent colour or a region changed. Called at the end of every GUI frame through `Display::refresh()`, and from blocking screens.

#### `static void LCD::setMonoAccentColour(uint16_t rgb565)`

New (inline). Stores the accent colour for the next refresh of a mono screen. Called by `guiHandleDraw()` at the start of every frame.

#### `static void LCD::setMonoRegions(uint8_t accentFromRow, uint8_t accentToRow, uint8_t accentFromColumn, uint8_t mutedFromRow)`

New (inline). Stores which parts of the next mono refresh are coloured. Lit pixels are white, except that rows `accentFromRow` to `accentToRow - 1` and columns from `accentFromColumn` take the accent colour, and rows from `mutedFromRow` are grey; grey takes precedence. Arguments are pixel rows and columns; 255 places a region off the screen. `guiHandleDraw()` resets the request to `(0, 0, 255, 255)` every frame, so a screen that wants colour calls this while it draws. Callers: `render_menu()`, `userConfirmation()`, `setCalibrateVIN()`, `cal100DrawRows()` and `hs02CjcMayRun()`.

#### `static bool LCD::cardScrollBegin()`

New. Prepares the card menu's slide animation and reports whether it applies. It returns true only in mono mode when both the active and the pending `mutedFromRow` are 64, which is the case between two card menu screens. On true it copies the position bar (the last four columns of the top eight strips) and the hint row (rows 64 to 79) from the secondary frame into the primary frame, so those parts show the new screen from the start, and invalidates the checksum. On false the caller uses the upstream slide. Called by `Display::transitionScrollDown()` and `Display::transitionScrollUp()`. Stores its result in the file-level static `cardScrollActive`.

#### `static void LCD::cardScrollStep(bool down)`

New. Moves the menu entry by one pixel row. The top 64 rows of the primary and secondary frames are treated as one 128-row column, leaving out the last four columns. With `down` true the rows move up and the new entry comes in from below; with `down` false they move down and the new entry comes in from above. Both frames are modified. Called 64 times per transition.

#### `static void LCD::refreshMonoThemed()`

New (private). Sends the 1 bit per pixel frame to the panel as RGB565: background `0x0042`, lit pixels white `0xFFFF`, the accent colour `monoAccent` or grey `0x9D37` according to `monoRegions`. It sends strip by strip (ten strips of eight rows), eight columns per SPI transfer, with each colour inverted as the panel requires. Called by `LCD::refresh()`. Uses a static 128-byte buffer `wire` so the data is not on the GUI task's stack.

## `source/Core/Drivers/Display.cpp`

#### `void Display::transitionScrollDown(const TickType_t viewEnterTime)`

Changed. With `LCD_160x80` and `HS02_THEME_CARD` it first calls `LCD::cardScrollBegin()`. If that returns true it calls `LCD::cardScrollStep(true)` 64 times, forcing a refresh after every fourth step and waiting `TICKS_100MS / 7` between refreshes, then returns. This path does not check the buttons. Otherwise the upstream whole-screen slide runs. Called by `guiRenderLoop()` for `TransitionAnimation::Down`. It blocks until the slide is finished.

#### `void Display::transitionScrollUp(const TickType_t viewEnterTime)`

Changed in the same way, with `LCD::cardScrollStep(false)`. Called by `guiRenderLoop()` for `TransitionAnimation::Up`.

## `source/Core/Drivers/KXTJ3_defines.h`

`KXJT3_ORIENTATION_THRESHOLD` is 2900 with `MODEL_HS02` and 2000, the upstream value, otherwise. `KXTJ3::getOrientation()` compares the accelerometer's x reading with it: a reading above the threshold is `ORIENTATION_RIGHT_HAND`, one below the negative threshold is `ORIENTATION_LEFT_HAND`, and anything between is `ORIENTATION_FLAT`. The comment gives the scale as about 4096 counts per g, which makes 2000 a tilt of about 29 degrees and 2900 about 45 degrees, and states that on the HS-02 the smaller value flipped the screen when the iron was only turned a little past level. The result is used by `startMOVTask()`.

## `source/Core/Inc/power.hpp`

`wattHistoryFilter`, the weight of the `x10WattHistory` moving average, was the literal 24. It is now `WATT_HISTORY_WEIGHT`, which defaults to 24 here and is set to 96 for this board in `configuration.h`.

## `source/Core/Threads/PIDThread.cpp`

The constant `hs02HighTempRampStartDegC` and the default definitions of `TIP_PID_INPUT_SCALE`, `TIP_PID_INTEGRAL_ZONE` and `TIP_PID_INTEGRAL_MAX_RATE` are described under "Build switches".

#### `T PID::update(const T set_point, const T new_reading, const TickType_t interval_ms, const T max_output)`

Changed (member of `template <class T, T Kp, T Ki, T Kd, T integral_limit_scale> struct PID`). Computes one PID step and returns the heater power in tenths of a watt, limited to 0 to `max_output`. `set_point` and `new_reading` are temperatures in units of 1 / `TIP_PID_INPUT_SCALE` °C, and `interval_ms` is the time since the previous call. State kept in the members `previous_error_term` and `integration_running_sum`.

The fork changed the following:

- The proportional and derivative terms are divided by `TIP_PID_INPUT_SCALE`, so the gains keep their meaning at either resolution.
- The integral accumulates only if, in addition to the upstream saturation test, the error is within `TIP_PID_INTEGRAL_ZONE` °C of zero and the error is shrinking no faster than `TIP_PID_INTEGRAL_MAX_RATE` °C per second. A value of 0 disables the respective test. Outside these conditions the sum is kept, not cleared.
- The integral increment is scaled back to whole degrees before it is multiplied by the interval, to stay inside 32 bits.
- The test that switches the output off below `MIN_TEMP_C` compares against `MIN_TEMP_C * TIP_PID_INPUT_SCALE`.

#### `static int32_t getPIDResultX10Watts(TemperatureType_t set_point, TemperatureType_t current_reading)`

Changed. Returns the controller output in tenths of a watt. `set_point` is in °C and `current_reading` in tenths of a °C. In the `TIP_CONTROL_PID` branch the fork passes `set_point * TIP_PID_INPUT_SCALE` and `current_reading * TIP_PID_INPUT_SCALE / 10` to `PID::update()`; upstream passes `set_point` and `current_reading / 10`. Called by `startPIDTask()` each time the ADC has new samples. Keeps the static `lastCall` and the static controller instance `ctrl`.

## `source/Core/Threads/MOVThread.cpp`

#### `void startMOVTask(void const *argument __unused)`

Changed. The movement task's entry function. Its loop reads the accelerometer once per `MOV_POLL_DELAY`, which is 50 ms on the HS-02. With `MODEL_HS02` the fork changed how the automatic display orientation (`OrientationMode` setting 2) is applied. Upstream calls `Display::setRotation()` on every reading that is not `ORIENTATION_FLAT`. Here the display is turned only after 10 readings in a row, half a second, that ask for an orientation other than the one applied last. A reading of `ORIENTATION_FLAT`, or one that agrees with the applied orientation, sets the count back to 0. The comment gives the reason: one reading past the threshold while the iron is being moved is not enough.

State is kept in two statics: `flipVotes`, the count, and `applied`, the orientation applied last (−1 none yet, 0 right-handed, 1 left-handed). While `OrientationMode` is not 2, `applied` is set to −1 on every pass, so the orientation is applied afresh when automatic mode is selected again. The comment states why the applied orientation is remembered here and not read back from the display: `Display::getRotation()` is inverted on boards built with `DISPLAY_FLIP`.

## `source/Core/Inc/Settings.h` and `source/Core/Src/Settings.cpp`

The new enum entries and their rows in `settingsConstants[]`, and the changed `MinVoltageCells` range, are listed under "Settings added".

#### `uint8_t lookupVoltageLevel()`

Changed. Returns the undervoltage threshold in tenths of a volt. When the power source setting `MinDCVoltageCells` is 0 (DC) and the board defines `DC_MINIMUM_VOLTAGE_X10`, it now returns that value if `LowVoltageAllowed` is on and 90 otherwise; upstream always returns 90 in this case. The battery-cell branch is unchanged. Called by `checkForUnderVoltage()` in `checkUndervoltage.cpp`.

## `source/Core/Src/main.cpp`

No function changed. `GUITaskStackSize` is larger than upstream; see "Constants outside `configuration.h`".

## `source/Core/Threads/GUIThread.cpp`

#### `OperatingMode guiHandleDraw(void)`

Changed. Draws one frame of the current operating mode and returns the mode to switch to. With `HS02_THEME_CARD` it now starts every frame by setting the mono accent colour to `hs02CardAccent(SettingsOptions::ColourMenu, 0x3DFF)` (blue unless a menu colour is chosen) and resetting the mono regions to all white with `LCD::setMonoRegions(0, 0, 255, 255)`. Called by `guiRenderLoop()` once per frame, and a second time when a transition needs the next frame in the secondary buffer.

#### `void guiRenderLoop(void)`

Changed. Runs one GUI frame: draw, mode change, transition, refresh. The fork added:

- With `MODEL_HS02`: calls to `hs02CurrentSenseSample()` and `hs02ApplyButtonLight()` at the start of every frame.
- With `MODEL_HS02` and `HS02_THEME_CARD`: a call to `hs02TuneStop()` whenever the current mode is not `HomeScreen`, then a call to `hs02TuneTick()`.
- With `LCD_160x80` and `HS02_THEME_CARD`: after the frame has been sent, the first time the current mode is a colour screen (home, soldering, sleeping, hibernating) and the frame was not the startup logo step, it sets the backlight to the `DisplayBrightness` setting. The static `backlightRevealed` makes this happen once.

Called from the loop in `startGUITask()`.

#### `void startGUITask(void const *argument)`

Changed. The GUI task's entry function. With `MODEL_HS02` and `HS02_THEME_CARD` the loop compares the operating mode before and after `guiRenderLoop()`. If the mode changed and the previous mode was `InitialisationDone`, `StartupWarnings` or `StartupLogo`, it restarts the loop at once instead of waiting out the 40 ms frame period, so start-up steps with nothing to show do not each cost a frame.

## `source/Core/Src/settingsGUI.cpp`

### Macros

- `SETTING_VALUE_TEXT(id)`: the text for a setting value that the upstream menu shows as one letter. In the card theme it expands to the constant `LargeCardWord<id>` (a whole English word of up to six large characters); otherwise to `translatedString(Tr->id)`.
- `SETTING_VALUE_OFF()`: draws the marker for a switched-off setting. In the card theme it prints `LargeCardWordOff` ("Off") in the large font; otherwise it calls `Display::drawUnavailableIcon()`.

### Menu tables

All of the following apply only with `LCD_160x80` and `HS02_THEME_CARD`:

- `powerMenu[]` gains an entry for `LowVoltageAllowed` (inside `#ifdef POW_DC`).
- `UIMenu[]` gains entries for `BuzzerEnabled`, `ButtonLight`, `ColourReady`, `ColourHeating`, `ColourBoost`, `ColourSleep` and `ColourMenu`, and loses the entry for `ReverseButtonSettings`. The card menu has a fixed button layout and does not read that setting.
- `advancedMenu[]` gains the entry `Calibrate100` between `CalibrateCJC` and `VoltageCalibration`, with the handler `setCalibrate100()` and the `draw` callback `displayCalibrate100()`, and the entry `DebugPages`, whose handler is `setOpenDebugPages()`. Neither entry is bound to a setting (`SettingsOptions::SettingsOptionsLength`), so the card menu runs the handler on OK.

The global `bool hs02DebugPagesRequested` (initially false) carries the request from `setOpenDebugPages()` to `cardHandleButtons()` in `SettingsMenu.cpp`.

### Functions

#### `static void printTemperatureUnit(void)`

New. In the card theme it prints `LargeSymbolDegF` or `LargeSymbolDegC` in the large font, according to the `TemperatureInF` setting. In other builds it does nothing. Called by the display functions of temperature values after the number.

#### `static int userConfirmation(const char *message)`

Changed. Asks the user to confirm an action and returns 1 for yes and 0 for no. Called by `setCalibrate()` and `setResetSettings()` when their menu entry is activated. It blocks until a button is pressed.

In the card theme, the messages `SettingsCalibrationWarning` and `SettingsResetWarning` are replaced by fixed short texts (`SmallCardConfirmCalibrate1` to `4`, `SmallCardConfirmReset1` to `3`), one small-font row of 16 pixels each, above a grey hint row reading no / no / yes in button order. OK short returns 1 and any other button returns 0. The loop redraws every 40 ms. Any other message still scrolls as upstream, with the mono regions reset to one colour.

#### `static void setCalibrate(void)`

Changed. The handler of the `CalibrateCJC` menu entry. If the `CalibrateCJC` setting is 0 it asks for confirmation with `userConfirmation()` and, on yes, sets the setting to 1; if the setting is already 1 it sets it back to 0. The fork added a call to `saveSettings()` straight after either change; upstream does not save here. The comment gives the reason: the next thing the user does is unplug the iron.

#### `static void setOpenDebugPages(void)`

New (card theme). The handler of the "Debug pages" menu entry. It sets `hs02DebugPagesRequested`; a menu handler cannot change screen itself, so `cardHandleButtons()` picks the request up straight after calling it.

#### `static void cal100DrawRows(const char *const *rows, uint8_t count, bool question)`

New (card theme). Clears the screen and prints `count` rows of text from `rows` in the small font, 16 pixels apart from the top. With `question` true it adds the row no / no / yes at pixel row 64, in button order (DOWN, UP, OK). It sets the mono regions so that the rows from 64 are grey and the rest is white. It does not refresh the display. Called by `setCalibrate100()` for each of its screens.

#### `static void setCalibrate100(void)`

New (card theme). The handler of the "Calibrate at 100C" menu entry. It measures the tip amplifier's reading while the tip is held in boiling water and stores it in the `Cal100uV` setting, which `fallbackTipTempCx10()` uses as the 100 °C point of its curve. The comment states that the heater is off everywhere in the menu, and that 100 °C is assumed although water boils about 1 °C lower for every 300 m above sea level. Called by `cardHandleButtons()` on OK short. It blocks until it returns, in these steps:

- It shows the question "Tip held in / boiling water / 30 seconds?" and redraws it every 40 ms. OK short carries on; any other button returns without a measurement.
- It shows "Measuring" and takes 16 samples with `getTipRawTemp(1)`, 100 ms apart; the comment says the CJC calibration takes them the same way. Their mean is converted to microvolts before the offset is removed, with `TipThermoModel::convertTipRawADCTouV(mean, true)`.
- The reading is accepted if it lies at least `kCal100MinAboveZerouV` (50 microvolts) above the `CalibrationOffset` setting and is not above `kCal100MaxuV` (6000 microvolts). It is not checked for being near the value expected at 100 °C. An accepted reading is written to `Cal100uV` and `saveSettings()` is called. A reading outside these limits changes nothing.
- It shows the result until a button is pressed: "100C saved at" or "No usable / reading", with the reading in microvolts on the row below. Leaving either the question or the result with a button sets `hs02MenuWaitForRelease`, so the card menu ignores that button until it is released.

#### `static void displayCardColour(SettingsOptions option)`

New (card theme). Prints the name of the colour selected by one of the accent colour settings, from `LargeCardColour0` ("Auto") to `LargeCardColour8`, in the large font. A stored value outside the table is shown as "Auto". Called by the five wrappers below.

#### `static void setCalibrateVIN(void)`

Changed. The blocking screen that calibrates the input voltage reading by adjusting the `VoltageDiv` setting. In the card theme it adds a hint row (− / + / save), draws the screen in one colour, and maps the buttons as follows: DOWN short calls `nextSettingValue()` (a larger divider shows a lower voltage), UP short calls `prevSettingValue()`, and OK short or OK long saves and leaves, in addition to the upstream exits. In other builds the mapping is unchanged.

### New display functions (card theme)

Each is the `draw` callback of its menu entry and is called by `render_menu()` on every frame the entry is shown.

| Function | What it draws |
| --- | --- |
| `static void displayBuzzerEnabled(void)` | A checkbox for `BuzzerEnabled`. |
| `static void displayButtonLight(void)` | A checkbox for `ButtonLight`. |
| `static void displayLowVoltageAllowed(void)` | A checkbox for `LowVoltageAllowed`. |
| `static void displayCalibrate100(void)` | A checkbox that is ticked when `Cal100uV` is not 0, that is, once a 100 °C point is stored. |
| `static void displayColourReady(void)` | `displayCardColour(SettingsOptions::ColourReady)`. |
| `static void displayColourHeating(void)` | `displayCardColour(SettingsOptions::ColourHeating)`. |
| `static void displayColourBoost(void)` | `displayCardColour(SettingsOptions::ColourBoost)`. |
| `static void displayColourSleep(void)` | `displayCardColour(SettingsOptions::ColourSleep)`. |
| `static void displayColourMenu(void)` | `displayCardColour(SettingsOptions::ColourMenu)`. |

### Changed display functions

These upstream `draw` callbacks keep their behaviour in other builds. In the card theme they differ as shown: "Off" means `SETTING_VALUE_OFF()` replaces the "unavailable" icon, "word" means `SETTING_VALUE_TEXT()` replaces the one-letter text, "unit" means `printTemperatureUnit()` is called after the number.

| Function | Change |
| --- | --- |
| `static void displayPDNegTimeout(void)` | Off |
| `static void displayBoostTemp(void)` | Off, unit |
| `static void displayAutomaticStartMode(void)` | Off, word ("Solder", "Sleep", "Room") |
| `static void displayTempChangeShortStep(void)` | unit |
| `static void displayTempChangeLongStep(void)` | unit |
| `static void displayLockingMode(void)` | Off, word ("Boost", "Full") |
| `static void displaySensitivity(void)` | Off |
| `static void displaySleepTemp(void)` | unit |
| `static void displaySleepTime(void)` | Off |
| `static void displayShutdownTime(void)` | Off |
| `static void displayHallEffect(void)` | Off |
| `static void displayDisplayRotation(void)` | word ("Right", "Left", "Auto") |
| `static void displayScrollSpeed(void)` | word ("Fast", "Slow") |
| `static void displayAnimationSpeed(void)` | Off, word ("Slow", "Medium", "Fast") |
| `static void displayLogoTime(void)` | Off |
| `static void displayPowerLimit(void)` | Off |
| `static void displayPowerPulse(void)` | Off |
| `static void displayReverseButtonSettings(void)` | Not compiled in the card theme, together with its menu entry. |

## `source/Core/Threads/UI/logic/SettingsMenu.cpp`

The card theme menu has three rows: the setting's name, its value, and a grey hint row that names what DOWN, UP and OK do. While browsing, UP and DOWN move between entries and OK opens a group, starts editing or runs an action. While editing, DOWN is −, UP is + (hold to repeat) and OK finishes. Holding OK goes back one level. The scrolling help text of the upstream layout is not shown.

The card menu uses these fields of `cxt->scratch_state`: `state1` is the index in the root menu, `state2` is 0 in the root menu or the index in the sub-menu plus 1, `state3` and `state4` hold the auto-repeat acceleration and timer, `state5` caches the menu length (0 means recount), and `state7` is non-zero while a value is being edited.

The global `bool hs02MenuWaitForRelease` (initially false) is set by the home screen when the menu is opened by holding a button.

All functions named `card*` are new and compiled only with `LCD_160x80` and `HS02_THEME_CARD`.

#### `static void printShortDescription(SettingsItemIndex settingsItemIndex, uint16_t cursorCharPosition)`

Changed. Prints the setting's short name and places the cursor for its value. In the card theme the cursor goes to (4, 32) instead of (4, 40), because the bottom 16 rows are the hint row. Called by `render_menu()`.

#### `static const menuitem *cardCurrentMenu(guiContext *cxt)`

Returns the menu being shown: `rootSettingsMenu` when `state2` is 0, otherwise the sub-menu selected by `state1`.

#### `static void cardStepEntry(guiContext *cxt, int8_t direction)`

Moves to the next (`direction` +1) or previous (−1) visible entry of the current menu and wraps round at either end. If no other entry is visible it does nothing. Otherwise it updates `state1` or `state2`, clears the cached menu length and requests the `Down` or `Up` transition. Called by `cardHandleButtons()` on a DOWN or UP press while browsing.

#### `static bool cardItemCycles(const menuitem *item)`

Returns true for the two entries that the card menu edits to the degree: those with an increment handler whose setting is `SleepTemp` or `BoostTemp`.

#### `static void cardStepTemperature(const menuitem *item, bool up, bool held)`

Changes the sleep or boost temperature setting. A press (`held` false) changes it by one degree; a held button moves it to the next multiple of ten in that direction. Values are in the unit selected by `TemperatureInF`. The sleep temperature ranges from 10 to 300 °C (60 to 580 °F) and wraps round. The boost temperature ranges from `MIN_BOOST_TEMP_C` to `MAX_TEMP_C` (or the °F equivalents) and has an "off" position, 0, beyond either end. Called by `cardHandleButtons()` while editing.

#### `static bool cardItemHasValue(guiContext *cxt, const menuitem *item)`

Returns true if the entry is a setting that the card menu edits with DOWN and UP: it is in a sub-menu, its `autoSettingOption` is a real setting, and it either has no increment handler or is one of the two temperature entries.

#### `static void cardDrawHints(guiContext *cxt, const menuitem *item)`

Clears and draws the hint row at pixel row 64. The OK slot (x 108) shows "save" while editing, "open" for a root entry without a handler, "edit" for an entry with an editable value, and "set" otherwise. The DOWN and UP slots show "next" and "prev" while browsing, or "-" and "+" while editing; they are left empty while browsing a menu with fewer than two visible entries. Called by `render_menu()` every frame.

#### `static OperatingMode cardHandleButtons(const ButtonState buttons, guiContext *cxt, const menuitem *item)`

Handles the buttons for the entry on screen and returns the next operating mode. Called by `gui_SettingsMenu()` every frame.

- While `hs02MenuWaitForRelease` is set it ignores the buttons until none is pressed, then clears the flag.
- OK long: ends editing. In the root menu it saves the settings and returns `HomeScreen`; in a sub-menu it returns to the root menu.
- OK short: ends editing if editing; starts editing if `cardItemHasValue()`; otherwise calls the entry's increment handler if it has one; otherwise, in the root menu, opens the sub-menu. After a handler it checks `hs02DebugPagesRequested`; if set, it clears the flag, saves the settings and returns `DebugMenuReadout`.
- UP or DOWN, short or held: while editing, changes the value with `cardStepTemperature()`, `nextSettingValue()` (UP) or `prevSettingValue()` (DOWN); while browsing, calls `cardStepEntry()` with −1 for UP and +1 for DOWN.

A held button repeats with the upstream pacing: the interval between repeats is `PRESS_ACCEL_INTERVAL_MAX` less an acceleration that grows by `PRESS_ACCEL_STEP` with each repeat, and it does not go below `PRESS_ACCEL_INTERVAL_MIN`. Releasing the button resets the acceleration.

#### `void render_menu(const menuitem *item, guiContext *cxt)`

Changed. Draws one menu entry. In the card theme it prints the short name, calls the entry's `draw` callback and `cardDrawHints()`, then sets the mono regions: while editing, the value rows (32 to 63) are in the accent colour; while browsing, the name rows (0 to 31) are. In both cases the last four columns (the position bar) are in the accent colour and the rows from 64 are grey. It then returns, skipping the upstream help-text logic. Called by `gui_SettingsMenu()`.

#### `OperatingMode gui_SettingsMenu(const ButtonState buttons, guiContext *cxt)`

Changed. The settings menu's per-frame function, called by `guiHandleDraw()` in the `SettingsMenu` mode. In the card theme it replaces the flashing scroll indicator by a position bar: a track on the right edge above the hint row (62 pixels high) with a thumb of at least 6 pixels that shows where the entry sits in its menu. It then returns the result of `cardHandleButtons()` instead of running the upstream button handling, so the `ReverseButtonSettings` setting is not read.

## `source/Core/Threads/UI/logic/HomeScreen.cpp`

The additions are guarded by `MODEL_HS02` and `HS02_THEME_CARD`. The home screen buttons are: UP or DOWN short changes the set temperature, UP held opens the settings menu, DOWN held opens the temperature adjust screen, OK starts soldering, and UP and DOWN held together plays a tune.

The file-level static `hs02SetTempSaveAt` holds the tick at which a changed set temperature is due to be written to flash, or 0 if nothing is pending.

#### `void hs02StepSetTemperature(bool up, bool large)`

New. Changes the `SolderingTemp` setting by one step up (`up` true) or down. With `large` false the step is the `TempChangeShortStep` setting, but at least 5; with `large` true it is the `TempChangeLongStep` setting, but at least 10. The result is the next multiple of the step in that direction, limited to `MIN_TEMP_C` to `MAX_TEMP_C`, or the °F limits when `TemperatureInF` is set. The value and the step are in the selected unit. It does not write to flash; it sets `hs02SetTempSaveAt` to two seconds from now. Called with `large` false by `handleHomeButtons()` and by `handleSolderingButtons()` (`Soldering.cpp`) on a short UP or DOWN press, and with `large` true by `handleSolderingButtons()` for each step of a held UP or DOWN.

#### `void hs02SaveSetTemperatureIfDue(bool now)`

New. Calls `saveSettings()` if a save is pending and either `now` is true or the two seconds have passed, then clears the pending mark. Called by `handleHomeButtons()` and by `handleSolderingButtons()` every frame with `now` false, and with `now` true before they leave their screen.

#### `OperatingMode handleHomeButtons(const ButtonState buttons, guiContext *cxt)`

Changed. Handles the home screen buttons and returns the next operating mode. Called by `drawHomeScreen()` every frame. The card theme adds the following ahead of the upstream `switch`:

- `BUTTON_BOTH_LONG`: on the first frame it starts tune 0; once the buttons have been held for five seconds it starts tune 1. The static `bothHeldSince` records when the hold began.
- Any button while a tune plays stops the tune and does nothing else.
- UP short or DOWN short calls `hs02StepSetTemperature()` with `large` false. UP is "+" unless `ReverseButtonTempChangeEnabled` is on.
- UP held saves a pending set temperature, sets `hs02MenuWaitForRelease` and returns `SettingsMenu`.
- Any other press saves a pending set temperature and falls through to the upstream handling, where DOWN held opens the temperature adjust screen and OK short starts soldering.

## `source/Core/Threads/UI/logic/Soldering.cpp`

The changes are guarded by `MODEL_HS02` and `HS02_THEME_CARD`. The file declares `hs02StepSetTemperature()` and `hs02SaveSetTemperatureIfDue()`, which are defined in `HomeScreen.cpp`.

#### `OperatingMode handleSolderingButtons(const ButtonState buttons, guiContext *cxt)`

Changed. Handles the buttons of the soldering screen and returns the next operating mode. Called by `gui_solderingMode()` at the end of every frame. `cxt->scratch_state.state1` holds the lock state (0 unlocked and released, 1 unlocked, 2 locked, 3 locked and released) and `state2` is non-zero while boosting.

While the buttons are unlocked, the card theme runs its own `switch` in place of the upstream one; the comment says it is laid out to match the home screen. It first calls `hs02SaveSetTemperatureIfDue(false)`. Then:

- No button: sets `state1` to 0, and ends boost unless boost is on and `getButtonOK()` reports that the OK button is still down. The comment explains that `BUTTON_OK_LONG` is reported once per hold (`Buttons.cpp`) and is followed by `BUTTON_NONE`.
- UP short or DOWN short: calls `hs02StepSetTemperature()` with `large` false, so the target changes by one small step at once. UP is "+" unless `ReverseButtonTempChangeEnabled` is on.
- UP held or DOWN held: calls `hs02StepSetTemperature()` with `large` true, so the target changes by one large step, and repeats this for as long as the button is held. The number changes on the soldering screen; the function stays in the `Soldering` mode. The repeats have the pacing of the card menu (see `cardHandleButtons()`): the interval between steps is `PRESS_ACCEL_INTERVAL_MAX` less an acceleration that grows by `PRESS_ACCEL_STEP` with each step, and it does not go below `PRESS_ACCEL_INTERVAL_MIN`. The statics `repeatTimer` and `repeatAcceleration` hold the tick of the last step and the acceleration; both are set to 0 on every frame whose button state is neither UP held nor DOWN held. The direction follows `ReverseButtonTempChangeEnabled` as for a short press.
- OK short or `BUTTON_BOTH`: saves a pending set temperature and returns `HomeScreen`, which stops heating.
- OK held: starts boost if the `BoostTemp` setting is not 0. Boost stays on for as long as the OK button is down.
- UP and DOWN held together: locks the buttons if the `LockingMode` setting is not 0, as upstream.

The change to the home screen is made without a transition animation. In upstream, OK held, `BUTTON_BOTH` or UP held returns to the home screen, DOWN held starts boost, and any short press opens the temperature adjust screen.

While the buttons are locked, the press that starts boost (when `BoostTemp` is not 0 and `LockingMode` is `lockingMode_t::BOOST`) is OK held; upstream uses DOWN held. With no button, boost is kept on while the OK button is still down, as in the unlocked case; upstream ends it. The rest of the locked branch is unchanged.

#### `OperatingMode gui_solderingMode(const ButtonState buttons, guiContext *cxt)`

Changed. The per-frame function of the soldering screen, called by `guiHandleDraw()` in the `Soldering` mode. The card theme changes when the buzzer beeps. The tip counts as having reached the target while its reading is within 10 °C of `currentTempTargetDegC`. Upstream beeps for a third of a second each time the reading comes into this window, and arms the beep again whenever the reading leaves it. The card theme beeps in two cases only: the first time the reading is in the window after the soldering screen is entered, and later when the reading is in the window and the target is 20 °C or more above the target it last beeped for. The comment names boost and a set temperature turned well up as such higher targets, and states that the reading drifting out of the window and back, as it does under load, does not beep again.

`scratch_state.state5` is non-zero once it has beeped and `state6` holds the target in °C that it beeped for. `guiRenderLoop()` clears the scratch state on every change of operating mode, so both start at 0 each time the screen is entered. `state3` holds the tick at which the beep ends, as upstream. The status LED is set as upstream: `LED_HOT` inside the window and `LED_HEATING` outside it.

## `source/Core/Threads/UI/logic/utils/Hs02Tune.cpp`

New file, compiled only with `MODEL_HS02` and `HS02_THEME_CARD`. It plays one of two tunes on the buzzer: 0 is the opening of Beethoven's "Ode to Joy" and 1 is the opening of Grieg's "In the Hall of the Mountain King". Each tune is a table of `Note` entries (`kOdeToJoy`, `kMountainKing`), a pitch in Hz (0 is a rest) and a length in milliseconds. State is kept in three file-level variables: `playing` (the tune number, or `kIdle`), `position` (the note that sounds next) and `noteEnds` (the tick at which the current note ends).

#### `bool hs02TunePlaying(void)`

Returns true while a tune is playing. Called by `handleHomeButtons()`.

#### `void hs02TuneStart(uint8_t tune)`

Starts tune 0 or 1 from its first note. It does nothing if the `BuzzerEnabled` setting is off, the number is out of range, or that tune is already playing, so it can be called on every frame a button is held. A different tune replaces the one that is playing. Called by `handleHomeButtons()`.

#### `void hs02TuneStop(void)`

Stops the tune, if one is playing, and silences the buzzer. Called by `handleHomeButtons()` on any button press, by `guiRenderLoop()` on every frame in which the home screen is not the current mode, and by `hs02TuneTick()` at the end of a tune.

#### `void hs02TuneTick(void)`

Advances the tune. Called once per GUI frame by `guiRenderLoop()`. While a note is sounding it silences the buzzer for the last 40 ms of the note (`kGapMs`), so repeated notes are heard apart. When the note has ended it starts the next one with `setBuzzerTone()`, or stops after the last note.

## `source/Core/Threads/UI/drawing/color_160x80/draw_home_gauge.cpp`

Everything listed here is new and compiled only with `HS02_THEME_CARD`, except the three `ui_draw_home_gauge_*` functions, which exist upstream and get a card theme body. Coordinates are screen pixels. Colour arguments are palette indices: `kBg` 0, `kInk` 1, `kAccent` 2, `kMuted` 3.

Each card screen uses four colours: background `0x0042`, white `0xFFFF`, one accent and grey `0x9D37`. The palettes `kCardReadyPalette`, `kCardSolderPalette`, `kCardBoostPalette` and `kCardSleepPalette` differ only in the accent: green `0x262B`, red `0xEA28`, amber `0xF4E1` and blue `0x3DFF`. `cardPalette[4]` is the working copy that `cardPaletteFor()` fills.

The text functions `drawWashText()`, `drawWashTextEmphasis()` and `drawWashUnsigned()` used below are upstream helpers in the same file.

#### `uint16_t hs02CardAccent(SettingsOptions setting, uint16_t ownColour)`

Returns an accent colour as RGB565: `ownColour` if the given colour setting is 0 or out of range, otherwise the fixed colour the setting selects (see "Settings added"). Called by `cardPaletteFor()` for the colour screens and by `guiHandleDraw()` for the menu.

#### `const uint16_t *cardPaletteFor(const uint16_t *modePalette, SettingsOptions setting)`

Copies a screen's palette into the static `cardPalette`, replaces the accent entry with `hs02CardAccent(setting, ...)` and returns a pointer to it. Called by `drawCardIdle()`, `drawCardSoldering()` and `drawCardSleep()` every frame.

#### `void drawCardTemperature(TemperatureType_t temperature)`

Draws the large temperature reading from `Hs02CardDigits::kDigits`, up to three digits without leading zeros, starting at (13, 25), followed by a degree ring. The value is in whole degrees.

#### `void drawCardFrame(const char *label)`

Draws the vertical accent bar at the left edge and the screen's label (for example the translated "READY") at (13, 9) in the accent colour.

#### `void drawCardTarget(const char *caption, TemperatureType_t target)`

Draws the right-hand column: a grey caption at (104, 8) and, under it at (104, 23), the target temperature in whole degrees from `Hs02CardDigits::kMediumDigits` in the accent colour, without leading zeros and followed by a small degree ring. The comment limits the caption to three letters, because the supply voltage drawn by `drawCardVoltage()` shares its row. Both callers pass the translated "set" label (`Hs02WashFont::kLabels.set`).

#### `void drawCardPowerBar(uint32_t x10Watt)`

Draws the bar along the bottom: heater power as a share of the power limit. `x10Watt` is in tenths of a watt. The limit is the `PowerLimit` setting; a setting of 0 is treated as 100 W. The bar is 100 pixels wide, a grey track with the filled part in the accent colour.

#### `uint32_t cardVolts()`

Returns the input voltage rounded to whole volts, from `getInputVoltageX10()` with the `VoltageDiv` setting.

#### `void drawCardValue(uint32_t value, char unit, uint8_t x, uint8_t y)`

Draws a whole number followed by a one-character unit in the small grey text, for example "20V" or "65W".

#### `void drawCardFooter(uint32_t value, char unit)`

Calls `drawCardValue()` at the fixed position bottom right, beside the bar (120, 63). Every card screen calls it with a power in whole watts and the unit "W".

#### `void drawCardVoltage()`

Draws the supply voltage from `cardVolts()`, followed by "V", in the top right corner. The text is right-aligned: it starts at x = 156 − 8 × (number of digits + 1), the glyphs being 8 pixels wide, at y = 8. Called by `drawCardNoTip()`, `drawCardIdle()`, `drawCardSoldering()` and `drawCardSleep()`, so every card screen shows it.

#### `TemperatureType_t cardSteadyTemperature(TemperatureType_t reading)`

Returns the temperature to display, in the same unit as `reading`. A change of one degree is taken over only after it has lasted half a second, so a reading on the edge between two values does not flicker; a change of two degrees or more is taken over at once. Keeps the statics `shown` and `differsSince`, which are shared by the three screens that call it.

#### `uint32_t cardSteadyWatts(uint32_t x10Watt)`

Returns heater power in tenths of a watt for the number beside the bar: a second, slower average on top of `x10WattHistory`. Each call moves the stored value one twelfth of the way to the input; if the previous call was more than half a second ago the stored value is set to the input. Keeps the statics `shownX256` (the value times 256) and `last`, which are shared by the two screens that call it. Called by `drawCardSoldering()` and `drawCardSleep()` every frame.

#### `void drawCardNoTip()`

Draws the home screen shown when no tip is fitted: the label "NO TIP", an animation of a tip sliding into the handle (a loop of 1.6 s: one second of travel, 0.6 s seated), the text "INSERT TIP", the supply voltage top right (`drawCardVoltage()`) and "0W" bottom right (`drawCardFooter()`). Called by `drawCardIdle()`.

#### `void drawCardIdle(TemperatureType_t tipTemp)`

Draws the idle home screen. Sets the palette with the `ColourReady` accent. If `isTipDisconnected()` it calls `drawCardNoTip()`. Otherwise it draws the frame with the "ready" label, the steadied tip temperature, the set temperature (`SolderingTemp`) under the "set" label, an empty power bar, the supply voltage top right and "0W" bottom right. The heater is off on this screen, so the power is drawn as the constant 0.

#### `void drawCardSoldering(bool boostModeOn)`

Draws the soldering screen. Sets the palette with the `ColourBoost` accent while boosting and the `ColourHeating` accent otherwise. It draws the frame with the "boost" label or "HEATING", the steadied tip temperature from `TipThermoModel::getTipInC()` (`getTipInF()` when `TemperatureInF` is set), the `BoostTemp` or `SolderingTemp` setting under the "set" label, the power bar from `x10WattHistory.average()`, the steadied power rounded to whole watts bottom right, and the supply voltage top right.

#### `void drawCardSleep(TemperatureType_t tipTemp)`

Draws the sleep screen. Sets the palette with the `ColourSleep` accent, then draws the frame with the "sleep" label, the steadied tip temperature, "Zzz" at (108, 24) and "IDLE" at (108, 42) in the right-hand column, the supply voltage top right, and bottom right the steadied heater power from `cardSteadyWatts(x10WattHistory.average())` rounded to whole watts. The comment describes that number as what holding the sleep temperature takes. This screen has no power bar.

#### `void ui_draw_home_gauge_idle(TemperatureType_t tipTemp)`

Changed. In the card theme it calls `drawCardIdle(tipTemp)`. Called by `drawHomeScreen()` every frame with the tip temperature in °C.

#### `void ui_draw_home_gauge_soldering(bool boostModeOn)`

Changed. In the card theme it calls `drawCardSoldering(boostModeOn)`. Called by `gui_solderingMode()` in `Soldering.cpp` every frame.

#### `void ui_draw_home_gauge_sleep(TemperatureType_t tipTemp)`

Changed. In the card theme it calls `drawCardSleep(tipTemp)`. Called by `gui_SolderingSleepingMode()` in `Sleep.cpp` every frame with the tip temperature in °C or °F, according to the `TemperatureInF` setting.

## `source/Core/Threads/UI/drawing/color_160x80/draw_temperature_change.cpp`

#### `void ui_draw_temperature_change(void)`

Changed. Draws the temperature adjust screen: a sign, the set temperature and the opposite sign. Upstream chooses which sign is on which side from `ReverseButtonTempChangeEnabled` alone. The fork swaps them when that setting and `TEMP_CHANGE_SWAP_DEFAULT` differ, so with the default 1 of the card theme "−" is on the left and "+" on the right until the setting is switched on. Called by `gui_solderingTempAdjust()` every frame.

## `source/Core/Threads/UI/logic/TemperatureAdjust.cpp`

#### `OperatingMode gui_solderingTempAdjust(const ButtonState buttonIn, guiContext *cxt)`

Changed. The per-frame function of the temperature adjust screen, called by `guiHandleDraw()` in the `TemperatureAdjust` mode. The direction of a button press is now reversed when `ReverseButtonTempChangeEnabled` and `TEMP_CHANGE_SWAP_DEFAULT` differ; upstream reverses it when the setting is on.

With `MODEL_HS02` and `HS02_THEME_CARD` the fork also changed the first frames after the screen opens. Upstream ignores all buttons until they have been released once. Here, if the button state on entry is UP held or DOWN held, that wait is skipped. The comment gives the reason: the screen is opened by holding UP or DOWN, and the same hold carries on changing the temperature. The rest is unchanged.

## `source/Core/Threads/UI/logic/CJC.cpp`

#### `static bool hs02CjcMayRun(const ButtonState buttons, guiContext *cxt, OperatingMode *leaveTo)`

New (`MODEL_HS02`). Decides whether the cold junction calibration may run on this frame. Returns true to carry on; on false the caller returns `*leaveTo`. Called by `performCJCC()` every frame.

- If no tip is fitted or the tip reads 40 °C or more, it sets `*leaveTo` to `StartupWarnings` and returns false. The calibration stays requested for the next start.
- If an earlier frame already allowed the calibration (`scratch_state.state5`), it returns true.
- If the question has not been shown and the MCU temperature is below `kHs02CjcBoardWarmCx10` (35.0 °C), it allows the calibration.
- Otherwise it shows the question "Iron is warm / Calibrate / anyway?" over a hint row reading no / no / yes, and keeps showing it on later frames (`scratch_state.state6`). OK short allows the calibration from the next frame. Any other button sets `*leaveTo` to `StartupWarnings`. With no button it stays in `CJCCalibration`.

The comment gives the reason: this board has no handle sensor, so the upstream test that tip and handle agree cannot be made.

#### `OperatingMode performCJCC(const ButtonState buttons, guiContext *cxt)`

Changed. The per-frame function of the CJC calibration, called by `guiHandleDraw()` in the `CJCCalibration` mode. With `MODEL_HS02` the upstream condition (tip fitted, and tip within 10 °C of the handle reading) is replaced by a call to `hs02CjcMayRun()`. The sampling and the storing of `CalibrationOffset` are unchanged.

## `source/Core/Threads/UI/logic/ShowStartupWarnings.cpp`

#### `static OperatingMode showWarningsStep(const ButtonState buttons, guiContext *cxt)`

Renamed. This is the upstream body of `showWarnings()`: it runs one start-up check per call, selected by `scratch_state.state1`. Its contents are unchanged.

#### `OperatingMode showWarnings(const ButtonState buttons, guiContext *cxt)`

Changed. Now a loop around `showWarningsStep()`. A check with nothing to show goes straight on to the next one in the same frame. The loop ends for the frame when the mode changes, when a check is waiting (the step number did not change) or when a warning was drawn (`scratch_state.state7` is set). Called by `guiHandleDraw()` in the `StartupWarnings` mode.

## `source/Core/Threads/UI/logic/DebugMenu.cpp`

#### `OperatingMode showDebugMenu(const ButtonState buttons, guiContext *cxt)`

Changed. The per-frame function of the debug pages, called by `guiHandleDraw()` in the `DebugMenuReadout` mode. On `LCD_160x80` the page counter now wraps at 19 instead of 17, for the two new pages. In the card theme the page drawn is `(state1 + 16) % 19`, so the three HS-02 pages (items 16, 17 and 18) come first and the standard pages follow.

## `source/Core/Threads/UI/drawing/color_160x80/draw_debug_menu.cpp`

#### `static void drawCurrentSensePage(void)`

New. Draws debug item 17 (the eighteenth page), five rows of the small font with the values from `hs02GetCurrentSense()`: a title, the "off" mean, the "on" mean and the peak as raw ADC counts, and a last row with the heater level in percent and the input voltage in volts with one decimal at which the "on" mean was taken.

#### `static void drawLiveSensorPage(void)`

New. Draws debug item 18, five rows of live values: the tip sensor reading in 12-bit ADC counts (`getTipRawTemp(0) / 8`), the same in microvolts before the offset is removed (`convertTipRawADCTouV(raw, true)`), the `CalibrationOffset` setting in microvolts, the MCU temperature from `getHandleTemperature(0)` in °C with one decimal, and the heater power from `x10WattHistory.average()` in watts with one decimal.

#### `void ui_draw_debug_menu(const uint8_t item_number)`

Changed. Draws one debug page. Items 18 and 17 are passed to the two functions above before the lookup in `DebugMenu[]`, which has no entries for them. Item 0 (build date) now also prints `SmallSymbolCustomBuild` ("Build" and the custom build number) at (0, 48). Called by `showDebugMenu()` every frame.

## Translations and tools

### `source/Core/Inc/Translation.h`

`enum class SettingsItemIndex` gains ten entries before `NUM_ITEMS`: `BuzzerEnabled`, `ColourReady`, `ColourHeating`, `ColourBoost`, `ColourSleep`, `ColourMenu`, `LowVoltageAllowed`, `ButtonLight`, `Calibrate100` and `DebugPages`. The header also declares the new string constants listed under "Generated data".

### `Translations/make_translation.py`

#### `get_constants() -> List[Tuple[str, str]]`

Changed. Returns the string constants shared by all languages, as pairs of symbol name and text. The fork added the symbols listed under "Generated data". The text of `SmallSymbolCustomBuild` is built from `CUSTOM_BUILD`.

#### `get_translation_common_text(small_symbol_conversion_table, large_symbol_conversion_table) -> str`

Changed. Writes each constant as a `const char*` definition followed by a `//` comment with the original text. The fork collapses every run of whitespace in that comment to a single space.

### `Translations/translation_EN.json` and `Translations/translations_definitions.json`

Both files gain one menu option for each of the ten new `SettingsItemIndex` entries. The definitions give each a `maxLen` of 7 and a `maxLen2` of 15; the English file gives the two-line menu name and the description. Only the English translation file was changed.

### `Translations/wash_font.py`

`BASE_CHARS`, the fixed ASCII characters always included in the Wash font subset, gains the letters `AGEILZz`, `NORS` and `H`. The card theme draws the fixed English texts "IDLE", "Zzz", "NO TIP", "INSERT TIP" and "HEATING" with them.

### `tools/hs02-lang-preview/host.cpp`

This host program links the UI code with hardware stubs and dumps every screen as an image. The fork added stubs for the two new BSP functions and extended the debug page loop in `main()` from 17 to 19 pages.

- `void hs02CurrentSenseSample(void)`: empty stub.
- `void hs02GetCurrentSense(uint16_t &offMean, uint16_t &onMean, uint16_t &peak, uint8_t &onPwmPercent, uint16_t &onVoltsX10)`: stub that returns the fixed values 12, 2345, 2600, 100 and 200.

### `tools/hs02-card-demo/`

New folder. It builds a browser demo of the card theme: the firmware's own GUI sources are compiled to WebAssembly and run in a web page against a stand-in for the hardware. The result is the single file `docs/demo/index.html`.

| File | What it is for |
| --- | --- |
| `build.sh` | Compiles the firmware's GUI sources (the display and button drivers, settings, translation, `GUIThread.cpp` and the files under `Core/Threads/UI`) and `demo_host.cpp` to WebAssembly with zig's C++ compiler, with `MODEL_HS02` and `HS02_THEME_CARD` defined, links them to `out/demo.wasm` and runs `pack.py`. It needs Python 3 with the `ziglang` package and one firmware build beforehand (`make model=HS02 firmware-EN` in `source`), which generates `Core/Gen/Translation.EN.cpp`. Run it from the repository root; `PYTHON` selects the interpreter. |
| `demo_host.cpp` | Stands in for the hardware. It stubs the board functions the GUI code calls, records what the LCD driver sends in a panel buffer, keeps a virtual clock that the delay functions advance, and runs a simple tip and heater model. Its header comment states that this model is not the firmware's PID. It imports `host_yield()` and `host_reboot()` from the page and exports the functions listed below. |
| `shim/` | Small replacement headers for FreeRTOS, `cmsis_os.h`, `Software_I2C.h` and `LCD_Port.hpp`, enough for the GUI sources to compile. |
| `pack.py` | Replaces the marker `@@WASM@@` in `page.html` with the base64 text of `demo.wasm` and writes the output file, so the demo is one file that also opens from disk. |
| `page.html` | The page. It draws the panel buffer into a canvas, offers the three buttons (also on the arrow keys and Enter) and controls to pull the tip out, move the iron, mute the sound and restart, sounds the buzzer, and calls `demo_frame()` every 40 ms. When the firmware waits inside a screen, the page ends the run and replays it from a saved copy of the memory with the recorded button states. |

`out/` holds the object files and `demo.wasm`; `.gitignore` excludes it.

Functions `demo_host.cpp` exports to the page:

| Function | What it does |
| --- | --- |
| `void demo_init(uint32_t keepFlash)` | Starts the GUI as `startGUITask()` does up to its loop: loads the settings, prepares the translations, initialises the display and sets the operating mode to `StartupLogo`. With `keepFlash` 0 it first fills the flash page with `0xFF`. |
| `void demo_frame(uint32_t elapsedMs)` | Advances the clock and the tip model by `elapsedMs`, runs `guiRenderLoop()` once and updates the state structure. |
| `void demo_publish(void)` | Updates the state structure without running a frame. |
| `Inputs *demo_inputs(void)` | Returns the structure the page writes: the button bits (bit 0 DOWN, bit 1 UP, bit 2 OK), a "moved" flag, a "tip removed" flag, the heat a joint draws in watts and the supply voltage in tenths of a volt. |
| `State *demo_state(void)` | Returns the structure the page reads: tick, tip temperature in tenths of a °C, target in °C, heater power in tenths of a watt, backlight, status LED, operating mode, rotation, inversion, buzzer pitch in Hz and the button light. |
| `uint16_t *demo_panel(void)` | Returns the panel buffer, `LCD_WIDTH` × `LCD_HEIGHT` pixels as RGB565 in the colours the panel shows. |
| `uint8_t *demo_flash(void)` | Returns the 2048-byte flash page that holds the settings. The page copies it out before a reboot and back in before `demo_init(1)`. |
| `uint32_t demo_flash_size(void)` | Returns the size of that flash page in bytes. |

## Generated data

### `source/Core/Threads/UI/drawing/color_160x80/Hs02CardDigits.hpp`

New file. It holds the digit bitmaps of the card theme in the namespace `Hs02CardDigits`. Its header comment says the file is generated, that the digits are the Inter SemiBold digits of `Hs02InterSemiBoldFont.hpp` after correcting two slipped rows in "7" and one in "5", and that the licence is in `licenses/Inter-OFL-1.1.txt`. The comment does not name the generator.

The format is the one `Display::drawBitmapColor()` takes: one byte per column per 8-row strip, least significant bit at the top.

| Table | Size | Used by |
| --- | --- | --- |
| `kDigits[10][kDigitDataBytes]` | 26 columns × 30 rows (`kDigitWidth`, `kDigitHeight`), four strips, 104 bytes per digit. Enlarged 3:2 from the original with edge smoothing. | `drawCardTemperature()` |
| `kMediumDigits[10][kMediumDataBytes]` | 17 columns × 20 rows (`kMediumWidth`, `kMediumHeight`), three strips, 51 bytes per digit. The original size. | `drawCardTarget()` |

### Translation symbols added in `Translations/make_translation.py`

`get_constants()` defines these symbols once for all languages. They are English text and are not translated. A `Small` prefix means the small font and a `Large` prefix the large font. `Translation.h` declares each as `extern const char *`.

| Group | Symbols | Used for |
| --- | --- | --- |
| Current sense debug page | `SmallSymbolCurrentSense`, `SmallSymbolCurrentOff`, `SmallSymbolCurrentOn`, `SmallSymbolCurrentPeak`, `SmallSymbolCurrentPwm` | Row labels of debug item 17. |
| Live sensor debug page | `SmallSymbolLiveRaw`, `SmallSymbolLiveMicrovolts`, `SmallSymbolLiveOffset`, `SmallSymbolLiveMcu`, `SmallSymbolLivePower` | Row labels of debug item 18. `setCalibrate100()` also prints `SmallSymbolLiveMicrovolts` ("uV") after its reading. |
| Build number | `SmallSymbolCustomBuild` | "Build" and the value of `CUSTOM_BUILD`, on debug item 0. |
| Hint row | `SmallSymbolHintOpen`, `SmallSymbolHintEdit`, `SmallSymbolHintDone`, `SmallSymbolHintRun`, `SmallSymbolHintPrev`, `SmallSymbolHintNext`, `SmallSymbolHintMore`, `SmallSymbolHintLess`, `SmallSymbolHintYes`, `SmallSymbolHintNo` | Button labels of the card menu, of the confirmation screens and of the question in `setCalibrate100()`: "open", "edit", "save", "set", "prev", "next", "+", "-", "yes", "no". |
| Confirmation texts | `SmallCardConfirmCalibrate1` to `4`, `SmallCardConfirmReset1` to `3` | The short prompts of `userConfirmation()` in the card theme. |
| CJC question | `SmallHs02CjcWarm1` to `3` | The question shown by `hs02CjcMayRun()`. |
| 100 °C calibration | `SmallCal100Ask1` to `3`, `SmallCal100Busy`, `SmallCal100Saved`, `SmallCal100Bad1` and `2` | The question, the "Measuring" text and the two result texts shown by `setCalibrate100()`. |
| Off | `LargeCardWordOff` | The word that replaces the "unavailable" icon in the card menu. |
| Colour names | `LargeCardColour0` to `LargeCardColour8` | "Auto" and the eight colour names shown by `displayCardColour()`. |
| Whole-word setting values | `LargeCardWordSettingRightChar`, `...LeftChar`, `...AutoChar`, `...SlowChar`, `...MediumChar`, `...FastChar`, `...StartSolderingChar`, `...StartSleepChar`, `...StartSleepOffChar`, `...LockBoostChar`, `...LockFullChar` | The words `SETTING_VALUE_TEXT()` selects in place of the translated one-letter values. Each name is `LargeCardWord` followed by the name of the `TranslationIndexTable` field it replaces. |
