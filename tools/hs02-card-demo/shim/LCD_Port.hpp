#pragma once
#include <stdint.h>
extern uint8_t demoBacklight;
static inline void LCDSetBacklight(const uint8_t brightness) { demoBacklight = brightness; }
