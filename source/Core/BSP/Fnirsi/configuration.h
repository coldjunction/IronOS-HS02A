#pragma once
#include <stdint.h>
/**
 * Configuration.h
 * Deep work in progress for Fnirsi HS-02A and HS-02B
 *
 */

#if defined(MODEL_HS02) == 0
#error "No model defined!"
#endif

//===========================================================================
//============================= Default Settings ============================
//===========================================================================
/**
 * Default soldering temp is 320.0 C
 * Temperature the iron sleeps at - default 150.0 C
 */

#define SLEEP_TEMP         150 // Default sleep temperature
#define BOOST_TEMP         420 // Default boost temp.
#define BOOST_MODE_ENABLED 1   // 0: Disable 1: Enable

/**
 * Blink the temperature on the cooling screen when its > 50C
 */
#define COOLING_TEMP_BLINK 0 // 0: Disable 1: Enable

/**
 * How many seconds/minutes we wait until going to sleep/shutdown.
 * Values -> SLEEP_TIME * 10; i.e. 5*10 = 50 Seconds!
 */
#define SLEEP_TIME    5  // x10 Seconds
#define SHUTDOWN_TIME 10 // Minutes

/**
 * Auto start off for safety.
 * Pissible values are:
 *  0 - none
 *  1 - Soldering Temperature
 *  2 - Sleep Temperature
 *  3 - Sleep Off Temperature
 */
#define AUTO_START_MODE 0 // Default to none

/**
 * Locking Mode
 * When in soldering mode a long press on both keys toggle the lock of the buttons
 * Possible values are:
 *  0 - Desactivated
 *  1 - Lock except boost
 *  2 - Full lock
 */
#define LOCKING_MODE 0 // Default to desactivated for safety

/**
 * OLED Orientation
 *
 */
#define ORIENTATION_MODE           2 // 0: Right 1:Left 2:Automatic - Default Automatic
#define MAX_ORIENTATION_MODE       2 // Up to auto
#define REVERSE_BUTTON_TEMP_CHANGE 0 // 0:Default 1:Reverse - Reverse the plus and minus button assigment for temperature change

/**
 * LCD Brightness
 * Range 0 - 127; however, thee is no noticable increase over 100.
 */
#define MIN_BRIGHTNESS     1    // Min LCD brightness selectable
#define MAX_BRIGHTNESS     105  // Max LCD brightness selectable

#define BRIGHTNESS_STEP    13   // LCD brightness increment
#define DEFAULT_BRIGHTNESS 105  // default LCD brightness

// Screen and menu theme. HS02_THEME_CARD selects the card theme: card-style home screens, a
// three-row settings menu with a button hint row, no boot logo, and the buzzer switched on.
// The Makefile defines it for every HS02 build; this fork builds and tests nothing else. The
// port's own "wash" layout is still in the source behind #ifndef HS02_THEME_CARD, untested here.
// The define has to come from the compiler command line, not from this file: some files test
// it before they include this header.

// Card theme: on the temperature screen + and - are the other way round from the port's
// default (asked for by the first user of the theme). "Swap + - keys" in the menu still swaps
// them back.
#ifdef HS02_THEME_CARD
#define TEMP_CHANGE_SWAP_DEFAULT 1
#endif

/**
 * Buzzer: one short beep when the tip reaches the set temperature (Soldering.cpp).
 * 0 is off (the port's default: "annoying"); the card theme uses a quiet setting.
 */
#ifdef HS02_THEME_CARD
#define BUZZER_VOLUME 6 // Range: 0 - 19
#else
#define BUZZER_VOLUME 0 // Range: 0 - 19
#endif

/**
 * Button backlight <- ->
 * 
 */
#define BUTTON_BACKLIGHT 1 // 0: Disabled, 1: Enabled at boot

/**
 * Temp change settings
 */
#define TEMP_CHANGE_SHORT_STEP     1  // Default temp change short step +1
#define TEMP_CHANGE_LONG_STEP      10 // Default temp change long step +10
#define TEMP_CHANGE_SHORT_STEP_MAX 50 // Temp change short step MAX value
#define TEMP_CHANGE_LONG_STEP_MAX  90 // Temp change long step MAX value

/* Power pulse for keeping power banks awake*/
#define POWER_PULSE_INCREMENT    1
#define POWER_PULSE_MAX          100 // x10 max watts
#define POWER_PULSE_WAIT_MAX     9   // 9*2.5s = 22.5 seconds
#define POWER_PULSE_DURATION_MAX 9   // 9*250ms = 2.25 seconds

#define POWER_PULSE_DEFAULT 5
#define POWER_PULSE_WAIT_DEFAULT     4 // Default rate of the power pulse: 4*2500 = 10000 ms = 10 s
#define POWER_PULSE_DURATION_DEFAULT 1 // Default duration of the power pulse: 1*250 = 250 ms

/**
 * OLED Orientation Sensitivity on Automatic mode!
 * Motion Sensitivity <0=Off 1=Least Sensitive 9=Most Sensitive>
 */
#define SENSITIVITY 7 // Default 7

/**
 * Detailed soldering screen
 * Detailed idle screen (off for first time users)
 */
#define DETAILED_SOLDERING 1 // 0: Disable 1: Enable - Default 0
#define DETAILED_IDLE      1 // 0: Disable 1: Enable - Default 0

#define THERMAL_RUNAWAY_TIME_SEC 5
#define THERMAL_RUNAWAY_TEMP_C   3

#define CUT_OUT_SETTING          0  // default to no cut-off voltage
// With "Power source" on DC the stock IronOS floor is 9 V, which refuses a 5 V supply outright.
// The HS-02 heats (slowly: about 10 W into this tip) from 5 V, and FNIRSI's own firmware has a
// 5 V mode, so with the "Allow under 9V" setting switched on the floor is 4.0 V (it is 9 V when
// the setting is off, the default). Below roughly 3.6 V the 3.3 V regulator drops out anyway.
#define DC_MINIMUM_VOLTAGE_X10   40
#define RECOM_VOL_CELL           33 // Minimum voltage per cell (Recommended 3.3V (33))
#define TEMPERATURE_INF          0  // default to 0
#define DESCRIPTION_SCROLL_SPEED 0  // 0: Slow 1: Fast - default to slow
#define ANIMATION_LOOP           1  // 0: off 1: on
#define ANIMATION_SPEED          settingOffSpeed_t::FAST

#define ADC_MAX_READING  (4096 * 8)  // oversample 12bit adc 8 times
#define ADC_VDD_MV        3300       // ADC max reading millivolts

// #define POW_PD 0
#define POW_PD_EXT 3 // TODO: implement
#define POW_DC

// Deriving the Voltage div:
// Vin_max = (3.3*(r1+r2))/(r2)
// vdiv = (32768*4)/(vin_max*10)

// #if defined(MODEL_HS02A) + defined(MODEL_HS02B) > 1
// #error "Multiple models defined!"
// #endif

#define I2C_SOFT_BUS_1
#define ACCEL_I2CBB1
#define ACCEL_KXTJ3

#define MIN_CALIBRATION_OFFSET 0 // Min value for calibration (was 100; this board's real value is about 0)
#define SOLDERING_TEMP         320 // Default soldering temp is 320.0 °C
#define MAX_TEMP_C             450 // Max soldering temp selectable °C
#define MAX_TEMP_F             850 // Max soldering temp selectable °F
#define MIN_TEMP_C             10  // Min soldering temp selectable °C
#define MIN_TEMP_F             50  // Min soldering temp selectable °F
#define MIN_BOOST_TEMP_C       250 // The min settable temp for boost mode °C
#define MIN_BOOST_TEMP_F       480 // The min settable temp for boost mode °F

#define FILTER_DISPLAYED_TIP_TEMP   4


#define VOLTAGE_DIV        540 // 540 - Default divider from schematic
// Default tip amplifier reading at zero temperature difference, in uV. The port used 900, the
// default IronOS has for other irons. On the HS-02A it measured as 0 (a cold tip reads 0 ADC
// counts), which is also where the stock firmware's curve starts; with 900, everything from
// room temperature to about 55 C read as 25 C. A unit whose amplifier does have a positive
// offset reads a cold tip too warm until "Calibrate CJC" has been run, which is the safer way
// round to be wrong.
#define CALIBRATION_OFFSET 0
#define POWER_LIMIT        100 // 100 watts default limit (was 60); needs a supply that can deliver it

#define USB_PD_VMAX        20 // Maximum voltage for PD to negotiate
#define MAX_POWER_LIMIT    100
#define POWER_LIMIT_STEPS  5
#define OP_AMP_GAIN_STAGE  57

#define HARDWARE_MAX_WATTAGE_X10 1000    // 100W

// No soft start: heater power goes to whatever the controller asks for at once. (For a while
// this build ramped it over half a second with SLEW_LIMIT 100, in case a charger cut out on a
// 0 -> 100 W step; with a charger that can supply the power limit that only delays heating.)
// #define SLEW_LIMIT 100

// How quickly the power shown on screen follows the real heater power: weight (of 256) given
// to each new 50 ms sample. The shared default of 24 takes about 1.5 s to catch up. The filter
// (expMovingAverage.h) is integer maths that rounds down, so a value approached from below
// stops short of it by up to (256 - weight) / weight tenths of a watt: 0.9 W with 24 (a steady
// 100 W showed as 99 W), 0.2 W with 96. It still stops short; the card theme rounds to the
// nearest watt, which hides what is left.
#define WATT_HISTORY_WEIGHT 96

// #define OLED_I2CBB1    0
// #define TIPTYPE_T12    0 // Can manually pick a T12 tip

/// Active Disturbance Rejection Control - first order
// https://nl.mathworks.com/help/slcontrol/ug/active-disturbance-rejection-control.html
// LIMITATION: estimator coefficients (A, B) are precalculated for FIXED sample rate
//#define TIP_CONTROL_ARDC1
#define TIP_ARDC1_Kp            1.5
#define TIP_ARDC1_Kd            0.5
#define TIP_ARDC1_b0            0.085
#define TIP_ARDC1_A             {6.7996e-02, 1.8013e-02, -9.0063e-01, 9.6863e-01}
#define TIP_ARDC1_B             {TIP_ARDC1_b0*1.2609e-03/0.07, 9.3200e-01, TIP_ARDC1_b0*-2.1961e-03/0.07, 9.0063e-01}

/// Use PID Control
#define TIP_CONTROL_PID

// HS-02A PID gains ("hybrid": a proportional gain close to the stock firmware's, plus the
// integral term stock lacks, working on the 0.1 C reading).
//
// What stock does, read from APP_HS_02A_V2.2.0_260807.bin (control function at
// 0x08009fb0, gains in .data at 0x20000018). Every 10 ms, on a 40-sample average:
//   duty % = 0.81 * e + 2.38 * (e - e1) + 0.28 * (e - 2*e1 + e2)      e in degrees C
// The result is assigned, not accumulated: there is NO integral term. Stock settles
// below the set point by (holding duty % / 0.81) degrees C and hides it by drawing the
// reading 10 C closer to the set point. In this PID's units (output in x10 W) the stock
// gains would be KP 13, KI 0, KD 380. The port this fork is based on used KP 40, KI 700,
// KD 8000 with an integral limit scale of 30, on whole degrees.
//
// The hybrid was first chosen in simulation only (KP 20 / KI 1000 / KD 2000), then corrected
// after a real HS-02A overshot a 320 C set point to about 345 C on heat-up with a 100 W limit.
// That overshoot is reproduced by a tip model whose sensor lags its heater by 0.6-1.0 s, and
// the values below were chosen on that family of models (tools/hs02-pid-sim):
//   KP 20     1.5x stock.
//   KI 700    removes the droop stock leaves.
//   KD 12000  leads the lagging sensor so power is cut before the reading arrives; 20000
//             and above oscillated in the model.
//   INTEGRAL_ZONE 30  the integral only builds within 30 C of the set point, so it cannot
//             load up during the approach.
//   INTEGRAL_MAX_RATE 8  ...and only while the tip approaches more slowly than 8 C per
//             second (see PIDThread.cpp). Measured without it: 4 C over at 320 C, 6 C over
//             on a boost to 450 C. The simulator puts both near 2-3 C with it; not yet
//             measured on the iron.
//   INTEGRAL_LIMIT_SCALE 60  lets the integral reach 60 % of the power limit on a heavy
//             joint.
//   INPUT_SCALE 10  the PID works on the 0.1 C reading.
#define TIP_PID_KP                   20
#define TIP_PID_KI                   700
#define TIP_PID_KD                   12000
#define TIP_PID_INTEGRAL_LIMIT_SCALE 60
#define TIP_PID_INTEGRAL_ZONE        30
#define TIP_PID_INTEGRAL_MAX_RATE    8
#define TIP_PID_INPUT_SCALE          10

/// Default Control if no other controller is defined
#define TIP_THERMAL_MASS        0    // Not used for PID
#define TIP_THERMAL_INERTIA     0    // Not used for PID
#define TIP_RESISTANCE          25   // C245 is around 2.5R

#define FLASH_LOGOADDR      (0x08000000 + (122 * 1024)) // 2KB up to address 0x0801F000 (124 * 1024)
#define SETTINGS_START_PAGE (0x08000000 + (120 * 1024))

#define LCD_160x80          1
