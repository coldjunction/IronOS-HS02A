# HS-02A custom IronOS: changes and known issues

Based on the HS-02 port of IronOS (bugparty/IronOS, branch `dev`, commit 93da587). IronOS is
GPLv3; this is a modified version and is published under the same licence.

## Build 35

- **Less overshoot:** the PID integral now only accumulates while the tip approaches the set
  point more slowly than 8 °C per second (`TIP_PID_INTEGRAL_MAX_RATE`). Measured before the
  change: 4 °C over at 320 °C, 6 °C over on a boost to 450 °C. The simulator puts both near
  2–3 °C with the change, at the cost of about 0.4 s on the last 10 °C. Not yet measured on
  hardware.
- **Steadier numbers on the card screens:** a one-degree change in the reading is only shown
  once it has lasted half a second (two degrees or more show at once), and the watt number
  beside the bar has a second, half-second average.
- **Supply voltage is shown while heating**, under the target temperature.
- **Sleep and boost temperature in the card menu:** a press changes the value by 1 degree, a
  held button jumps to the next multiple of 10. Temperature values in the menu are followed by
  C or F.
- **Second tune:** keep Up and Down held for five seconds on the ready screen.
- **Fixed: boost "Off" did not survive a change of temperature unit.** Switching to °F turned
  it into 30 °F (shown as on); switching to °C turned it into the maximum temperature. This was
  in the port's code. "Off" now stays off.
- **Fixed: the ready and heating screens showed the tip in °C beside a target in °F** when the
  unit is Fahrenheit.
- **Fixed: Down on the ready screen skipped a step** when the target was between two steps
  (321 went to 315; it now goes to 320).
- **Shorter start-up:** display initialisation waits cut from 450 ms to 250 ms (the ST7735S
  data sheet values), and start-up checks with nothing to show no longer take a frame each.

## Build 34

- **"Button light" setting** (card theme, user interface menu): switches the white light behind
  the Down and Up buttons on or off. Default on. The light is one on/off output, so there is no
  dimming or colour. The light in the OK button is the heating indicator and is not affected.

## Build 33 (fixes from a code review)

- **Sleep / boost temperature, Down button:** a value that sits between the steps (for example a
  sleep temperature of 170 °C shown as 330 °F after changing the unit) jumped to an unrelated
  value. Down now goes to the nearest step below it.
- **"Swap A B keys" removed from the card theme menu:** the card menu has a fixed button layout
  (Down, Up, OK, as its hint row shows) and never read that setting.
- **"Swap + - keys" now also swaps the quick temperature steps on the home screen**, not only
  the temperature screen.
- **CJC calibration asks before calibrating a warm iron.** The HS-02 has no handle sensor, and
  until the offset is right a tip of up to about 50–60 °C reads as 25 °C, so a warm tip could be
  calibrated as "cold". If the MCU reads 35 °C or more at start-up the iron now shows
  "Iron is warm / Calibrate anyway?" (Down or Up = no, OK = yes). "No" keeps the calibration
  requested for the next start. A tip that reads 40 °C or more is skipped as before.
- Comment on the power display filter corrected (it still stops up to 0.2 W short of a steady
  value; the card theme rounds to the nearest watt).

## Behaviour changes to know about when updating

- **Cell cut-off voltage range is 2.5–3.5 V per cell** (the port allowed 2.4–3.8 V). A stored
  value outside the new range is reset to 3.3 V at the first start.
- **No heat-up ramp and no soft start.** Heater power and boost targets are applied at once.
- **Default power limit 100 W.**
- **Input voltages under 9 V are refused** unless "Allow under 9V" is switched on.

## Known issues

- **Units with a factory tip calibration are untested.** The iron this was developed on has
  none (the debug page shows "Custom Curve"), so it uses the fallback curve, which is what
  the temperature work here changed. The factory-calibration branch of
  `source/Core/BSP/Fnirsi/ThermoModel.cpp` is the port's original code. A review found that it
  subtracts a fixed 25 °C where its comment says "handle temperature", and clamps the result at
  zero, so such a unit may show the MCU temperature for a cold tip and read some degrees high
  when warm. It may also apply the calibration offset differently from the fallback curve. Not
  confirmed on hardware.
- **A cold tip can read 25 °C even after a CJC calibration.** Seen on the development iron: the
  reading stays at 25 °C until the tip is well above hand temperature, and for the first moment
  of a heat-up. The likely cause is the tip amplifier's output sitting at its lower limit for a
  cold tip on that unit, which firmware cannot see below; not confirmed. Soldering temperatures
  are not affected.
- **The CJC "iron is warm" check is weak.** It relies on the MCU's own temperature and did not
  trigger in a test straight after heating. Only calibrate an iron you know is cold.
- **Readings below about 223 °C depend on the CJC calibration.** Run "Calibrate CJC" once with
  the iron cold (unplugged for 30 minutes or more).
- **No tip-resistance or short-circuit detection.** A missing tip is detected.
- **PID gains were tuned on one iron and in a simulator.** Overshoot above 400 °C and with
  other tip shapes is not measured.
- **Temperature accuracy at soldering temperature is not verified** against a reference
  (solder melting point or a tip thermometer).
