# HS-02A custom IronOS: changes and known issues

Based on the HS-02 port of IronOS (bugparty/IronOS, branch `dev`, commit 93da587). IronOS is
GPLv3; this is a modified version and is published under the same licence.

## Build 43

- **Ready screen: holding Down changes the target on the spot**, like the heating screen. The
  full-screen temperature screen is no longer opened from anywhere.
- **Button light brightness.** The "Button light" setting is now Off or 1 (dim) to 10 (full)
  instead of on / off. The light is dimmed in software at 100 Hz. **After updating, the setting
  reads 1 (dim) if it was on before; set it to the brightness you want.**

## Build 42

- **"Calibrate at 100C" stores whatever the iron reads.** The reading used to be refused unless
  it fell in a window around the expected value. What an iron reads at 100 °C differs too much
  between irons for that, and measuring it is the point. Only a reading with no signal at all,
  or one too large to store, is refused ("No usable reading"). This puts the responsibility on
  the water: it has to be at a rolling boil.
- **Fixed: the buzzer could stay on** if the heating screen was left within a third of a second
  of a beep. It is now switched off whenever that screen is left.
- **Fixed: a button held to dismiss the 100 °C result screen** carried on into the menu.
- **Fixed: the curve skipped a section** if the cold offset was lowered a lot after the 100 °C
  point was stored. Such a point is now ignored.

## Build 40

- **"Calibrate at 100C" (new, Advanced settings).** Hold the tip in boiling water with the
  heater off, confirm, and the iron stores that reading as its 100 °C point.
- **The temperature curve now corrects for each iron's amplifier zero.** The curve is kept as
  signal (reading minus the amplifier's zero) instead of raw readings. The zero comes from
  "Calibrate CJC" when it is above 0, and from the 100 °C point when CJC reads 0, where the
  true zero can be below 0 and CJC cannot see it. On the development iron the amplifier reads
  0 cold and about 1300 µV at 100 °C, a zero near -700 µV. Before this change that iron showed
  about 40 °C in boiling water and, by the same offset, far too little at soldering
  temperatures: the tip was an estimated 50 °C hotter than displayed. After calibrating, a
  drop of water on the tip sat still at a set 100 °C and boiled off at 105 °C.
  **Expect the same set temperature to give a cooler tip than before on an iron like that.**
- **One beep per heat-up.** The buzzer sounds when the tip first reaches the target, and again
  only when a target at least 20 °C higher is reached (boost, or the set temperature turned
  up). It no longer beeps each time the reading drifts out of range and back under load.
- **Heating screen: holding Up or Down changes the target on the spot**, by "Temp change long"
  (10 °C unless set larger), repeating at the settings menu's pace. No other screen opens. A
  press still changes it by 5 °C.
- **Screen flip needs a clear tilt.** With automatic orientation the iron has to be tilted
  about 45° the other way (was about 29°) and held there for half a second before the screen
  turns over.

## Build 36

- **Fixed: "Calibrate CJC" never ran.** The request was only written to memory when the menu
  was left, so unplugging the iron from inside the menu, which is what the prompt asks for,
  lost it. It is now saved at once.
- **Cold-end offset default is 0, and 0 can be stored.** Measured on the development iron, the
  tip amplifier reads 0 with a cold tip; the port's default of 900 µV (IronOS's default for
  other irons) made everything from room temperature to about 55 °C read as 25 °C, and for the
  first moment of a heat-up. The stock firmware's curve also starts at 0. **If you update from
  an earlier build, run "Calibrate CJC" once with the iron cold, or restore default settings:**
  the old value of 900 stays in your settings otherwise.
- **Supply voltage is in the top right corner of every screen, and heater power is always shown
  bottom right** (0 W on the ready screen).
- **Heating screen buttons:** Up and Down now change the target directly, as on the ready
  screen. Press OK to stop heating. Hold OK for boost (it was hold Down). Hold Up or Down and
  the temperature screen opens and keeps changing the target while the button stays down; you
  no longer have to let go and hold again.
- **One firmware only.** The builds with the port's interface and the two alternative PID
  profiles are gone; the Makefile always builds the card theme with the hybrid PID.
- **Browser demo** of the screens and menus: https://coldjunction.github.io/IronOS-HS02A/demo/

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
- **Boost is on OK held** (the port uses Down held), and OK pressed stops heating.

## Known issues

- **Units with a factory tip calibration are untested.** The iron this was developed on has
  none (the debug page shows "Custom Curve"), so it uses the fallback curve, which is what
  the temperature work here changed. The factory-calibration branch of
  `source/Core/BSP/Fnirsi/ThermoModel.cpp` is the port's original code. A review found that it
  subtracts a fixed 25 °C where its comment says "handle temperature", and clamps the result at
  zero, so such a unit may show the MCU temperature for a cold tip and read some degrees high
  when warm. It may also apply the calibration offset differently from the fallback curve. Not
  confirmed on hardware.
- **Calibrate before trusting the display.** The tip amplifier's zero differs from iron to
  iron by enough to matter: 100 µV is about 4 °C at soldering temperatures, and the development
  iron was off by about 1500 µV against the port's curve. Run "Calibrate CJC" and then
  "Calibrate at 100C". An uncalibrated iron whose zero is below 0 runs hotter than it shows.
- **The 100 °C calibration is an estimate above 100 °C.** It fixes the reading at 100 °C and
  shifts the rest of the curve by the zero it finds, using an assumed signal of 2000 µV at
  100 °C and assuming water boils at 100 °C (about 1 °C less per 300 m above sea level). The
  result at soldering temperatures is probably within 15 °C, and has not been checked against
  a reference there.
- **Nothing below 25 °C is shown.** The sensor measures how much hotter the tip is than the
  end it plugs into, which is assumed to be at 25 °C. A tip colder than that end would give a
  negative voltage, which the amplifier cannot output.
- **The CJC "iron is warm" check is weak.** It relies on the MCU's own temperature and did not
  trigger in a test straight after heating. Only calibrate an iron you know is cold.
- **No tip-resistance or short-circuit detection.** A missing tip is detected.
- **PID gains were tuned on one iron and in a simulator.** Overshoot above 400 °C and with
  other tip shapes is not measured.
- **Temperature accuracy at soldering temperature is not verified** against a reference
  (solder melting point or a tip thermometer).
