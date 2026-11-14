// HS-02 card theme easter eggs: two short tunes on the buzzer.
// Hold UP and DOWN together on the home screen to play the first; keep holding for five
// seconds and the second takes over. Any button stops a tune.
//   0: the opening of Beethoven's "Ode to Joy"
//   1: the opening of Grieg's "In the Hall of the Mountain King"
// Both are in the public domain. To play something else, replace a table: each entry is a
// pitch in Hz (0 = rest) and a length in milliseconds.
#include "BSP.h"
#include "FreeRTOS.h"
#include "Settings.h"
#include "configuration.h"
#include "task.h"

#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
void setBuzzerTone(uint16_t hz); // Core/BSP/Fnirsi/BSP.cpp

namespace {
struct Note {
  uint16_t hz;
  uint16_t ms;
};
constexpr uint16_t A5 = 880, As5 = 932, B5 = 988, C6 = 1047, D6 = 1175, Ds6 = 1245, E6 = 1319, F6 = 1397, G6 = 1568, A6 = 1760;
constexpr uint16_t kBeat = 320; // a quarter note of the first tune
const Note         kOdeToJoy[] = {
            {E6, kBeat}, {E6, kBeat}, {F6, kBeat}, {G6, kBeat}, {G6, kBeat}, {F6, kBeat}, {E6, kBeat}, {D6, kBeat}, {C6, kBeat}, {C6, kBeat}, {D6, kBeat},     {E6, kBeat},
            {E6, kBeat * 3 / 2}, {D6, kBeat / 2}, {D6, kBeat * 2}, {E6, kBeat}, {E6, kBeat}, {F6, kBeat}, {G6, kBeat}, {G6, kBeat}, {F6, kBeat}, {E6, kBeat}, {D6, kBeat},
            {C6, kBeat}, {C6, kBeat}, {D6, kBeat}, {E6, kBeat}, {D6, kBeat * 3 / 2}, {C6, kBeat / 2}, {C6, kBeat * 2},
};
constexpr uint16_t kStep = 170; // an eighth note of the second tune
const Note         kMountainKing[] = {
            {A5, kStep}, {B5, kStep}, {C6, kStep}, {D6, kStep}, {E6, kStep}, {C6, kStep}, {E6, kStep * 2}, {Ds6, kStep}, {B5, kStep}, {Ds6, kStep * 2}, {D6, kStep}, {As5, kStep}, {D6, kStep * 2},
            {A5, kStep}, {B5, kStep}, {C6, kStep}, {D6, kStep}, {E6, kStep}, {C6, kStep}, {E6, kStep},     {A6, kStep},  {G6, kStep}, {E6, kStep},      {C6, kStep}, {E6, kStep},  {G6, kStep * 4},
            {A5, kStep}, {B5, kStep}, {C6, kStep}, {D6, kStep}, {E6, kStep}, {C6, kStep}, {E6, kStep * 2}, {Ds6, kStep}, {B5, kStep}, {Ds6, kStep * 2}, {D6, kStep}, {As5, kStep}, {D6, kStep * 2},
            {A5, kStep}, {B5, kStep}, {C6, kStep}, {D6, kStep}, {E6, kStep}, {C6, kStep}, {E6, kStep},     {A6, kStep},  {G6, kStep}, {E6, kStep},      {C6, kStep}, {E6, kStep},  {A5, kStep * 4},
};
struct Tune {
  const Note *notes;
  uint8_t     length;
};
const Tune kTunes[] = {
    {kOdeToJoy, sizeof(kOdeToJoy) / sizeof(kOdeToJoy[0])},
    {kMountainKing, sizeof(kMountainKing) / sizeof(kMountainKing[0])},
};
constexpr uint8_t  kTuneCount = sizeof(kTunes) / sizeof(kTunes[0]);
constexpr uint8_t  kIdle      = 0xFF;
constexpr uint16_t kGapMs     = 40; // silence at the end of each note, so repeated notes are heard apart

uint8_t    playing  = kIdle; // which tune, or kIdle
uint8_t    position = 0;     // the note that sounds next
TickType_t noteEnds = 0;
} // namespace

bool hs02TunePlaying(void) { return playing != kIdle; }

// Starts a tune from its first note. Does nothing if that tune is already playing, so it can be
// called on every frame a button is held; a different tune replaces the one that is playing.
void hs02TuneStart(uint8_t tune) {
  if (!getSettingValue(SettingsOptions::BuzzerEnabled) || tune >= kTuneCount || tune == playing) {
    return;
  }
  setBuzzerTone(0);
  playing  = tune;
  position = 0;
  noteEnds = xTaskGetTickCount();
}

void hs02TuneStop(void) {
  if (playing != kIdle) {
    playing = kIdle;
    setBuzzerTone(0);
  }
}

// Called once per GUI frame.
void hs02TuneTick(void) {
  if (playing == kIdle) {
    return;
  }
  const TickType_t now = xTaskGetTickCount();
  if ((int32_t)(noteEnds - now) > 0) {
    if ((int32_t)(noteEnds - now) <= (int32_t)(kGapMs * configTICK_RATE_HZ / 1000)) {
      setBuzzerTone(0);
    }
    return;
  }
  if (position >= kTunes[playing].length) {
    hs02TuneStop();
    return;
  }
  const Note &note = kTunes[playing].notes[position];
  setBuzzerTone(note.hz);
  noteEnds = now + (TickType_t)note.ms * configTICK_RATE_HZ / 1000;
  position++;
}
#endif
