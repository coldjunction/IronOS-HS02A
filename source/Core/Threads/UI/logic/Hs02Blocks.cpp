// HS-02 card theme: "Block drop", a small falling-block game on the upright screen.
// Opened from Advanced settings. The iron is held upright, tip down, and the picture is drawn
// turned a quarter: the 160 pixel side of the display is the height of the game.
//
// Pieces of two to four cells fall into a field of 8 x 15 cells. A full row disappears and
// counts one point; the pieces fall faster as the score grows. The game ends when a new piece
// has no room.
//
//   UP            piece one cell to the left   (held: repeats)
//   DOWN          piece one cell to the right  (held: repeats)
//   OK            turn the piece a quarter
//   OK held       the piece falls fast
//   UP + DOWN     held for a second: leave the game
//   after the end: OK plays again
//
// The buttons are read directly (BSP), not through the menu's short / long press decoding,
// which only reports a press when the button is let go.
#include "BSP.h"
#include "Display.hpp"
#include "LCD.hpp"
#include "OperatingModes.h"

#if defined(MODEL_HS02) && defined(HS02_THEME_CARD)
void        setBuzzerTone(uint16_t hz); // Core/BSP/Fnirsi/BSP.cpp; silent when the buzzer setting is off
extern bool hs02MenuWaitForRelease;     // SettingsMenu.cpp: the card menu ignores buttons until all are up

namespace {
constexpr uint8_t kCols = 8, kRows = 15, kCell = 9; // 9 pixel pitch, 8 pixel blocks
constexpr uint8_t kFieldX = 4, kFieldY = 23;         // upright pixels: the field is 72 wide, 135 high
constexpr uint8_t kHeader = 20;                      // upright pixels above the field: score, next piece

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

enum class Phase : uint8_t { Start, Playing, Over };
Phase      phase = Phase::Start;
uint8_t    field[kRows];  // one byte per row, bit 0 = left column
Shape      piece;         // the falling piece, as it is turned now
int8_t     pieceX, pieceY;
uint8_t    nextShape;
uint16_t   score;
uint32_t   seed;
TickType_t fallAt, repeatAt, okSince, bothSince;
bool       wasLeft, wasRight, wasOk, okNeedsRelease; // okNeedsRelease: OK was down when a piece landed
uint8_t    beepFrames;

uint8_t randomShape() {
  seed = seed * 1664525u + 1013904223u;
  return (uint8_t)((seed >> 24) % kShapeCount);
}

bool fits(const Shape &shape, int8_t x, int8_t y) {
  for (uint8_t i = 0; i < shape.count; i++) {
    const int8_t cx = x + shape.cells[i][0], cy = y + shape.cells[i][1];
    if (cx < 0 || cx >= kCols || cy < 0 || cy >= kRows || (field[cy] & (1 << cx))) {
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

void beep(uint16_t hz, uint8_t frames) {
  setBuzzerTone(hz);
  beepFrames = frames;
}

bool spawn() {
  piece     = kShapes[nextShape];
  nextShape = randomShape();
  pieceX    = (kCols - 2) / 2;
  pieceY    = 0;
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
  fallAt = xTaskGetTickCount() + 600;
  phase  = Phase::Playing;
}

void lockPiece() {
  for (uint8_t i = 0; i < piece.count; i++) {
    field[pieceY + piece.cells[i][1]] |= (uint8_t)(1 << (pieceX + piece.cells[i][0]));
  }
  uint8_t cleared = 0;
  for (int8_t r = kRows - 1; r >= 0; r--) {
    if (field[r] == (uint8_t)((1 << kCols) - 1)) {
      for (int8_t above = r; above > 0; above--) {
        field[above] = field[above - 1];
      }
      field[0] = 0;
      cleared++;
      r++; // the row that moved down is looked at next
    }
  }
  if (cleared) {
    score += cleared;
    beep(1568, 3);
  }
  okNeedsRelease = true; // a held OK does not carry over to the next piece
  if (!spawn()) {
    phase = Phase::Over;
    beep(392, 8);
  }
}

// ---- drawing: upright coordinates (x across 0..79, y down 0..159) onto the lying display ------
// The tip end of the iron is the left end of the picture in right-handed orientation and the
// right end in left-handed orientation, so "down" in the game is always towards the tip.
void block(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
  if (Display::getRotation()) { // left-handed
    Display::drawFilledRect(y, 79 - (x + width - 1), y + height - 1, 79 - x, false);
  } else {
    Display::drawFilledRect(159 - (y + height - 1), x, 159 - y, x + width - 1, false);
  }
}

void drawNumber(uint16_t value, uint8_t x, uint8_t y) {
  const uint8_t digits[3] = {(uint8_t)(value / 100 % 10), (uint8_t)(value / 10 % 10), (uint8_t)(value % 10)};
  for (uint8_t d = 0; d < 3; d++) {
    for (uint8_t row = 0; row < 5; row++) {
      for (uint8_t col = 0; col < 3; col++) {
        if (kDigits[digits[d]][row] & (4 >> col)) {
          block(x + d * 12 + col * 3, y + row * 3, 3, 3);
        }
      }
    }
  }
}

void draw(bool showScore) {
  if (showScore) {
    drawNumber(score, 4, 3);
  }
  const Shape &next = kShapes[nextShape]; // the piece that comes next, small, top right
  for (uint8_t i = 0; i < next.count; i++) {
    block(54 + next.cells[i][0] * 5, 5 + next.cells[i][1] * 5, 4, 4);
  }
  // the walls and the floor of the field
  block(kFieldX - 2, kFieldY - 1, 1, kRows * kCell + 1);
  block(kFieldX + kCols * kCell, kFieldY - 1, 1, kRows * kCell + 1);
  block(kFieldX - 2, kFieldY + kRows * kCell, kCols * kCell + 3, 1);
  for (uint8_t r = 0; r < kRows; r++) {
    for (uint8_t c = 0; c < kCols; c++) {
      if (field[r] & (1 << c)) {
        block(kFieldX + c * kCell, kFieldY + r * kCell, kCell - 1, kCell - 1);
      }
    }
  }
  if (phase == Phase::Playing) {
    for (uint8_t i = 0; i < piece.count; i++) {
      block(kFieldX + (pieceX + piece.cells[i][0]) * kCell, kFieldY + (pieceY + piece.cells[i][1]) * kCell, kCell - 1, kCell - 1);
    }
  }
  // The strip with the score takes the accent colour. The colour regions are whole display
  // columns, counted from the left, so this only works when the strip is at the right end.
  LCD::setMonoRegions(0, 0, Display::getRotation() ? 255 : 160 - kHeader, 255);
}
} // namespace

// One frame of the game; called by guiHandleDraw() while the mode is Hs02Blocks.
OperatingMode hs02BlocksFrame(const ButtonState buttons, guiContext *cxt) {
  (void)buttons;
  const TickType_t now   = xTaskGetTickCount();
  const bool       right = getButtonA() != 0; // DOWN
  const bool       left  = getButtonB() != 0; // UP
  const bool       ok    = getButtonOK() != 0;

  if (beepFrames && --beepFrames == 0) {
    setBuzzerTone(0);
  }
  // A fresh entry (the mode's scratch state was cleared): wait until OK, which opened the game,
  // is up, then start.
  if (cxt->scratch_state.state1 == 0) {
    phase = Phase::Start;
    if (!ok && !left && !right) {
      cxt->scratch_state.state1 = 1;
      wasLeft = wasRight = wasOk = okNeedsRelease = false;
      bothSince                             = 0;
      newGame();
    }
    draw(false);
    return OperatingMode::Hs02Blocks;
  }

  // UP and DOWN together for a second: leave
  if (left && right) {
    if (bothSince == 0) {
      bothSince = now | 1;
    } else if ((now - bothSince) >= TICKS_SECOND) {
      setBuzzerTone(0);
      beepFrames             = 0;
      hs02MenuWaitForRelease = true; // both buttons are still down
      cxt->transitionMode    = TransitionAnimation::None;
      return OperatingMode::SettingsMenu;
    }
  } else {
    bothSince = 0;
  }

  if (phase == Phase::Over) {
    if (ok && !wasOk) {
      newGame();
    }
    wasOk = ok;
    draw(((now / 400) & 1) != 0); // the score blinks
    return OperatingMode::Hs02Blocks;
  }

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
  // OK: a quarter turn when it goes down; held for longer it drops the piece fast
  if (ok && !wasOk) {
    okSince            = now;
    const Shape    candidate = turned(piece);
    const int8_t   nudges[]  = {0, -1, 1, -2};
    for (uint8_t i = 0; i < 4; i++) {
      if (fits(candidate, pieceX + nudges[i], pieceY)) {
        piece = candidate;
        pieceX += nudges[i];
        break;
      }
    }
  }
  if (!ok) {
    okNeedsRelease = false;
  }
  const bool fast = ok && !okNeedsRelease && (now - okSince) >= 300;
  wasLeft         = left;
  wasRight        = right;
  wasOk           = ok;

  // falling: every 600 ms at first, 20 ms quicker per point, not quicker than 120 ms
  if (fast || (int32_t)(now - fallAt) >= 0) {
    if (fits(piece, pieceX, pieceY + 1)) {
      pieceY++;
    } else {
      lockPiece();
    }
    const uint32_t interval = score >= 24 ? 120 : 600 - score * 20;
    fallAt                  = now + interval;
  }
  draw(true);
  return OperatingMode::Hs02Blocks;
}
#endif
