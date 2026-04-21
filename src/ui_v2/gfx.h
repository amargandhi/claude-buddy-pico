// gfx.h — thin PicoGraphics wrapper with V2 helpers.
//
// V2 screens don't touch PicoGraphics directly; they go through these helpers
// so the palette, font choices, and truncation policy are all in one place.
#pragma once

#include <cstddef>
#include <cstdint>

#include "palette.h"

namespace pimoroni {
class PicoGraphics;
}  // namespace pimoroni

namespace ui_v2::gfx {

enum class Align : uint8_t { Left, Center, Right };

// Must be called once before any other gfx call.
void bind(pimoroni::PicoGraphics* g);
pimoroni::PicoGraphics* native_graphics();

// Basic ops.
void clear(Rgb24 c);
void fill_rect(int x, int y, int w, int h, Rgb24 c);
void rect_outline(int x, int y, int w, int h, Rgb24 c);
void rounded_rect(int x, int y, int w, int h, int r, Rgb24 c);
void pixel(int x, int y, Rgb24 c);
void line(int x0, int y0, int x1, int y1, Rgb24 c, int thickness = 1);

// Text.
// `fixed` maps to PicoGraphics' fixed-width rendering path (used in V1).
int  text_width(const char* text, float scale = 1.0f, bool fixed = true);
void text(const char* s, int x, int y, Rgb24 c, float scale = 1.0f, bool fixed = true);
void text_aligned(const char* s, int x, int y, int w, Align align, Rgb24 c,
                  float scale = 1.0f);
// Ellipsize `text` into `out` if it exceeds `max_w` at `scale`. Safe if `text`
// is null or out_size == 0.
void fit_text(char* out, size_t out_size, const char* text, int max_w,
              float scale = 1.0f);

// Chip: rounded pill with centered text. Height is 12 px (fits status rail).
// Returns the width the chip actually drew (useful for status-rail layout).
int draw_chip(int x, int y, const char* label, Rgb24 bg, Rgb24 fg);

// Horizontal progress bar, 0..max filled.
void progress_bar(int x, int y, int w, int h, uint32_t value, uint32_t max,
                  Rgb24 fill, Rgb24 bg);

// 8-bar micro sparkline. Values clamp to `max`.
void sparkline(int x, int y, int w, int h, const uint16_t values[8],
               uint8_t count, uint16_t max, Rgb24 fg);

// Small status LED dot (used in chips).
void dot(int cx, int cy, int r, Rgb24 c);

// Format helpers (no heap).
void format_seconds_hms(char* out, size_t n, uint32_t seconds);          // "03h 21m"
void format_mmss(char* out, size_t n, uint32_t seconds);                 // "02:14"
void format_thousands(char* out, size_t n, uint32_t value);              // "1,482"
void format_compact(char* out, size_t n, uint32_t value);                // "1.4M"
void format_clock_hhmm(char* out, size_t n, uint32_t epoch_local);       // "14:07"

// Commit frame to panel. Caller owns backlight.
void commit();

}  // namespace ui_v2::gfx
