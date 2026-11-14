/*
 * Power.hpp
 *
 *  Created on: 28 Oct, 2018
 *     Authors: Ben V. Brown, David Hilton (David's Idea)
 */

#include "BSP.h"
#include "configuration.h"
#include "expMovingAverage.h"
#include "stdint.h"
#include <history.hpp>
#ifndef POWER_HPP_
#define POWER_HPP_

// thermal mass = 1690 milliJ/*C for my tip.
//  ->  Wattsx10*Seconds to raise Temp from room temp to +100*C, divided by 100*C.
// we divide mass by 20 to let the I term dominate near the set point.
//  This is necessary because of the temp noise and thermal lag in the system.
// Once we have feed-forward temp estimation we should be able to better tune this.

// Boards may set WATT_HISTORY_WEIGHT in configuration.h; 24 is the long-standing default
#ifndef WATT_HISTORY_WEIGHT
#define WATT_HISTORY_WEIGHT 24
#endif
const uint8_t                                        wattHistoryFilter = WATT_HISTORY_WEIGHT; // I term look back weighting
extern expMovingAverage<uint32_t, wattHistoryFilter> x10WattHistory;

uint32_t availableW10(uint8_t sample);
void     setTipX10Watts(int32_t mw);
uint16_t X10WattsToPWM(int32_t milliWatts, uint8_t sample = 0);
#endif /* POWER_HPP_ */
