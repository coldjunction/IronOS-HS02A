# IronOS for the FNIRSI HS-02A: card theme, retuned PID

A modified version of the [bugparty/IronOS](https://github.com/bugparty/IronOS) port of
[IronOS](https://github.com/Ralim/IronOS) for the FNIRSI HS-02A soldering iron.

**[Try it in your browser](https://coldjunction.github.io/IronOS-HS02A/demo/)**: the real screens and menus, compiled to WebAssembly.

**Status:** developed and tested on one HS-02A. It is not an official FNIRSI or IronOS release.
Flash it at your own risk, and read the known issues first.

## Credits

This fork is a set of changes on top of other people's work. Nearly all of the firmware is theirs.

- **[IronOS](https://github.com/Ralim/IronOS)** by Ben V. Brown (Ralim) and its contributors:
  the firmware itself.
- **The HS-02 port of IronOS** by [PanKleszcz](https://github.com/PanKleszcz/IronOS),
  [OK2CM](https://github.com/ok2cm/IronOS) and [bugparty](https://github.com/bugparty/IronOS):
  everything that makes IronOS run on this iron, including the display driver, power and
  button handling, the buzzer, the colour home screen, the tip temperature curve and the
  decoding of the stock firmware's factory calibration. This fork starts from
  bugparty/IronOS, branch `dev`, commit `93da587`.
- **[Inter](https://github.com/rsms/inter)** by The Inter Project Authors (SIL Open Font
  License, see [licenses/Inter-OFL-1.1.txt](licenses/Inter-OFL-1.1.txt)): the digits on the
  home screen, already used by the port.

If you want a maintained HS-02 firmware, use the port linked above. If something here is
useful to it, it is free to take under the same licence.

## Controls

The iron has three buttons. Left to right with the screen facing you: **Down**, **Up**, **OK**.
You can try all of this in the [browser demo](https://coldjunction.github.io/IronOS-HS02A/demo/).

### Flashing the firmware

1. Unplug the iron.
2. Hold **OK** and plug the iron into a computer with a USB-C cable. A drive named `BOOTLOADER` appears.
3. Copy the `.bin` file from the [latest release](../../releases/latest) onto that drive. The iron restarts with the new firmware.

### Ready screen (after plugging in)

| To do this | Do this |
|---|---|
| Start heating | Press **OK** |
| Change the target temperature | Press **Up** or **Down** |
| Lower the target quickly | Hold **Down** |
| Open the settings | Hold **Up** |

### Heating screen

| To do this | Do this |
|---|---|
| Change the target temperature | Press **Up** or **Down** |
| Change it quickly | Hold **Up** or **Down** |
| Boost (hotter for one big joint) | Hold **OK** for as long as you need it |
| Stop heating | Press **OK** |

### Settings

| To do this | Do this |
|---|---|
| Move through the entries | **Down** for the next one, **Up** for the previous one |
| Open a group, or start changing a value | Press **OK** |
| Change the value | **Down** for less, **Up** for more (hold to repeat) |
| Save the value | Press **OK** |
| Go back one level, or leave the settings | Hold **OK** |

The bottom row of the settings screen always shows what the three buttons do at that moment.

### The screens

- **Ready:** the iron is plugged in and not heating.
- **Heating:** the tip is heating to the target. The bar along the bottom is heater power.
- **Boost:** the same, while OK is held, heating to the boost temperature.
- **Sleep:** the iron has not moved for a while and has dropped to the sleep temperature. Move it or press a button to wake it.
- **No tip:** no tip is fitted.

Every screen shows the tip temperature (large), the supply voltage (top right) and the heater power in watts (bottom right).
The [user guide](Documentation/HS02A-Custom-Firmware.md) covers every menu entry.

## Calibrating the temperature

Do this once after flashing. Without it the display can be tens of degrees off: the
development iron ran about 50 °C hotter than it showed.

1. **Calibrate CJC (cold iron).** Leave the iron unplugged for 30 minutes. Plug in, open
   **Advanced settings → Calibrate CJC at next boot**, press OK, then unplug and plug in
   again. It measures for two seconds at start-up.
2. **Calibrate at 100C (boiling water).** Keep a small pot of water at a rolling boil. Open
   **Advanced settings → Calibrate at 100C**, hold the front 2 cm of the tip in the water for
   30 seconds, press OK, and keep it there while "Measuring" shows. The heater stays off; do
   not heat the iron in the water.
3. **Check it.** A cold iron shows 25 °C and the tip in boiling water about 100 °C. A drop of
   water on the tip sits still at a set 100 °C and boils off at about 105 °C.

The iron cannot show less than 25 °C: its sensor measures how much hotter the tip is than the
handle end, which is assumed to be at room temperature. The
[user guide](Documentation/HS02A-Custom-Firmware.md#calibrating-the-temperature) explains why
both steps are needed and what the calibration cannot do.

## What is different from the port

- **PID retuned for less overshoot.** Tuned in a simulator and against measurements on the
  iron; the stock firmware's control law was decoded for comparison. No heat-up ramp or soft
  start: full power from the first moment.
- **Card theme.** A redrawn ready / heating / boost / sleep screen with a colour per mode, a
  power bar, the supply voltage, and a no-tip animation.
- **Simpler settings menu.** One entry per screen, whole words for values, and a hint row that
  shows what the three buttons do, in the order they sit on the iron.
- **Temperature calibration that works.** "Calibrate CJC" now actually runs, and a new
  "Calibrate at 100C" step in boiling water corrects for each iron's amplifier zero, which the
  port's curve ignored.
- **New settings:** accent colour per screen, buzzer on/off, button light on/off, a switch to
  allow supplies under 9 V, debug pages from the menu (with three HS-02 pages: tip
  calibration, current sense, live sensor values).
- **Small things:** no boot logo and a shorter start-up, steadier temperature and watt numbers,
  two tunes on the buzzer.

## Documents

- [User guide and feature list](Documentation/HS02A-Custom-Firmware.md): installing, every
  button, screen and menu entry, calibration, power supplies, building from source.
- [Function reference](Documentation/HS02A-Function-Reference.md): what every function added
  or changed by this fork does.
- [Changelog and known issues](HS02A-CHANGELOG.md).

The firmware file is attached to the [releases](../../releases). There is one firmware: the
card theme with the hybrid PID.

## Known limits

- Units that carry a factory tip calibration are untested (the development iron has none).
- No tip-resistance or short-circuit detection.
- Temperature accuracy at soldering temperature has not been checked against a reference.
- English only.

## Licence

GPL-3.0, the licence of IronOS (see [LICENSE](LICENSE)). The stock FNIRSI firmware is not
included. This project is not affiliated with FNIRSI.

The rest of this page is the README of the port this fork is based on.

---

[![Total Downloads](https://img.shields.io/github/downloads/bugparty/IronOS/total)](https://github.com/bugparty/IronOS)
[![Latest Release](https://img.shields.io/github/v/release/bugparty/IronOS)](https://github.com/bugparty/IronOS/releases/latest)

# IronOS - FNIRSI HS-02 fork

This repository provides [FNIRSI HS-02](https://www.fnirsi.com/products/hs-02) iron support.

## HS-02 Color UI

The HS-02 home, soldering, boost, and sleep pages use a four-colour interface.

![HS-02 soldering UI preview](docs/assets/hs02-wash-ui.png)

It is a fork of following projects:
* [Ralim - IronOS](https://github.com/Ralim/IronOS)
* [PanKleszcz - IronOS](https://github.com/PanKleszcz/IronOS)
* [ok2cm - IronOS](https://github.com/ok2cm/IronOS)

## Key Features

- Aggresive PID controller , better heat power than stock firmware.
- Full screen LCD support (Main Screen is colored now).
- Accelerometer support.
- Status LED support.
- Buzzer support; however, disabled in default releases because it is too annoying.
- Stock Boot logo added.

## Missing features & limitations

- Tip resistance detection and short circuit protection.
- PD required voltage selection.
- Translations in CJK won't works, others in theory is fine, but I don't recommend.

## Supported Hardware

|     Device     | DC  | QC  | PD   |        Notes                  |
| :------------: | :-: | :-: | :--: | :---------------------------: |
| FNIRSI HS-02A  | ✔️  | ❌  | ✔️\* |                               |
| FNIRSI HS-02B  | ✔️  | ❌  | ✔️\* | Not tested                    |
| \*\*           |     |     |      |                               |

\* _HS-02_ has CH224K PD controller which provides no negotiation feedback to the HS02A CPU (as far as I know). Requested voltage is fixed to 20V in this release. 
\*\* [Original IronOS](https://github.com/Ralim/IronOS) HW ports are preserved but they are _not tested and may be broken!_

## Installation

1. Hold _OK button_ and connect HS-02 to PC using USB cable.
2. Release the button when _"Please drag the firmware file to the USB flash disk!"_ message is displayed.
3. Wait until HS02 bootloader is detected as FAT mass storage device named _BOOTLOADER_ having an empty file _READY.TXT_ insde.
4. Copy _HS02_xx_firmware.bin_ file on the _BOOTLOADER_ device.
    1. On Linux, it is required to copy (replace) the file one more time for some reason.
    2. There is a timeout (≈ 40 seconds) to start the upload. If expired, the bootloader is terminated and current firmware is launched.
5. _"Updating firmware!"_ message is displayed and the device is rebooted into the new firmware.

## Builds

The links in the table below allow to download available builds directly:

|        Device         | Stable Release |
|:---------------------:|:--------------:|
| FNIRSI HS-02A         | [English](https://github.com/bugparty/IronOS/releases/latest/download/HS02_EN_firmware.bin), [Русский](https://github.com/bugparty/IronOS/releases/latest/download/HS02_RU_firmware.bin), [Svenska](https://github.com/bugparty/IronOS/releases/latest/download/HS02_SV_firmware.bin), [other languages](https://github.com/bugparty/IronOS/releases/latest/download/HS02_all_languages.zip) |

[^changelog]:

## Getting Started

To get started with _IronOS firmware_, please jump to [Getting Started Guide](https://ralim.github.io/IronOS/GettingStarted/) (original IronOS documentation).

## Basic Control

Supported device is controlled by two buttons which can be pressed in the following ways:
 - short: ~1 second or so;
 - long: more than 1 second;
 - both (press & hold both of them together).

Available buttons are:
 - `+/A` button: near the front closer to the tip (for irons) or on the left side of the device (for plates);
 - `-/B` button: near the back far from the tip (for irons) or on the right side of the device (for plates).

After powering on the device for the first time with _IronOS_ installed and having the tip/plate plugged in, on the main menu in _standby mode_ the unit shows a pair of prompts for the two most common operations:
- pressing the `+/A` button enters the _soldering mode_;
- pressing the `-/B` button enters the _settings menu_;
- in _soldering mode_:
  - short press of `+/A` / `-/B` buttons changes the soldering temperature;
  - long press of the `+/A` button enables _boost mode_ (increasing soldering temperature to the adjustable setting as long as the button is pressed);
  - long press of the `-/B` button enters _standby mode_ and stops heating;
- in _standby mode_:
  - long press of the `+/A` button enters _soldering temperature adjust mode_ (the same as the one in the _soldering mode_, but allows to adjust the temperature before heating up);
  - long hold of the `-/B` button enters the [_debug menu_](https://ralim.github.io/IronOS/DebugMenu/);
- in _menu mode_ (to make it short here):
  - `-/B` scrolls & cycles through menus and submenus;
  - `+/A` enters to menu & submenu settings or changes their values if they are activated already.

Additional details are described in the [menu information](https://ralim.github.io/IronOS/Menu/).
## UI preview renderer

The preview is rendered from the production drawing code through WebAssembly:

```bash
source /path/to/emsdk/emsdk_env.sh
tools/hs02-wasm-preview/build.sh
python3 -m http.server 18080 --directory tools/hs02-wasm-preview
```

Open `http://127.0.0.1:18080/web/` and select idle, solder, boost, or sleep.

## License

The code created by the community is covered by the [GNU GPLv3](https://www.gnu.org/licenses/gpl-3.0.html#license-text) license **unless noted elsewhere**.
Other components such as _FreeRTOS_ and _USB-PD_ have their own licenses.

## Commercial Use

This software is provided _**"AS IS"**_, so I cannot provide any commercial support for the firmware.
However, you are more than welcome to distribute links to the firmware or provide hardware with this firmware.
**Please do not re-host the files, but rather link to this page, so that there are no old versions of the firmware scattered around**.
