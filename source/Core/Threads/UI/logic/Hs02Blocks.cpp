// HS-02 card theme: the "Super High Intensity Block Simulator", a small falling-block game on
// the upright screen. Opened from Advanced settings. The iron is held upright, tip down, and
// the picture is drawn turned a quarter: the 160 pixel side of the display is the height of
// the game.
//
// Pieces of two to four cells fall into a field of 8 x 15 cells. A full row disappears and
// counts one point; the pieces fall faster, and the music plays faster, as the score grows.
// The game ends when a new piece has no room.
//
//   UP            piece one cell to the left   (held: repeats)
//   DOWN          piece one cell to the right  (held: repeats)
//   OK            turn the piece a quarter
//   OK held       for a second: leave the game
//   after the end: OK plays again
//
// The buttons are read directly (BSP), not through the menu's short / long press decoding,
// which only reports a press when the button is let go.
//
// Drawing. Each of the eight shapes has a colour of its own, which is more than the colour
// screen's four-colour pictures allow. So the game does not use the frame buffer at all: it
// paints rectangles straight onto the panel (LCD::fillPanelRect) and only repaints what has
// changed since the frame before. guiRenderLoop() does not send the frame buffer while this
// mode is active.
#include "BSP.h"
#include "Display.hpp"
#include "LCD.hpp"
#include "OperatingModes.h"

#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
void        setBuzzerTone(uint16_t hz); // Core/BSP/Fnirsi/BSP.cpp; silent when the buzzer setting is off
extern bool hs02MenuWaitForRelease;     // SettingsMenu.cpp: the card menu ignores buttons until all are up
// utils/Hs02Tune.cpp
void    hs02TuneStart(uint8_t tune);
void    hs02TuneStop(void);
bool    hs02TunePlaying(void);
uint8_t hs02TuneCount(void);
void    hs02TuneSetTempo(uint8_t percent);

namespace {
constexpr uint8_t kCols = 8, kRows = 15, kCell = 9; // 9 pixel pitch, 8 pixel blocks
constexpr uint8_t kFieldX = 4, kFieldY = 23;         // upright pixels: the field is 72 wide, 135 high

// True RGB565 colours. 0 is the background; 1 to 8 belong to the eight shapes, in the order of
// kShapes: blue, green, red, orange, yellow, purple, pink, white.
const uint16_t     kColours[9] = {0x0042, 0x3DFF, 0x262B, 0xEA28, 0xF4E1, 0xFE62, 0xA45F, 0xF396, 0xFFFF};
constexpr uint16_t kWall = 0x9D37, kScore = 0xFFFF;

struct Shape {
  uint8_t count;
  int8_t  cells[4][2]; // x, y
};
const Shape kShapes[] = {
    {2, {{0, 0}, {1, 0}, {0, 0}, {0, 0}}}, // pair
    {3, {{0, 0}, {1, 0}, {2, 0}, {0, 0}}}, // bar of three
    {3, {{0, 0}, {1, 0}, {0, 1}, {0, 0}}}, // corner
    {4, {{0, 0}, {1, 0}, {0, 1}, {1, 1}}}, // square
    {4, {{0, 0}, {1, 0}, {2, 0}, {1, 1}}}, // T
    {4, {{0, 0}, {1, 0}, {2, 0}, {0, 1}}}, // L
    {4, {{0, 0}, {1, 0}, {2, 0}, {2, 1}}}, // L, mirrored
    {4, {{0, 0}, {1, 0}, {2, 0}, {3, 0}}}, // bar of four
};
constexpr uint8_t kShapeCount = sizeof(kShapes) / sizeof(kShapes[0]);

// Digits 0-9, 3 x 5, one bit per pixel, top row first, most significant bit left
const uint8_t kDigits[10][5] = {{7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};

// The "sad horn" played when the game is lost: four falling notes, the last one long.
struct HornNote {
  uint16_t hz, ms;
};
const HornNote    kHorn[]    = {{466, 280}, {440, 280}, {415, 280}, {392, 850}};
constexpr uint8_t kHornNotes = sizeof(kHorn) / sizeof(kHorn[0]);

enum class Phase : uint8_t { Start, Playing, Over };
Phase      phase = Phase::Start;
uint32_t   field[kRows]; // four bits per cell (0 = empty, 1 to 8 = colour), cell 0 = left column
Shape      piece;        // the falling piece, as it is turned now
uint8_t    pieceColour;
int8_t     pieceX, pieceY;
uint8_t    nextShape;
uint16_t   score;
uint32_t   seed;
TickType_t fallAt, repeatAt, okSince, hornEnds;
bool       wasLeft, wasRight, wasOk;
uint8_t    hornStep; // 0: silent, 1..4: that note of kHorn is sounding
// what is on the panel now
uint8_t  shownCell[kRows][kCols];
uint16_t shownScore;
uint8_t  shownNext;
bool     shownScoreVisible;

uint8_t cellAt(uint8_t column, uint8_t row) { return (uint8_t)((field[row] >> (column * 4)) & 15); }

uint8_t randomShape() {
  seed = seed * 1664525u + 1013904223u;
  return (uint8_t)((seed >> 24) % kShapeCount);
}

bool fits(const Shape &shape, int8_t x, int8_t y) {
  for (uint8_t i = 0; i < shape.count; i++) {
    const int8_t cx = x + shape.cells[i][0], cy = y + shape.cells[i][1];
    if (cx < 0 || cx >= kCols || cy < 0 || cy >= kRows || cellAt(cx, cy)) {
      return false;
    }
  }
  return true;
}

// A quarter turn: (x, y) -> (-y, x), moved back so that the smallest x and y are 0
Shape turned(const Shape &shape) {
  Shape  result = shape;
  int8_t lowX = 127, lowY = 127;
  for (uint8_t i = 0; i < shape.count; i++) {
    result.cells[i][0] = -shape.cells[i][1];
    result.cells[i][1] = shape.cells[i][0];
    lowX               = result.cells[i][0] < lowX ? result.cells[i][0] : lowX;
    lowY               = result.cells[i][1] < lowY ? result.cells[i][1] : lowY;
  }
  for (uint8_t i = 0; i < shape.count; i++) {
    result.cells[i][0] -= lowX;
    result.cells[i][1] -= lowY;
  }
  return result;
}

bool spawn() {
  piece       = kShapes[nextShape];
  pieceColour = (uint8_t)(nextShape + 1);
  nextShape   = randomShape();
  pieceX      = (kCols - 2) / 2;
  pieceY      = 0;
  return fits(piece, pieceX, pieceY);
}

void newGame() {
  for (uint8_t r = 0; r < kRows; r++) {
    field[r] = 0;
  }
  score     = 0;
  seed      = xTaskGetTickCount() * 2654435761u + 1;
  nextShape = randomShape();
  spawn();
  fallAt   = xTaskGetTickCount() + 600;
  hornStep = 0;
  setBuzzerTone(0);
  phase = Phase::Playing;
}

bool rowFull(uint8_t row) {
  for (uint8_t c = 0; c < kCols; c++) {
    if (cellAt(c, row) == 0) {
      return false;
    }
  }
  return true;
}

void lockPiece() {
  for (uint8_t i = 0; i < piece.count; i++) {
    field[pieceY + piece.cells[i][1]] |= (uint32_t)pieceColour << ((pieceX + piece.cells[i][0]) * 4);
  }
  for (int8_t r = kRows - 1; r >= 0; r--) {
    if (rowFull(r)) {
      for (int8_t above = r; above > 0; above--) {
        field[above] = field[above - 1];
      }
      field[0] = 0;
      score++;
      r++; // the row that moved down is looked at next
    }
  }
  if (!spawn()) {
    phase = Phase::Over;
    hs02TuneStop();
    hornStep = 1;
    hornEnds = xTaskGetTickCount() + kHorn[0].ms;
    setBuzzerTone(kHorn[0].hz);
  }
}

// The music: the last tune of the table, round and round, faster as the score grows.
void music() {
  if (!hs02TunePlaying()) {
    hs02TuneStart((uint8_t)(hs02TuneCount() - 1));
  }
  hs02TuneSetTempo((uint8_t)(100 + (score > 25 ? 25 : score) * 4));
}

void horn(TickType_t now) {
  if (hornStep == 0) {
    return;
  }
  if ((int32_t)(now - hornEnds) >= 0) {
    if (hornStep >= kHornNotes) {
      hornStep = 0;
      setBuzzerTone(0);
      return;
    }
    hornEnds = now + kHorn[hornStep].ms;
    setBuzzerTone(kHorn[hornStep].hz);
    hornStep++;
  } else if (hornStep == kHornNotes) { // the last note wobbles
    setBuzzerTone(((now / 80) & 1) ? kHorn[kHornNotes - 1].hz : (uint16_t)(kHorn[kHornNotes - 1].hz - 12));
  }
}

// ---- drawing: upright coordinates (x across 0..79, y down 0..159) onto the lying panel --------
// The tip end of the iron is the left end of the picture in right-handed orientation and the
// right end in left-handed orientation, so "down" in the game is always towards the tip.
void block(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t rgb565) {
  if (Display::getRotation()) { // left-handed
    LCD::fillPanelRect(y, 80 - x - width, height, width, rgb565);
  } else {
    LCD::fillPanelRect(160 - y - height, x, height, width, rgb565);
  }
}

void drawNumber(uint16_t value, uint8_t x, uint8_t y) {
  const uint8_t digits[3] = {(uint8_t)(value / 100 % 10), (uint8_t)(value / 10 % 10), (uint8_t)(value % 10)};
  for (uint8_t d = 0; d < 3; d++) {
    for (uint8_t row = 0; row < 5; row++) {
      for (uint8_t col = 0; col < 3; col++) {
        if (kDigits[digits[d]][row] & (4 >> col)) {
          block(x + d * 12 + col * 3, y + row * 3, 3, 3, kScore);
        }
      }
    }
  }
}

// Everything, from an empty panel: used when the game is opened.
void drawAll() {
  LCD::fillPanelRect(0, 0, 160, 80, kColours[0]);
  block(kFieldX - 2, kFieldY - 1, 1, kRows * kCell + 1, kWall); // the walls and the floor of the field
  block(kFieldX + kCols * kCell, kFieldY - 1, 1, kRows * kCell + 1, kWall);
  block(kFieldX - 2, kFieldY + kRows * kCell, kCols * kCell + 3, 1, kWall);
  for (uint8_t r = 0; r < kRows; r++) {
    for (uint8_t c = 0; c < kCols; c++) {
      shownCell[r][c] = 0;
    }
  }
  shownScore        = 0xFFFF;
  shownNext         = 0xFF;
  shownScoreVisible = false;
}

// Only what differs from what the panel shows.
void drawChanges(bool scoreVisible) {
  for (uint8_t r = 0; r < kRows; r++) {
    for (uint8_t c = 0; c < kCols; c++) {
      uint8_t wanted = cellAt(c, r);
      if (phase == Phase::Playing) {
        for (uint8_t i = 0; i < piece.count; i++) {
          if (pieceX + piece.cells[i][0] == c && pieceY + piece.cells[i][1] == r) {
            wanted = pieceColour;
          }
        }
      }
      if (wanted != shownCell[r][c]) {
        shownCell[r][c] = wanted;
        block(kFieldX + c * kCell, kFieldY + r * kCell, kCell - 1, kCell - 1, kColours[wanted]);
      }
    }
  }
  if (score != shownScore || scoreVisible != shownScoreVisible) {
    shownScore        = score;
    shownScoreVisible = scoreVisible;
    block(4, 3, 36, 15, kColours[0]);
    if (scoreVisible) {
      drawNumber(score, 4, 3);
    }
  }
  if (nextShape != shownNext) { // the piece that comes next, small, top right
    shownNext = nextShape;
    block(54, 5, 20, 10, kColours[0]);
    const Shape &next = kShapes[nextShape];
    for (uint8_t i = 0; i < next.count; i++) {
      block(54 + next.cells[i][0] * 5, 5 + next.cells[i][1] * 5, 4, 4, kColours[nextShape + 1]);
    }
  }
}

OperatingMode leave(guiContext *cxt) {
  hs02TuneStop();
  hornStep = 0;
  setBuzzerTone(0);
  LCD::forceNextRefresh();       // the menu's frame must repaint the whole panel
  hs02MenuWaitForRelease = true; // OK is still down
  cxt->transitionMode    = TransitionAnimation::None;
  return OperatingMode::SettingsMenu;
}
} // namespace

// One frame of the game; called by guiHandleDraw() while the mode is Hs02Blocks.
OperatingMode hs02BlocksFrame(const ButtonState buttons, guiContext *cxt) {
  (void)buttons;
  const TickType_t now   = xTaskGetTickCount();
  const bool       right = getButtonA() != 0; // DOWN
  const bool       left  = getButtonB() != 0; // UP
  const bool       ok    = getButtonOK() != 0;

  // A fresh entry (the mode's scratch state was cleared): paint the empty game, then wait until
  // OK, which opened the game, is up, and start.
  if (cxt->scratch_state.state1 == 0) {
    cxt->scratch_state.state1 = 1;
    phase                     = Phase::Start;
    hornStep                  = 0;
    for (uint8_t r = 0; r < kRows; r++) {
      field[r] = 0;
    }
    score     = 0;
    seed      = now * 2654435761u + 1;
    nextShape = randomShape();
    drawAll();
    return OperatingMode::Hs02Blocks;
  }
  if (phase == Phase::Start) {
    if (!ok && !left && !right) {
      wasLeft = wasRight = wasOk = false;
      newGame();
    }
    drawChanges(false);
    return OperatingMode::Hs02Blocks;
  }

  // OK held for a second: leave. (The press that starts the hold has already turned the piece,
  // or started a new game, by then.)
  if (ok && !wasOk) {
    okSince = now;
  }
  if (ok && wasOk && (now - okSince) >= TICKS_SECOND) {
    return leave(cxt);
  }

  if (phase == Phase::Over) {
    horn(now);
    if (ok && !wasOk) {
      newGame();
    }
    wasOk = ok;
    drawChanges(phase == Phase::Playing || ((now / 400) & 1) != 0); // after the end the score blinks
    return OperatingMode::Hs02Blocks;
  }
  music();

  // sideways: at once on a press, then repeating after a quarter of a second
  if (left != right) {
    const bool fresh = left ? !wasLeft : !wasRight;
    if (fresh || (int32_t)(now - repeatAt) >= 0) {
      const int8_t step = left ? -1 : 1;
      if (fits(piece, pieceX + step, pieceY)) {
        pieceX += step;
      }
      repeatAt = now + (fresh ? 250 : 80);
    }
  }
  // OK: a quarter turn when it goes down
  if (ok && !wasOk) {
    const Shape  candidate = turned(piece);
    const int8_t nudges[]  = {0, -1, 1, -2};
    for (uint8_t i = 0; i < 4; i++) {
      if (fits(candidate, pieceX + nudges[i], pieceY)) {
        piece = candidate;
        pieceX += nudges[i];
        break;
      }
    }
  }
  wasLeft  = left;
  wasRight = right;
  wasOk    = ok;

  // falling: every 600 ms at first, 20 ms quicker per point, not quicker than 120 ms
  if ((int32_t)(now - fallAt) >= 0) {
    if (fits(piece, pieceX, pieceY + 1)) {
      pieceY++;
    } else {
      lockPiece();
    }
    const uint32_t interval = score >= 24 ? 120 : 600 - score * 20;
    fallAt                  = now + interval;
  }
  drawChanges(true);
  return OperatingMode::Hs02Blocks;
}
#endif
