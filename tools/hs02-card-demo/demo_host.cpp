// Browser host for the HS-02A card theme demo (tools/hs02-card-demo, built by build.sh).
//
// The firmware's own GUI thread, menu logic, settings, button handling and display
// drivers are compiled unchanged. This file only stands in for the hardware: an ST7735
// panel that records what the driver sends it, three buttons, a flash page, a clock and a
// very simple tip (the heater model is NOT the firmware's PID, just enough to make the
// soldering screens move).
#include "BSP.h"
#include "Buttons.hpp"
#include "Display.hpp"
#include "FnirsiBootLogo.h"
#include "LCD.hpp"
#include "OperatingModes.h"
#include "Settings.h"
#include "TipThermoModel.h"
#include "Translation.h"
#include "main.hpp"
#include "power.hpp"
#include "settingsGUI.hpp"
#include "ui_drawing.hpp"
#include <cstring>

#define IMPORT(name) __attribute__((import_module("env"), import_name(#name)))
extern "C" {
IMPORT(host_yield) void host_yield(uint32_t ms); // the page shows the panel, waits, then resumes us
IMPORT(host_reboot) void host_reboot(void);      // never returns
}

// ---- what the page writes / reads ------------------------------------------------------
struct Inputs {
  uint8_t  buttons;    // bit0 DOWN (A), bit1 UP (B), bit2 OK
  uint8_t  moved;      // iron picked up / moved since the last look
  uint8_t  tipRemoved;
  uint8_t  jointWatts; // heat drawn from the tip by a joint
  uint16_t voltsX10;
};
struct State {
  uint32_t tick;
  uint16_t tipCx10, targetC, wattsX10;
  uint8_t  backlight, led, mode, rotated, inverted;
  uint16_t buzzerHz;    // what the buzzer is sounding, 0 = silent
  uint8_t  buttonLight; // the white light behind DOWN and UP
};
static Inputs   inputs = {0, 0, 0, 0, 200};
static State    state;
static uint16_t panel[LCD_WIDTH * LCD_HEIGHT]; // true RGB565 colours as the panel shows them
static uint8_t  flashPage[2048];
uint8_t         demoBacklight = 0;

// ---- clock, heater model ---------------------------------------------------------------
static TickType_t g_tick   = 1000;
static float      tipC     = 26.0f, integralW = 0.0f, heaterW = 0.0f;
static uint32_t   wattTimer = 0;

volatile TemperatureType_t                    currentTempTargetDegC = 0;
volatile TickType_t                           lastMovementTime      = 0;
volatile AccelType                            DetectedAccelerometerVersion = AccelType::KXTJ3;
volatile TaskHandle_t                         pidTaskNotification   = nullptr;
bool                                          settingsWereReset     = false;
bool                                          usb_pd_available      = false;
int32_t                                       powerSupplyWattageLimit = 0;
uint8_t                                       accelInit             = 1;
uint8_t                                       heaterThermalRunawayCounter = 0;
expMovingAverage<uint32_t, wattHistoryFilter> x10WattHistory        = {0};

static void advance(uint32_t ms) {
  while (ms) {
    const uint32_t step = ms > 10 ? 10 : ms;
    const float    dt   = step / 1000.0f;
    ms -= step;
    g_tick += step;
    const float target = inputs.tipRemoved ? 0.0f : (float)currentTempTargetDegC;
    float       limit  = (float)getSettingValue(SettingsOptions::PowerLimit);
    const float volts  = inputs.voltsX10 / 10.0f, available = volts * volts / 2.5f;
    if (limit <= 0 || limit > 100) limit = 100;
    if (limit > available) limit = available;
    if (target < 10) {
      heaterW = integralW = 0;
    } else {
      const float err = target - tipC;
      float       p   = 2.0f * err + integralW;
      if (p > 0 && p < limit) integralW += 1.0f * err * dt;
      if (integralW < 0) integralW = 0;
      if (integralW > limit) integralW = limit;
      heaterW = p < 0 ? 0 : (p > limit ? limit : p);
    }
    const float ambient = 26.0f;
    tipC += dt * (heaterW * 0.83f - (tipC - ambient) / 36.0f - (tipC > 150 ? inputs.jointWatts : 0)) / 0.9f;
    if (inputs.tipRemoved) tipC = ambient;
    wattTimer += step;
    if (wattTimer >= 50) {
      wattTimer = 0;
      x10WattHistory.update((uint32_t)(heaterW * 10));
    }
  }
  if (inputs.moved) lastMovementTime = g_tick;
}

extern "C" {
TickType_t xTaskGetTickCount(void) { return g_tick; }
void       vTaskDelay(TickType_t ticks) {
  advance(ticks);
  if (ticks > 10) host_yield(ticks); // very short waits (panel settle) just pass time
}
void vTaskDelayUntil(TickType_t *previousWake, TickType_t increment) {
  const TickType_t wake = *previousWake + increment;
  *previousWake         = wake;
  if ((int32_t)(wake - g_tick) > 0) {
    const uint32_t wait = wake - g_tick;
    advance(wait);
    host_yield(wait);
  }
}
}

// ---- ST7735 panel ----------------------------------------------------------------------
// Column-major: inside the window the address runs down a column (y), then across (x).
// The panel is an IPS type: with inversion ON it shows the value sent, with it OFF the
// inverse. The firmware switches inversion OFF at start (the "invert colours" setting) and
// sends inverted colours, which is what the port's notes observed on a real unit.
static uint8_t winX0, winX1 = LCD_WIDTH - 1, winY0, winY1 = LCD_HEIGHT - 1, curX, curY;
static bool    inversionOn = true;

static void pushPixel(uint16_t wire) {
  if (curX < LCD_WIDTH && curY < LCD_HEIGHT) panel[curY * LCD_WIDTH + curX] = inversionOn ? wire : (uint16_t)~wire;
  if (curY >= winY1) {
    curY = winY0;
    curX = curX >= winX1 ? winX0 : curX + 1;
  } else {
    curY++;
  }
}
void FRToSSPI::init() {}
void FRToSSPI::sendLcdReset(void) {}
void FRToSSPI::fastSend(const uint8_t *data, size_t length) {
  for (size_t i = 0; i + 1 < length; i += 2) pushPixel((uint16_t)(data[i] << 8 | data[i + 1]));
}
void FRToSSPI::sendPixels(uint8_t *data, size_t length) {
  for (size_t i = 0; i < length; i++)
    for (uint8_t bit = 0; bit < 8; bit++) pushPixel((data[i] >> bit) & 1 ? 0xFFFF : 0x0000);
}
void FRToSSPI::sendCmdChain(const FRToSSPI::SPI_CMD *commands, size_t length) {
  for (size_t i = 0; i < length; i++) {
    const FRToSSPI::SPI_CMD &c = commands[i];
    switch (c.cmd) {
    case 0x2B: winX0 = c.data[1] - ST7735_XOFFSET; winX1 = c.data[3] - ST7735_XOFFSET; break;
    case 0x2A: winY0 = c.data[1] - ST7735_YOFFSET; winY1 = c.data[3] - ST7735_YOFFSET; break;
    case 0x2C: curX = winX0; curY = winY0; break;
    case 0x20: inversionOn = false; break;
    case 0x21: inversionOn = true; break;
    case 0x36: state.rotated = c.data[0] == 0x48; break;
    default: break;
    }
  }
}

// ---- board functions -------------------------------------------------------------------
uint8_t getButtonA() { return inputs.buttons & 1; }
uint8_t getButtonB() { return (inputs.buttons >> 1) & 1; }
uint8_t getButtonOK() { return (inputs.buttons >> 2) & 1; }

void flash_read_buffer(uint8_t *buffer, const uint16_t length) { memcpy(buffer, flashPage, length < sizeof flashPage ? length : sizeof flashPage); }
void flash_save_buffer(const uint8_t *buffer, const uint16_t length) { memcpy(flashPage, buffer, length < sizeof flashPage ? length : sizeof flashPage); }

static uint16_t tipMicrovolts() {
  static const float curve[][2] = {{0, 30.5f}, {5400, 223}, {6680, 273}, {8370, 350}, {10540, 400}, {12090, 450}, {13640, 500}};
  if (tipC <= curve[0][1]) return 0;
  for (int i = 1; i < 7; i++)
    if (tipC <= curve[i][1]) return (uint16_t)(curve[i - 1][0] + (tipC - curve[i - 1][1]) * (curve[i][0] - curve[i - 1][0]) / (curve[i][1] - curve[i - 1][1]));
  return 13640;
}
uint64_t          getDeviceID() { return 0x1234567890ABCDEFULL; }
uint32_t          getDeviceValidation() { return 0; }
uint8_t           getDeviceValidationStatus() { return 0; }
uint16_t          getHandleTemperature(uint8_t) { return 300; }
uint16_t          getInputVoltageX10(uint16_t, uint8_t) { return inputs.voltsX10; }
bool              getIsPoweredByDCIN() { return true; }
uint16_t          getTipRawTemp(uint8_t) { return tipMicrovolts(); }
uint8_t           getTipResistanceX10() { return 25; }
uint16_t          getTipThermalMass() { return 0; }
uint16_t          getTipInertia() { return 0; }
bool              isTipDisconnected() { return inputs.tipRemoved; }
bool              isTipShorted() { return false; }
uint8_t           preStartChecks() { return 1; }
void              setTipX10Watts(int32_t) {}
void              setTipPWM(const uint16_t, const bool) {}
uint16_t          X10WattsToPWM(int32_t, uint8_t) { return 0; }
uint32_t          availableW10(uint8_t) { return (uint32_t)inputs.voltsX10 * inputs.voltsX10 / 25; }
void              setStatusLED(const enum StatusLED led) { state.led = (uint8_t)led; }
void              setBuzzer(bool on) { state.buzzerHz = (on && getSettingValue(SettingsOptions::BuzzerEnabled)) ? 2000 : 0; }
void              setBuzzerTone(uint16_t hz) { state.buzzerHz = getSettingValue(SettingsOptions::BuzzerEnabled) ? hz : 0; }
static uint8_t    buttonLightOverride = 0xFF;
void              hs02ApplyButtonLight(void) { state.buttonLight = (buttonLightOverride != 0xFF ? buttonLightOverride : getSettingValue(SettingsOptions::ButtonLight)) ? 1 : 0; }
void              hs02OverrideButtonLight(uint8_t level) {
  buttonLightOverride = level;
  hs02ApplyButtonLight();
}
void              resetWatchdog() {}
void              reboot() { host_reboot(); }
void              delay_ms(uint16_t ms) { advance(ms); }
bool              getHallSensorFitted() { return false; }
int16_t           getRawHallEffect() { return 0; }
bool              getFUS302IRQLow() { return false; }
void              unstick_I2C() {}
// Invented numbers: nobody has measured the real sensor yet (that is what the page is for).
static uint16_t senseOn = 0, sensePeak = 0, senseVolts = 0;
static uint8_t  senseLevel = 0;
void hs02CurrentSenseSample(void) {
  const float volts = inputs.voltsX10 / 10.0f, full = volts * volts / 2.5f;
  const uint8_t level = (uint8_t)(heaterW / full * 127);
  if (level == 0) return;
  float counts = volts / 2.5f * 0.010f * 49.0f / 3.3f * 4095.0f;
  if (counts > 4095) counts = 4095;
  if ((uint16_t)counts > sensePeak) sensePeak = (uint16_t)counts;
  if (level >= senseLevel) { senseLevel = level; senseOn = (uint16_t)(counts * 0.97f); senseVolts = inputs.voltsX10; }
}
void hs02GetCurrentSense(uint16_t &offMean, uint16_t &onMean, uint16_t &peak, uint8_t &onPwmPercent, uint16_t &onVoltsX10) {
  offMean = 9; onMean = senseOn; peak = sensePeak; onPwmPercent = (uint8_t)(senseLevel * 100 / 127); onVoltsX10 = senseVolts;
}
bool              hs02GetFactoryTipCal(uint32_t &a, uint32_t &b, uint32_t &c) { a = 223; b = 393; c = 572; return false; }
uint32_t          TipThermoModel::convertTipRawADCTouV(uint16_t raw, bool) { return raw; }
TemperatureType_t TipThermoModel::convertTipRawADCToDegC(uint16_t) { return (TemperatureType_t)tipC; }
TemperatureType_t TipThermoModel::getTipInC(bool) { return (TemperatureType_t)tipC; }
TemperatureType_t TipThermoModel::getTipInF(bool) { return (TemperatureType_t)(tipC * 9 / 5 + 32); }
TemperatureType_t TipThermoModel::getTipInCx10(bool) { return (TemperatureType_t)(tipC * 10); }
TemperatureType_t TipThermoModel::getTipMaxInC() { return 450; }
TemperatureType_t TipThermoModel::convertCtoF(TemperatureType_t c) { return c * 9 / 5 + 32; }
TemperatureType_t TipThermoModel::convertFtoC(TemperatureType_t f) { return f < 32 ? 0 : (f - 32) * 5 / 9; }

void showBootLogo(void) { // as Core/BSP/Fnirsi/BSP.cpp
#ifdef HS02_THEME_CARD
  return;
#endif
  LCD::drawNativeImage(FNIRSI_LOGO_X, FNIRSI_LOGO_Y, FNIRSI_LOGO_W, FNIRSI_LOGO_H, fnirsiBootLogo);
  vTaskDelay(TICKS_SECOND / 60);
  LCD::setBrightness(getSettingValue(SettingsOptions::DisplayBrightness));
  waitForButtonPressOrTimeout(TICKS_SECOND * 2);
}

// ---- entry points ----------------------------------------------------------------------
extern ButtonState   buttonsAtDeviceBoot;
extern OperatingMode currentOperatingMode;
extern guiContext    context;
void                 guiRenderLoop(void);
void                 ui_pre_render_assets(void);

static void publish() {
  state.tick      = g_tick;
  state.tipCx10   = (uint16_t)(tipC * 10);
  state.targetC   = (uint16_t)currentTempTargetDegC;
  state.wattsX10  = (uint16_t)(heaterW * 10);
  state.backlight = demoBacklight;
  state.mode      = (uint8_t)currentOperatingMode;
  state.inverted  = !inversionOn;
}

extern "C" {
// keepFlash: the page has put the previous flash contents back (a reboot, not a first start)
void demo_init(uint32_t keepFlash) {
  if (!keepFlash) memset(flashPage, 0xFF, sizeof flashPage);
  settingsWereReset = loadSettings();
  prepareTranslations(); // as startGUITask(), up to its endless loop
  Display::setBrightness(0);
  Display::initialize();
  Display::setInverseDisplay(getSettingValue(SettingsOptions::DisplayInversion));
  ui_pre_render_assets();
  memset(&context, 0, sizeof(context));
  Display::setRotation(getSettingValue(SettingsOptions::OrientationMode) & 1, false);
  currentOperatingMode = OperatingMode::StartupLogo;
  publish();
}
// One pass of the GUI thread's loop; the page calls this every 40 ms.
void demo_frame(uint32_t elapsedMs) {
  advance(elapsedMs);
  guiRenderLoop();
  publish();
}
void      demo_publish(void) { publish(); }
Inputs   *demo_inputs(void) { return &inputs; }
State    *demo_state(void) { return &state; }
uint16_t *demo_panel(void) { return panel; }
uint8_t  *demo_flash(void) { return flashPage; }
uint32_t  demo_flash_size(void) { return sizeof flashPage; }
}
