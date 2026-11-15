// HS-02 card theme: "Meltdown", a small first-person game. Opened from Advanced settings.
// You walk a maze of corridors, seen in 3D, armed with a soldering iron. Solder blobs come
// for you. Melt them all: the exit, a red wall, then turns green and lets you out.
//
//   DOWN / UP     turn left / right (slowly at first, so a short press aims)
//   OK tapped     fire
//   OK held       walk forward
//   DOWN + UP     held for a second: leave the game
//   after the end: OK plays again
//
// Sound and lights. The buzzer plays short effects for a shot, a hit, a blob noticing you,
// being hurt and so on; there is no music. The white button light
// flashes when the player is hurt; the light in the OK button flashes with every shot. The
// usual "tip is still hot" blinking of that light is off while the game is on screen.
//
// How it is drawn. The walls are ray-cast: one ray per screen column finds the nearest wall on
// the 16 x 16 grid, and the wall is drawn taller the nearer it is. There are 80 columns, each
// two pixels wide. Enemies and repair kits are flat pictures that always face the player,
// scaled by distance and hidden behind nearer walls (the wall distance of every column is
// kept for that). Each column is put together in memory, ceiling, wall, floor, things, the
// player's iron, and then sent to the panel in one piece (LCD::drawNativeImage), so nothing
// flickers. The frame buffer is not used; guiRenderLoop() does not send it while this mode is
// active. All arithmetic is fixed point with 8 fraction bits ("Q8": 256 is one map cell).
#include "BSP.h"
#include "Display.hpp"
#include "Hs02MazeArt.hpp"
#include "LCD.hpp"
#include "OperatingModes.h"

#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
void        setBuzzerTone(uint16_t hz); // Core/BSP/Fnirsi/BSP.cpp; silent when the buzzer setting is off
extern bool hs02MenuWaitForRelease;     // SettingsMenu.cpp: the card menu ignores buttons until all are up
void        hs02OverrideButtonLight(uint8_t level); // Core/BSP/Fnirsi/BSP.cpp: 0..10, or 0xFF for the setting

namespace {
using namespace Hs02MazeArt;

constexpr uint8_t kCols = 80, kViewH = 72; // 80 columns of 2 pixels; display rows 72..79 are the status strip
constexpr int32_t kCell = 256;

// True RGB565 colours
constexpr uint16_t kCeiling = 0x18E4, kFloor = 0x39C7, kHurtTint = 0x6000, kExit = 0x07E0, kExitShut = 0xB000, kStrip = 0x0042, kHealth = 0xE8E4, kBlobMark = 0xBDF7;
const uint16_t     kWallColours[3] = {0x4C9A, 0xB3A6, 0x7BD0}; // blue, brick, grey-green
const uint16_t     kBlobPalette[4] = {0, 0xBDF7, 0x2104, 0xFFFF};
const uint16_t     kBlobHurt[4]    = {0, 0xFFFF, 0xF800, 0xFFFF};
const uint16_t     kKitPalette[4]  = {0, 0xFFFF, 0xE8E4, 0};
const uint16_t     kIronPalette[4] = {0, 0xC618, 0xFD20, 0x4A49};
const uint16_t     kIronHotPal[4]  = {0, 0xC618, 0xFFE0, 0x4A49};

enum : uint8_t { kNothing = 0, kTypeBlob = 1, kTypeKit = 2 };
struct Thing {
  int16_t    x, y; // Q8
  uint8_t    type, health;
  bool       awake;
  TickType_t nextAttack, hurtUntil;
};
struct Seen { // a thing as it appears in this frame
  int16_t         firstCol, lastCol, top, size; // columns [firstCol, lastCol), rows from top, size rows high
  int32_t         depth;                        // Q8
  const uint8_t  *sprite;
  const uint16_t *palette;
  uint8_t         thing;
};

enum class Phase : uint8_t { Start, Playing, Won, Lost };
Phase      phase = Phase::Start;
uint16_t   walls[16]; // bit x of row y
uint8_t    exitX, exitY;
Thing      things[kThingCount];
int32_t    posX, posY;    // Q8
uint16_t   angle;         // 256 steps per full turn, in Q8
int16_t    health;
uint8_t    blobsLeft;
TickType_t startedAt, lastFrame, okSince, bothSince, turnSince, fireUntil, hurtUntil, flashUntil, endedAt;
bool       wasOk, fireNow;
uint16_t   wallDistance[kCols]; // Q8, capped
uint8_t    wire[kViewH * 2 * 2]; // one column, twice: what goes to the panel
// what the status strip and the end screen show now
int16_t shownHealth;
uint8_t shownBlobs;
bool    endDrawn;

int32_t sine(uint8_t a) {
  const uint8_t i = a & 63;
  switch (a >> 6) {
  case 0:
    return kSine[i];
  case 1:
    return kSine[64 - i];
  case 2:
    return -kSine[i];
  default:
    return -kSine[64 - i];
  }
}
int32_t cosine(uint8_t a) { return sine((uint8_t)(a + 64)); }
int32_t absolute(int32_t v) { return v < 0 ? -v : v; }

bool isWall(int32_t cellX, int32_t cellY) { return cellX < 0 || cellY < 0 || cellX > 15 || cellY > 15 || ((walls[cellY] >> cellX) & 1); }

// ---- sound effects: a pitch (Hz, 0 = silence) for every 40 ms. The buzzer has one voice, so a
// new effect replaces the one that is sounding. ----
struct Effect {
  const uint16_t *hz;
  uint8_t         steps;
  uint8_t         priority; // an effect does not cut off one of a higher priority
};
const uint16_t kShotHz[]     = {1900, 1300, 800};
const uint16_t kHitHz[]      = {600, 450};
const uint16_t kNoticedHz[]  = {262, 330, 262};
const uint16_t kKitHz[]      = {880, 1175, 1568};
const uint16_t kHurtHz[]     = {196, 147, 196, 131};
const uint16_t kMeltedHz[]   = {740, 587, 466, 370, 294, 220};
const uint16_t kExitOpenHz[] = {740, 587, 466, 370, 294, 220, 0, 784, 784, 988, 988, 1175, 1175, 1568, 1568, 1568};
const uint16_t kWonHz[]      = {1047, 1047, 1319, 1319, 1568, 1568, 2093, 2093, 2093, 2093};
const uint16_t kLostHz[]     = {392, 392, 370, 370, 349, 349, 330, 330, 311, 311, 294, 294, 294, 294};
#define HS02_EFFECT(table, priority) {table, (uint8_t)(sizeof(table) / sizeof(table[0])), priority}
const Effect kShot = HS02_EFFECT(kShotHz, 1), kNoticed = HS02_EFFECT(kNoticedHz, 1), kHit = HS02_EFFECT(kHitHz, 2), kKitTaken = HS02_EFFECT(kKitHz, 3),
             kHurt = HS02_EFFECT(kHurtHz, 3), kMelted = HS02_EFFECT(kMeltedHz, 4), kExitOpen = HS02_EFFECT(kExitOpenHz, 5), kWon = HS02_EFFECT(kWonHz, 6),
             kLost = HS02_EFFECT(kLostHz, 6);
#undef HS02_EFFECT
const Effect *effect = nullptr; // the one that is sounding
TickType_t    effectStart;

void sound(const Effect &e, TickType_t now) {
  if (effect == nullptr || e.priority >= effect->priority) {
    effect      = &e;
    effectStart = now;
    setBuzzerTone(e.hz[0]);
  }
}
void soundTick(TickType_t now) { // every frame
  if (effect == nullptr) {
    return;
  }
  const uint32_t step = (now - effectStart) / (40 * TICKS_SECOND / 1000);
  if (step >= effect->steps) {
    effect = nullptr;
    setBuzzerTone(0);
  } else {
    setBuzzerTone(effect->hz[step]);
  }
}

// The white button light: on, off, on for 80 ms each after a hit on the player, whatever the
// "Button light" setting is. The light in the OK button: on while the iron fires.
void lights(TickType_t now) {
  const int32_t left = (int32_t)(flashUntil - now);
  hs02OverrideButtonLight(left > 0 ? ((((left - 1) / 80) & 1) ? 0 : 10) : 0xFF);
  setStatusLED(phase == Phase::Playing && (int32_t)(fireUntil - now) > 0 ? LED_HOT : LED_OFF);
}

// A colour at 8/8, 6/8, 4/8 or 3/8 of its brightness
uint16_t shade(uint16_t colour, uint8_t level) {
  static const uint8_t eighths[4] = {8, 6, 4, 3};
  const uint32_t       k          = eighths[level > 3 ? 3 : level];
  const uint32_t       r = ((colour >> 11) & 31) * k / 8, g = ((colour >> 5) & 63) * k / 8, b = (colour & 31) * k / 8;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

uint8_t texel(const uint8_t *sprite, uint8_t x, uint8_t y) { return (uint8_t)((sprite[y * 4 + (x >> 2)] >> ((x & 3) * 2)) & 3); }

void newGame() {
  const TickType_t now   = xTaskGetTickCount(); // "not running" for the ...Until times: 0 would be in the future after 25 days of uptime
  uint8_t          count = 0;
  blobsLeft     = 0;
  for (uint8_t y = 0; y < 16; y++) {
    walls[y] = 0;
    for (uint8_t x = 0; x < 16; x++) {
      const char c = kLevel[y][x];
      if (c == '#' || c == 'E') {
        walls[y] |= (uint16_t)(1u << x);
      }
      if (c == 'E') {
        exitX = x;
        exitY = y;
      } else if (c == 'P') {
        posX = x * kCell + kCell / 2;
        posY = y * kCell + kCell / 2;
      } else if ((c == 'b' || c == 'm') && count < kThingCount) {
        Thing &t     = things[count++];
        t.x          = (int16_t)(x * kCell + kCell / 2);
        t.y          = (int16_t)(y * kCell + kCell / 2);
        t.type       = c == 'b' ? kTypeBlob : kTypeKit;
        t.health     = 2;
        t.awake      = false;
        t.nextAttack = now;
        t.hurtUntil  = now;
        blobsLeft += c == 'b';
      }
    }
  }
  for (; count < kThingCount; count++) {
    things[count].type = kNothing;
  }
  angle     = 0; // facing east
  health    = 100;
  startedAt = lastFrame = now;
  fireUntil = hurtUntil = now;
  turnSince             = 0; // A2: a turn held when the last game ended is not carried over
  fireNow               = false;
  shownHealth           = -1;
  shownBlobs            = 0xFF;
  endDrawn              = false;
  phase                 = Phase::Playing;
  LCD::fillPanelRect(0, kViewH, 160, 80 - kViewH, kStrip);
}

// Moves the player, sliding along walls. Walking into the exit, once it is open, ends the game.
void walk(int32_t dx, int32_t dy) {
  constexpr int32_t kRadius = 50;
  int32_t           cx = (posX + dx + (dx > 0 ? kRadius : -kRadius)) >> 8, cy = posY >> 8;
  if (!isWall(cx, cy)) {
    posX += dx;
  } else if (cx == exitX && cy == exitY && blobsLeft == 0) {
    phase = Phase::Won;
  }
  cx = posX >> 8;
  cy = (posY + dy + (dy > 0 ? kRadius : -kRadius)) >> 8;
  if (!isWall(cx, cy)) {
    posY += dy;
  } else if (cx == exitX && cy == exitY && blobsLeft == 0) {
    phase = Phase::Won;
  }
}

bool clearLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  for (int32_t i = 1; i < 16; i++) {
    if (isWall((x0 + (x1 - x0) * i / 16) >> 8, (y0 + (y1 - y0) * i / 16) >> 8)) {
      return false;
    }
  }
  return true;
}

void think(TickType_t now, uint32_t elapsedMs) {
  for (uint8_t i = 0; i < kThingCount; i++) {
    Thing &t = things[i];
    if (t.type == kNothing) {
      continue;
    }
    const int32_t dx = posX - t.x, dy = posY - t.y;
    const int32_t ax = absolute(dx), ay = absolute(dy);
    const int32_t distance = (ax > ay ? ax + ay / 2 : ay + ax / 2) + 1; // near enough, and never 0
    if (t.type == kTypeKit) {
      if (distance < 150 && health < 100) {
        health = health > 70 ? 100 : health + 30;
        t.type = kNothing;
        sound(kKitTaken, now);
      }
      continue;
    }
    if (!t.awake) {
      t.awake = distance < 6 * kCell && clearLine(t.x, t.y, posX, posY);
      if (t.awake) {
        sound(kNoticed, now);
      }
      continue;
    }
    if (distance > 190) { // slide towards the player, 1.3 cells a second
      const int32_t step = (int32_t)(333 * elapsedMs / 1000);
      const int32_t sx = dx * step / distance, sy = dy * step / distance;
      if (!isWall((t.x + sx + (sx > 0 ? 70 : -70)) >> 8, t.y >> 8)) {
        t.x = (int16_t)(t.x + sx);
      }
      if (!isWall(t.x >> 8, (t.y + sy + (sy > 0 ? 70 : -70)) >> 8)) {
        t.y = (int16_t)(t.y + sy);
      }
    } else if ((int32_t)(now - t.nextAttack) >= 0 && clearLine(t.x, t.y, posX, posY)) { // not round a corner
      t.nextAttack = now + 700;
      health -= 10;
      hurtUntil  = now + 150;
      flashUntil = now + 240;
      sound(kHurt, now);
    }
  }
}

void drawStrip() {
  if (health != shownHealth) {
    shownHealth       = health;
    const uint8_t bar = (uint8_t)(health <= 0 ? 0 : health * 60 / 100);
    LCD::fillPanelRect(2, kViewH + 2, 60, 4, shade(kHealth, 3));
    LCD::fillPanelRect(2, kViewH + 2, bar, 4, kHealth);
  }
  if (blobsLeft != shownBlobs) {
    shownBlobs = blobsLeft;
    LCD::fillPanelRect(100, kViewH + 2, 58, 4, kStrip);
    for (uint8_t i = 0; i < blobsLeft && i < 7; i++) {
      LCD::fillPanelRect(152 - i * 8, kViewH + 2, 5, 4, kBlobMark);
    }
  }
}

void render(TickType_t now) {
  const uint8_t a    = (uint8_t)(angle >> 8);
  const int32_t dirX = cosine(a), dirY = sine(a);                  // Q8
  const int32_t planeX = -dirY * 169 / 256, planeY = dirX * 169 / 256; // 0.66 of the right-hand vector: about 67 degrees across

  // ---- where the things appear ----
  Seen    seen[kThingCount];
  uint8_t seenCount = 0;
  for (uint8_t i = 0; i < kThingCount; i++) {
    const Thing &t = things[i];
    if (t.type == kNothing) {
      continue;
    }
    const int32_t dx = t.x - posX, dy = t.y - posY;
    const int32_t depth = (dx * dirX + dy * dirY) >> 8; // along the view
    if (depth < 50) {
      continue; // behind, or in the player's face
    }
    const int32_t sideways = (-dx * dirY + dy * dirX) >> 8;
    const int32_t centre   = kCols / 2 + sideways * 61 / depth; // 61 = half the columns / 0.66
    const int32_t wallH    = kViewH * kCell / depth;             // height of a wall at that depth
    const int32_t size     = t.type == kTypeBlob ? wallH * 3 / 4 : wallH / 2;
    if (size < 2 || size > 400) {
      continue;
    }
    const int32_t half = size / 4 + 1; // in columns: the picture is square, and a column is two pixels
    if (centre + half < 0 || centre - half >= kCols) {
      continue;
    }
    Seen &s    = seen[seenCount++];
    s.firstCol = (int16_t)(centre - half);
    s.lastCol  = (int16_t)(centre + half);
    s.size     = (int16_t)size;
    s.top      = (int16_t)(kViewH / 2 + wallH / 2 - size); // standing on the floor
    s.depth    = depth;
    s.thing    = i;
    if (t.type == kTypeBlob) {
      s.sprite  = ((now / 300) & 1) ? kBlobA : kBlobB;
      s.palette = (int32_t)(t.hurtUntil - now) > 0 ? kBlobHurt : kBlobPalette;
    } else {
      s.sprite  = Hs02MazeArt::kKit;
      s.palette = kKitPalette;
    }
  }
  for (uint8_t i = 1; i < seenCount; i++) { // far ones first
    const Seen s = seen[i];
    int8_t     j = (int8_t)i - 1;
    for (; j >= 0 && seen[j].depth < s.depth; j--) {
      seen[j + 1] = seen[j];
    }
    seen[j + 1] = s;
  }

  const bool      hurt    = (int32_t)(hurtUntil - now) > 0;
  const bool      firing  = (int32_t)(fireUntil - now) > 0;
  const uint8_t  *iron    = firing ? kIronHot : kIron;
  const uint16_t *ironPal = firing ? kIronHotPal : kIronPalette;
  static uint16_t column[kViewH]; // static: the GUI thread's stack is small
  // ceiling and floor are darker towards the horizon, in three bands of twelve rows
  uint16_t ceilings[3], floors[3];
  for (uint8_t band = 0; band < 3; band++) {
    ceilings[band] = hurt ? kHurtTint : shade(kCeiling, band);
    floors[band]   = hurt ? kHurtTint : shade(kFloor, (uint8_t)(2 - band));
  }

  for (uint8_t c = 0; c < kCols; c++) {
    // ---- the wall this column's ray meets ----
    const int32_t camera = ((2 * c + 1 - kCols) * 256) / kCols; // -1 .. 1 across the screen, Q8
    const int32_t rayX = dirX + ((planeX * camera) >> 8), rayY = dirY + ((planeY * camera) >> 8);
    int32_t       cellX = posX >> 8, cellY = posY >> 8;
    const int32_t perX = rayX == 0 ? 0x100000 : 65536 / absolute(rayX), perY = rayY == 0 ? 0x100000 : 65536 / absolute(rayY);
    int32_t       nextX = (rayX < 0 ? (posX & 255) : 256 - (posX & 255)) * perX >> 8;
    int32_t       nextY = (rayY < 0 ? (posY & 255) : 256 - (posY & 255)) * perY >> 8;
    bool          sideY = false;
    for (uint8_t steps = 0; steps < 40; steps++) {
      if (nextX < nextY) {
        nextX += perX;
        cellX += rayX < 0 ? -1 : 1;
        sideY = false;
      } else {
        nextY += perY;
        cellY += rayY < 0 ? -1 : 1;
        sideY = true;
      }
      if (isWall(cellX, cellY)) {
        break;
      }
    }
    int32_t distance = sideY ? nextY - perY : nextX - perX;
    if (distance < 24) {
      distance = 24;
    }
    wallDistance[c]   = (uint16_t)(distance > 65535 ? 65535 : distance);
    const int32_t tall = kViewH * kCell / distance;
    const int32_t top  = tall >= kViewH ? 0 : (kViewH - tall) / 2;
    const int32_t end  = tall >= kViewH ? kViewH : top + tall;
    uint16_t      wall, joint; // the bricks and the lines between them
    if (cellX == exitX && cellY == exitY) {
      wall = joint = blobsLeft ? kExitShut : shade(kExit, (uint8_t)((now / 250) & 1)); // shut: red. Open: green, pulsing
    } else {
      const uint16_t base  = kWallColours[(uint8_t)(cellX * 3 + cellY * 5) % 3];
      const uint8_t  level = (uint8_t)((distance > 5 * kCell ? 2 : distance > 2 * kCell ? 1 : 0) + (sideY ? 1 : 0));
      wall                 = shade(base, level);
      joint                = tall < 20 ? wall : shade(base, (uint8_t)(level + 1)); // too small to see far away
    }
    // Bricks: four courses up the wall, a joint every half cell along it, shifted by half a
    // brick on every other course. along and up are 0..255 across one wall face.
    const int32_t along  = (sideY ? posX + ((distance * rayX) >> 8) : posY + ((distance * rayY) >> 8)) & 255;
    const int32_t upStep = 65536 / tall;                              // Q8, per display row
    int32_t       upQ8   = (top - (kViewH - tall) / 2) * upStep;      // the wall's own top can be above the view
    for (int32_t y = 0; y < kViewH; y++) {
      if (y < top) {
        column[y] = ceilings[y / 12];
      } else if (y < end) {
        const int32_t up = (upQ8 >> 8) & 255;
        upQ8 += upStep;
        column[y] = ((up & 63) < 8 || ((along + ((up >> 6) & 1) * 64) & 127) < 10) ? joint : wall;
      } else {
        column[y] = floors[(y - kViewH / 2) / 12];
      }
    }

    // ---- the things in front of that wall ----
    for (uint8_t i = 0; i < seenCount; i++) {
      const Seen &s = seen[i];
      if (c < s.firstCol || c >= s.lastCol || s.depth >= distance) {
        continue;
      }
      const uint8_t tx = (uint8_t)((c - s.firstCol) * 16 / (s.lastCol - s.firstCol));
      for (int32_t y = s.top < 0 ? 0 : s.top; y < s.top + s.size && y < kViewH; y++) {
        const uint8_t pixel = texel(s.sprite, tx, (uint8_t)((y - s.top) * 16 / s.size));
        if (pixel) {
          column[y] = s.palette[pixel];
        }
      }
    }

    // ---- the player's iron: 16 columns wide in the middle, 32 rows high at the bottom ----
    if (c >= kCols / 2 - 8 && c < kCols / 2 + 8) {
      const uint8_t tx = (uint8_t)(c - (kCols / 2 - 8));
      for (uint8_t y = 0; y < 32; y++) {
        const uint8_t pixel = texel(iron, tx, y / 2);
        if (pixel) {
          column[kViewH - 32 + y] = ironPal[pixel];
        }
      }
    }

    // ---- to the panel: the column twice, side by side, colours inverted as refreshColor() does ----
    for (uint8_t y = 0; y < kViewH; y++) {
      const uint16_t sent     = (uint16_t)~column[y];
      wire[y * 2]             = wire[kViewH * 2 + y * 2] = (uint8_t)(sent >> 8);
      wire[y * 2 + 1]         = wire[kViewH * 2 + y * 2 + 1] = (uint8_t)(sent & 0xFF);
    }
    LCD::drawNativeImage(c * 2, 0, 2, kViewH, wire);
  }

  // ---- a shot: the nearest blob in the middle of the view (or within three columns of it:
  // aiming with two buttons is coarse), if no wall is nearer ----
  if (fireNow) {
    fireNow = false;
    for (int8_t i = (int8_t)seenCount - 1; i >= 0; i--) { // nearest is last
      const Seen &s = seen[i];
      Thing      &t = things[s.thing];
      if (t.type == kTypeBlob && s.firstCol - 3 <= kCols / 2 && s.lastCol + 3 > kCols / 2 && s.depth < wallDistance[kCols / 2]) {
        t.hurtUntil = now + 150;
        t.awake     = true;
        if (--t.health == 0) {
          t.type = kNothing;
          blobsLeft--;
          sound(blobsLeft ? kMelted : kExitOpen, now);
        } else {
          sound(kHit, now);
        }
        break;
      }
    }
  }
}

// The end screens: the whole panel in one colour, with the time taken, in seconds, in the middle.
void drawEnd(bool won) {
  const uint16_t back = won ? 0x0320 : 0x6000;
  LCD::fillPanelRect(0, 0, 160, 80, back);
  static const uint8_t digits[10][5] = {{7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
                                        {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};
  uint32_t             seconds       = (endedAt - startedAt) / TICKS_SECOND;
  if (seconds > 999) {
    seconds = 999;
  }
  const uint8_t value[3] = {(uint8_t)(seconds / 100), (uint8_t)(seconds / 10 % 10), (uint8_t)(seconds % 10)};
  for (uint8_t d = 0; d < 3; d++) {
    for (uint8_t row = 0; row < 5; row++) {
      for (uint8_t col = 0; col < 3; col++) {
        if (digits[value[d]][row] & (4 >> col)) {
          LCD::fillPanelRect(44 + d * 26 + col * 6, 25 + row * 6, 6, 6, 0xFFFF);
        }
      }
    }
  }
}

OperatingMode leave(guiContext *cxt) {
  effect = nullptr;
  setBuzzerTone(0);
  hs02OverrideButtonLight(0xFF); // back to the setting
  setStatusLED(LED_OFF);         // guiHandleDraw() takes it over again
  LCD::forceNextRefresh();       // the menu's frame must repaint the whole panel
  hs02MenuWaitForRelease = true; // buttons are still down
  cxt->transitionMode    = TransitionAnimation::None;
  return OperatingMode::SettingsMenu;
}
} // namespace

// One frame of the game; called by guiHandleDraw() while the mode is Hs02Maze.
OperatingMode hs02MazeFrame(const ButtonState buttons, guiContext *cxt) {
  (void)buttons;
  const TickType_t now   = xTaskGetTickCount();
  const bool       left  = getButtonA() != 0; // DOWN
  const bool       right = getButtonB() != 0; // UP
  const bool       ok    = getButtonOK() != 0;

  // A fresh entry (the mode's scratch state was cleared): wait until OK, which opened the game,
  // is up, then start.
  if (cxt->scratch_state.state1 == 0) {
    cxt->scratch_state.state1 = 1;
    phase                     = Phase::Start;
    bothSince                 = 0;
    flashUntil                = now;
    effect                    = nullptr;
    setBuzzerTone(0);
    lights(now);
    LCD::fillPanelRect(0, 0, 160, 80, kStrip);
    return OperatingMode::Hs02Maze;
  }
  soundTick(now); // in every phase, or an effect would hang while a button is held on the end screen
  lights(now);
  if (phase == Phase::Start) {
    if (!ok && !left && !right) {
      wasOk = false;
      newGame();
    }
    return OperatingMode::Hs02Maze;
  }

  // DOWN and UP together for a second: leave
  if (left && right) {
    if (bothSince == 0) {
      bothSince = now | 1;
    } else if ((now - bothSince) >= TICKS_SECOND) {
      return leave(cxt);
    }
  } else {
    bothSince = 0;
  }

  if (phase != Phase::Playing) {
    if (!endDrawn) {
      endDrawn = true;
      drawEnd(phase == Phase::Won);
      effect = nullptr;
      sound(phase == Phase::Won ? kWon : kLost, now);
    }
    if (ok && !wasOk && (now - endedAt) > TICKS_SECOND / 2) {
      phase = Phase::Start; // the game starts when OK is let go, so that press neither walks nor fires
    }
    wasOk = ok;
    return OperatingMode::Hs02Maze;
  }

  uint32_t elapsed = now - lastFrame;
  lastFrame        = now;
  if (elapsed > 100) {
    elapsed = 100;
  }
  // turning: 72 degrees a second for the first 200 ms, for aiming, then 144 degrees a second
  if (left != right) {
    if (turnSince == 0) {
      turnSince = now | 1;
    }
    const uint32_t rate = (int32_t)(now - turnSince) < 200 ? 51 : 102; // 256 = a full turn a second (turnSince can be a tick ahead)
    const uint16_t turn = (uint16_t)(rate * 256 * elapsed / 1000);
    angle               = (uint16_t)(left ? angle - turn : angle + turn);
  } else {
    turnSince = 0;
  }
  // OK: a tap fires, a hold walks
  if (ok && !wasOk) {
    okSince = now;
  }
  if (ok && (now - okSince) >= 160) { // 2.5 cells a second
    const uint8_t a    = (uint8_t)(angle >> 8);
    const int32_t step = (int32_t)(640 * elapsed / 1000);
    walk(cosine(a) * step / 256, sine(a) * step / 256);
  }
  if (!ok && wasOk && (now - okSince) < 250) {
    fireNow   = true;
    fireUntil = now + 120;
    sound(kShot, now);
  }
  wasOk = ok;

  think(now, elapsed);
  if (health <= 0) {
    health = 0;
    if (phase == Phase::Playing) { // not if the exit was reached in this very frame
      phase = Phase::Lost;
    }
  }
  if (phase != Phase::Playing) {
    endedAt = now;
    return OperatingMode::Hs02Maze;
  }
  render(now);
  drawStrip();
  return OperatingMode::Hs02Maze;
}
#endif
