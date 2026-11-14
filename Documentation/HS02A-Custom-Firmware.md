# HS-02A custom firmware: user guide and feature list

This fork changes the [bugparty/IronOS](https://github.com/bugparty/IronOS) port for the
FNIRSI HS-02A in three areas: temperature control (PID), the temperature reading at the cold
end, and the user interface (the "card theme"). This page explains how to install it and
describes every feature of the card theme build, including the ones that come from IronOS and
the port.

- For what each function in the code does, see
  [HS02A-Function-Reference.md](HS02A-Function-Reference.md).
- For the list of changes and known issues, see [HS02A-CHANGELOG.md](../HS02A-CHANGELOG.md).

**Status:** developed and tested on one HS-02A. Read the known issues in the changelog before
relying on it. You flash this at your own risk.

**Credit:** the firmware is [IronOS](https://github.com/Ralim/IronOS) by Ben V. Brown (Ralim)
and contributors. The HS-02 port, which did the hard part of making IronOS run on this iron
(display driver, power, buttons, buzzer, the colour home screen), is by PanKleszcz, OK2CM and
bugparty. This fork is a set of changes on top of their work.

## Contents

1. [Which file to flash](#which-file-to-flash)
2. [Installing](#installing)
3. [Buttons](#buttons)
4. [Screens](#screens)
5. [Heating features](#heating-features)
6. [Settings menu, entry by entry](#settings-menu-entry-by-entry)
7. [Calibrating the cold end (CJC)](#calibrating-the-cold-end-cjc)
8. [Debug pages](#debug-pages)
9. [Tunes](#tunes)
10. [Power supplies](#power-supplies)
11. [Building from source](#building-from-source)
12. [How the PID was tuned](#how-the-pid-was-tuned)

## Which file to flash

Each release has four firmware files. They differ only in the user interface and the PID gains.

| File name contains | Interface | PID |
|---|---|---|
| `hybrid-PID_card-theme` | Card theme (everything on this page) | Hybrid (this fork's tuning) |
| `hybrid-PID` | The port's original interface | Hybrid |
| `factory-equivalent-PID` | The port's original interface | Gains equivalent to FNIRSI's stock control law |
| `original-port-PID` | The port's original interface | The port's own gains |

The settings added by this fork (colours, buzzer, button light, low-voltage switch, debug pages
entry) only have menu entries in the card theme file. The temperature reading fixes, the
100 W default power limit and the removed heat-up ramp are in all four.

## Installing

1. Unplug the iron.
2. Hold the **OK** button and plug the iron into a computer with a USB-C cable. A drive named
   `BOOTLOADER` appears.
3. Copy the firmware `.bin` file onto that drive. The iron restarts with the new firmware.

To go back to the stock firmware, do the same with FNIRSI's own firmware file from their
website. The stock firmware is not included in this repository.

Settings are kept when you update. A setting whose stored value is outside the range the new
build allows is reset to its default.

## Buttons

The three buttons, left to right as you hold the iron with the screen facing you: **Down**,
**Up**, **OK**. Down is "less" or "next", Up is "more" or "previous".

### Ready screen

| Action | Result |
|---|---|
| Press Up / Down | Target temperature up / down to the next step (5 degrees unless "Temp change short" is larger). Saved two seconds later. |
| Press OK | Start heating |
| Hold Up | Open the settings menu |
| Hold Down | Open the temperature screen |
| Hold Up and Down together | Play a tune (see [Tunes](#tunes)) |

### Heating screen

| Action | Result |
|---|---|
| Press any button | Open the temperature screen |
| Hold Down | Boost: heat to the boost temperature for as long as the button is held |
| Hold Up, or hold OK | Stop heating and go back to the ready screen |
| Hold Up and Down together | Lock or unlock the buttons, if "Allow locking buttons" is on |

### Temperature screen

| Action | Result |
|---|---|
| Press Down / Up | Target down / up by "Temp change short" |
| Hold Down / Up | Target down / up by "Temp change long", repeating |
| Press or hold OK | Back |

The screen also closes by itself after three seconds without a button press.

### Settings menu

The bottom row of the screen shows what each button does at that moment, in button order.

| Action | Browsing | Editing a value |
|---|---|---|
| Down | Next entry | Less |
| Up | Previous entry | More |
| OK | Open the group, start editing, or run the action | Save |
| Hold OK | Back one level (from the top level: leave the menu) | |

Holding Down or Up repeats, faster the longer it is held. Sleep temperature and boost
temperature change by 1 degree per press and jump to the next multiple of 10 while held.

## Screens

- **Ready:** tip temperature (large), the target ("SET"), and the supply voltage.
- **Heating:** tip temperature, the target, the supply voltage, and along the bottom a bar and
  a number for heater power. The bar is the share of the power limit in use.
- **Boost:** the heating screen in its own colour, with the boost temperature as target.
- **Sleep:** tip temperature while the iron idles at the sleep temperature.
- **No tip:** an animation of a tip sliding into the handle. The menu stays reachable.

Temperatures are in the unit chosen under "Temperature unit". Each screen has its own accent
colour (green, red, amber, blue), which can be changed per screen in the menu.

To keep the numbers readable, a one-degree change in the reading is only shown once it has
lasted half a second (larger changes show at once), and the watt number is averaged over about
half a second. The power bar is not averaged.

## Heating features

- **Fast heat-up.** Full power from the first moment: no soft start and no temperature ramp.
  Power is limited by the "Power limit" setting (default and maximum 100 W) and by what the
  supply can deliver at its voltage.
- **Boost.** Hold Down while heating to raise the target to the boost temperature
  (default 420 °C) for as long as the button is held. Useful for one large joint. Set the boost
  temperature to "Off" to disable it.
- **Sleep.** The iron has a motion sensor. When it has not moved for the sleep timeout
  (default 50 seconds) it drops to the sleep temperature (default 150 °C) and returns to the
  target when moved or when a button is pressed.
- **Shutdown.** After the shutdown timeout without movement (default 10 minutes) heating stops.
- **Start-up behaviour.** The iron can start heating as soon as it is plugged in, start in
  sleep, or wait at room temperature until moved, instead of waiting on the ready screen.
- **Button lock.** With "Allow locking buttons" on, holding Up and Down together while heating
  locks the buttons, so the target cannot be changed by accident. The same hold unlocks them.
- **Power pulse.** Some power banks switch off when the load is small. The iron can draw a
  short pulse of power at intervals to keep them on.
- **Buzzer.** A beep when the tip reaches the target temperature.
- **Status light.** The light in the OK button blinks while heating, is steady when hot, and
  flashes slowly while the tip is still hot after heating has stopped.

Not present: tip resistance detection, short-circuit detection, Bluetooth, soldering profiles,
hall sensor support.

## Settings menu, entry by entry

"New" marks entries added by this fork. Everything else comes from IronOS and the port.

### Power settings

| Entry | What it does |
|---|---|
| Power source | "DC" for a USB-C charger or a fixed supply, or a cell count (3S to 6S) when running from a lithium battery, which enables the cell cut-off below. |
| Minimum voltage | Battery only: the voltage per cell at which heating stops, 2.5 to 3.5 V (default 3.3 V). |
| Allow under 9V (new) | Off: a supply below 9 V gives a low-voltage warning. On: supplies down to 4 V are accepted, for example a 5 V USB port. Heating is slow at low voltage. |

### Soldering settings

| Entry | What it does |
|---|---|
| Boost temp | The boost target, or "Off". Press for 1 degree, hold for steps of 10. |
| Start-up behavior | What the iron does when plugged in: wait on the ready screen, heat at once, or start in sleep with or without heating. |
| Temp change short | Step of a button press on the temperature screen. |
| Temp change long | Step of a held button on the temperature screen. |
| Allow locking buttons | Off, boost only (locked buttons still allow boost), or full. |

### Sleep mode

| Entry | What it does |
|---|---|
| Motion sensitivity | How easily movement wakes the iron, 1 to 9. "Off" disables sleep and hides the two entries below. |
| Sleep temp | Tip temperature while sleeping. Press for 1 degree, hold for steps of 10. |
| Sleep timeout | Time without movement before sleep. |
| Shutdown timeout | Time without movement before heating stops. |

### User interface

| Entry | What it does |
|---|---|
| Temperature unit | °C or °F. Stored temperatures are converted and rounded down to a multiple of 10, so 170 °C becomes 330 °F. |
| Display orientation | Right-handed, left-handed, or automatic from the motion sensor. |
| Cooldown flashing | From the port. Has no effect on the colour screens. |
| Scrolling speed | Speed of long texts that scroll across the screen. |
| Swap + - keys | Swaps Down and Up for temperature changes (ready screen and temperature screen). |
| Anim. speed, Anim. loop | From IronOS, for menu icon animations. The HS-02 menu has no icons, so they have no effect. |
| Screen brightness | Backlight level. |
| Invert screen | Inverts the display colours. |
| Boot logo duration | Has no effect in the card theme, which shows no boot logo. |
| Detailed idle screen, Detailed solder screen | From IronOS. The HS-02 colour screens do not have a detailed variant, so they have no effect on the ready screen. |
| Buzzer beep (new) | The beep at target temperature, and the tunes. |
| Button light (new) | The white light behind the Down and Up buttons, on or off. |
| Ready / Heating / Boost / Sleep / Menu colour (new) | Accent colour of that screen: Auto (the theme's own colour) or blue, green, red, orange, yellow, purple, pink, white. |

"Swap A B keys" from the port is not in the card theme menu, because the card menu has a
fixed button layout.

### Advanced settings

| Entry | What it does |
|---|---|
| Power limit | Maximum average heater power, up to 100 W (default 100 W). Lower it for a supply that cannot deliver 100 W at its voltage. |
| Calibrate CJC at next boot | See [Calibrating the cold end](#calibrating-the-cold-end-cjc). |
| Calibrate input voltage | Adjusts the supply voltage reading: Down and Up change the shown voltage until it matches a meter, OK saves. |
| Power pulse | Power of the keep-awake pulse for power banks, or "Off". The two entries below appear when it is on. |
| Power pulse delay | Time between pulses, in steps of 2.5 s. |
| Power pulse duration | Length of a pulse, in steps of 250 ms. |
| Restore default settings | Resets every setting, including the CJC calibration. |
| Debug pages (new) | Opens the [debug pages](#debug-pages). |

## Calibrating the cold end (CJC)

The HS-02A has no temperature sensor in the handle. The firmware assumes the cold end of the
tip is at 25 °C and needs to know what the tip amplifier reads when the tip is at that
temperature. Until it does, low temperatures (below about 50–60 °C) can all read as 25 °C.

1. Leave the iron unplugged for at least 30 minutes with the tip fitted.
2. Plug it in, open **Advanced settings → Calibrate CJC at next boot** and confirm.
3. Unplug and plug in again. The calibration runs at start-up and takes about two seconds.

If the board reads warm at start-up the iron asks "Iron is warm / Calibrate anyway?". Answer
no (Down or Up) unless you know the iron is cold. A tip that reads 40 °C or more is skipped
without a message; the calibration stays requested for the next start.

The calibration only affects readings below about 223 °C. Readings above that come from fixed
measured points.

On some irons a cold tip still reads 25 °C after calibrating. See the known issues in the
changelog.

## Debug pages

Open them from **Advanced settings → Debug pages**. Down goes to the next page; Up leaves. The first three are specific to the HS-02A:

- **Tip Cal:** whether a factory tip calibration was found in the stock firmware's settings
  page ("In Use") or not ("Custom Curve"), and its three values.
- **I sense raw:** raw readings of the heater current shunt with the heater off and on. This
  input is read for information only; the firmware does not act on it.
- **Live sensors:** `raw` (tip sensor, ADC counts), `uV` (the same in microvolts before the
  offset is removed), `ofs` (the stored CJC offset in microvolts), `mcu` (the microcontroller's
  own temperature) and `pwr` (heater power).

The standard IronOS pages follow. The first of them shows the build number of this fork.

## Tunes

On the ready screen, hold Up and Down together: the buzzer plays the opening of Beethoven's
"Ode to Joy". Keep holding for five seconds and it switches to the opening of Grieg's "In the
Hall of the Mountain King". Any button stops a tune. Both are in the public domain. The tunes
are silent when "Buzzer beep" is off.

To change a tune, edit the note tables in
`source/Core/Threads/UI/logic/utils/Hs02Tune.cpp`: each entry is a pitch in Hz and a length in
milliseconds.

## Power supplies

The iron asks a USB-C PD charger for 20 V. Heater power depends on the voltage: the firmware
works out what the tip can draw at the present voltage and never asks for more than the power
limit. For the full 100 W use a charger and cable rated for 100 W or more at 20 V.

Below 9 V the iron shows a low-voltage warning unless "Allow under 9V" is on. At 5 V the tip
draws roughly 10 W, so heating takes minutes.

## Building from source

You need the Arm GNU Toolchain (`arm-none-eabi-gcc`, 14.2 was used), `make`, and Python 3 with
the `bdflib` package.

```bash
cd source
make -j8 model=HS02 firmware-EN GLOBAL_DEFINES="-DHS02_THEME_CARD"
```

The result is `source/Hexfile/HS02_EN_firmware.bin`. Leave `GLOBAL_DEFINES` empty for the
port's original interface with the hybrid PID, or use `-DHS02_FACTORY_PID` or
`-DHS02_ORIGINAL_PID` for the other two PID profiles. Delete `source/Objects`,
`source/Hexfile` and `source/Core/Gen` when switching between variants. The build number shown
on the debug page is `CUSTOM_BUILD` in `Translations/make_translation.py`.

Only the English translation has the menu texts added by this fork.

## How the PID was tuned

The stock firmware's control law was read from its binary: proportional and derivative terms
only, no integral, which leaves a temperature droop under load. The port's own gains hold
temperature under load but overshoot on heat-up. The hybrid profile was tuned in a simulator
(`tools/hs02-pid-sim/`) of the tip (heater mass, tip mass, a lagging sensor) and the
firmware's integer PID, then corrected against overshoot measured on the real iron. It feeds
the PID tenths of a degree, lets the integral act only within 30 °C of the target and only
while the tip is approaching slowly, and uses a stronger derivative term.

The simulator's tip model is assumed, not measured, and it was calibrated against one iron and
one tip shape. Treat its numbers as estimates.
