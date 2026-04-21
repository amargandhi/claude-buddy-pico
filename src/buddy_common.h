#pragma once

#include <cstdint>

// Shared constants and helpers for buddy species files.
// Each species file (src/buddies/<name>.cpp) includes this header
// and defines its 7 state functions.

extern const int BUDDY_X_CENTER;
extern const int BUDDY_CANVAS_W;
extern const int BUDDY_Y_BASE;
extern const int BUDDY_Y_OVERLAY;
extern const int BUDDY_CHAR_W;
extern const int BUDDY_CHAR_H;

extern const uint16_t BUDDY_BG;
extern const uint16_t BUDDY_HEART;
extern const uint16_t BUDDY_DIM;
extern const uint16_t BUDDY_YEL;
extern const uint16_t BUDDY_WHITE;
extern const uint16_t BUDDY_CYAN;
extern const uint16_t BUDDY_GREEN;
extern const uint16_t BUDDY_PURPLE;
extern const uint16_t BUDDY_RED;
extern const uint16_t BUDDY_BLUE;

void buddyPrintLine(const char* line, int y_px, uint16_t color, int x_off = 0);
void buddyPrintSprite(const char* const* lines, uint8_t n_lines, int y_offset, uint16_t color, int x_off = 0);
void buddySetCursor(int x, int y);
void buddySetColor(uint16_t fg);
void buddyPrint(const char* s);
