// gfx.cpp — PicoGraphics wrapper implementation.
#include "gfx.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "libraries/pico_graphics/pico_graphics.hpp"
#include "drivers/st7789/st7789.hpp"

namespace ui_v2::gfx {

namespace {
pimoroni::PicoGraphics* g_graphics = nullptr;

// Apply a 24-bit color via the PicoGraphics 8-8-8 set_rgb path. PicoGraphics
// snaps to the active pen format internally. Keeps color handling identical
// to V1.
void set_pen(Rgb24 c) {
  if(g_graphics == nullptr) return;
  g_graphics->set_pen(c.r, c.g, c.b);
}

// Copy src into dst, clamping to dst_size-1 and null-terminating.
void safe_copy(char* dst, size_t dst_size, const char* src) {
  if(dst_size == 0) return;
  if(src == nullptr) { dst[0] = '\0'; return; }
  std::snprintf(dst, dst_size, "%s", src);
}
}  // namespace

void bind(pimoroni::PicoGraphics* g) { g_graphics = g; }

pimoroni::PicoGraphics* native_graphics() { return g_graphics; }

void clear(Rgb24 c) {
  if(g_graphics == nullptr) return;
  set_pen(c);
  g_graphics->clear();
}

void fill_rect(int x, int y, int w, int h, Rgb24 c) {
  if(g_graphics == nullptr) return;
  set_pen(c);
  g_graphics->rectangle(pimoroni::Rect(x, y, w, h));
}

void rect_outline(int x, int y, int w, int h, Rgb24 c) {
  if(g_graphics == nullptr || w <= 0 || h <= 0) return;
  set_pen(c);
  g_graphics->rectangle(pimoroni::Rect(x, y, w, 1));
  g_graphics->rectangle(pimoroni::Rect(x, y + h - 1, w, 1));
  g_graphics->rectangle(pimoroni::Rect(x, y, 1, h));
  g_graphics->rectangle(pimoroni::Rect(x + w - 1, y, 1, h));
}

void rounded_rect(int x, int y, int w, int h, int r, Rgb24 c) {
  if(g_graphics == nullptr || w <= 0 || h <= 0) return;
  r = std::min({r, w / 2, h / 2});
  if(r <= 0) {
    fill_rect(x, y, w, h, c);
    return;
  }
  set_pen(c);
  // Center strip (full width).
  g_graphics->rectangle(pimoroni::Rect(x, y + r, w, h - 2 * r));
  // Top and bottom strips (reduced width).
  g_graphics->rectangle(pimoroni::Rect(x + r, y, w - 2 * r, r));
  g_graphics->rectangle(pimoroni::Rect(x + r, y + h - r, w - 2 * r, r));
  // Four corners: filled quarter-circles via integer Bresenham-like scan.
  for(int dy = 0; dy < r; ++dy) {
    const int dx_limit = static_cast<int>(
        __builtin_sqrtf(static_cast<float>(r * r - (r - dy) * (r - dy))));
    for(int dx = 0; dx <= dx_limit; ++dx) {
      // TL
      g_graphics->pixel(pimoroni::Point(x + r - 1 - dx, y + dy));
      // TR
      g_graphics->pixel(pimoroni::Point(x + w - r + dx, y + dy));
      // BL
      g_graphics->pixel(pimoroni::Point(x + r - 1 - dx, y + h - 1 - dy));
      // BR
      g_graphics->pixel(pimoroni::Point(x + w - r + dx, y + h - 1 - dy));
    }
  }
}

void pixel(int x, int y, Rgb24 c) {
  if(g_graphics == nullptr) return;
  set_pen(c);
  g_graphics->pixel(pimoroni::Point(x, y));
}

void line(int x0, int y0, int x1, int y1, Rgb24 c, int thickness) {
  if(g_graphics == nullptr) return;
  thickness = std::max(1, thickness);
  set_pen(c);

  int dx = std::abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  const int radius = thickness / 2;

  while(true) {
    for(int oy = -radius; oy <= radius; ++oy) {
      for(int ox = -radius; ox <= radius; ++ox) {
        g_graphics->pixel(pimoroni::Point(x0 + ox, y0 + oy));
      }
    }
    if(x0 == x1 && y0 == y1) break;
    const int e2 = err * 2;
    if(e2 >= dy) { err += dy; x0 += sx; }
    if(e2 <= dx) { err += dx; y0 += sy; }
  }
}

int text_width(const char* s, float scale, bool fixed) {
  if(g_graphics == nullptr || s == nullptr) return 0;
  g_graphics->set_font("bitmap6");
  return g_graphics->measure_text(s, scale, 0, fixed);
}

void text(const char* s, int x, int y, Rgb24 c, float scale, bool fixed) {
  if(g_graphics == nullptr || s == nullptr || s[0] == '\0') return;
  set_pen(c);
  g_graphics->set_font("bitmap6");
  g_graphics->text(s, pimoroni::Point(x, y), 4096, scale, 0.0f, 0, fixed);
}

void text_aligned(const char* s, int x, int y, int w, Align align, Rgb24 c,
                  float scale) {
  const int tw = text_width(s, scale);
  int draw_x = x;
  if(align == Align::Center) draw_x = x + (w - tw) / 2;
  else if(align == Align::Right) draw_x = x + w - tw;
  text(s, draw_x, y, c, scale);
}

void fit_text(char* out, size_t out_size, const char* src, int max_w,
              float scale) {
  if(out == nullptr || out_size == 0) return;
  out[0] = '\0';
  if(src == nullptr) return;
  safe_copy(out, out_size, src);
  if(text_width(out, scale) <= max_w) return;

  const char* ellipsis = "...";
  const int ew = text_width(ellipsis, scale);
  if(ew > max_w) { out[0] = '\0'; return; }

  size_t n = std::strlen(out);
  while(n > 0) {
    out[--n] = '\0';
    char candidate[96];
    std::snprintf(candidate, sizeof(candidate), "%s%s", out, ellipsis);
    if(text_width(candidate, scale) <= max_w) {
      safe_copy(out, out_size, candidate);
      return;
    }
  }
  safe_copy(out, out_size, ellipsis);
}

int draw_chip(int x, int y, const char* label, Rgb24 bg, Rgb24 fg) {
  if(label == nullptr) return 0;
  const int tw = text_width(label);
  const int w = tw + 12;  // 6 px padding each side
  const int h = 12;
  rounded_rect(x, y, w, h, 3, bg);
  text(label, x + 6, y + 2, fg);
  return w;
}

void progress_bar(int x, int y, int w, int h, uint32_t value, uint32_t max,
                  Rgb24 fill, Rgb24 bg) {
  if(max == 0) max = 1;
  fill_rect(x, y, w, h, bg);
  const uint32_t filled = std::min<uint32_t>(w, (static_cast<uint64_t>(value) * w) / max);
  if(filled > 0) {
    fill_rect(x, y, static_cast<int>(filled), h, fill);
  }
}

void sparkline(int x, int y, int w, int h, const uint16_t values[8],
               uint8_t count, uint16_t max, Rgb24 fg) {
  if(count == 0) return;
  if(max == 0) max = 1;
  const int bar_w = std::max(2, (w - 8) / 8);
  const int step = bar_w + 1;
  for(uint8_t i = 0; i < count && i < 8; ++i) {
    const int bar_h = std::min<int>(h, (values[i] * h) / max);
    const int bx = x + i * step;
    const int by = y + (h - bar_h);
    fill_rect(bx, by, bar_w, bar_h, fg);
  }
}

void dot(int cx, int cy, int r, Rgb24 c) {
  if(g_graphics == nullptr) return;
  set_pen(c);
  for(int dy = -r; dy <= r; ++dy) {
    for(int dx = -r; dx <= r; ++dx) {
      if(dx * dx + dy * dy <= r * r) {
        g_graphics->pixel(pimoroni::Point(cx + dx, cy + dy));
      }
    }
  }
}

void format_seconds_hms(char* out, size_t n, uint32_t seconds) {
  if(out == nullptr || n == 0) return;
  const uint32_t h = seconds / 3600;
  const uint32_t m = (seconds % 3600) / 60;
  std::snprintf(out, n, "%02" PRIu32 "h %02" PRIu32 "m", h, m);
}

void format_mmss(char* out, size_t n, uint32_t seconds) {
  if(out == nullptr || n == 0) return;
  const uint32_t m = seconds / 60;
  const uint32_t s = seconds % 60;
  std::snprintf(out, n, "%02" PRIu32 ":%02" PRIu32, m, s);
}

void format_thousands(char* out, size_t n, uint32_t value) {
  if(out == nullptr || n == 0) return;
  char raw[16];
  std::snprintf(raw, sizeof(raw), "%" PRIu32, value);
  const int len = static_cast<int>(std::strlen(raw));
  int commas = (len - 1) / 3;
  const int total = len + commas;
  if(static_cast<size_t>(total + 1) > n) {
    std::snprintf(out, n, "%" PRIu32, value);
    return;
  }
  out[total] = '\0';
  int src = len - 1;
  int dst = total - 1;
  int run = 0;
  while(src >= 0) {
    if(run == 3) {
      out[dst--] = ',';
      run = 0;
    }
    out[dst--] = raw[src--];
    ++run;
  }
}

void format_compact(char* out, size_t n, uint32_t value) {
  if(out == nullptr || n == 0) return;
  if(value >= 1'000'000u) {
    std::snprintf(out, n, "%" PRIu32 ".%" PRIu32 "M",
                  value / 1'000'000u, (value % 1'000'000u) / 100'000u);
  } else if(value >= 1'000u) {
    std::snprintf(out, n, "%" PRIu32 ".%" PRIu32 "k",
                  value / 1000u, (value % 1000u) / 100u);
  } else {
    std::snprintf(out, n, "%" PRIu32, value);
  }
}

void format_clock_hhmm(char* out, size_t n, uint32_t epoch_local) {
  if(out == nullptr || n == 0) return;
  const uint32_t seconds_of_day = epoch_local % 86400u;
  const uint32_t h = seconds_of_day / 3600u;
  const uint32_t m = (seconds_of_day % 3600u) / 60u;
  std::snprintf(out, n, "%02" PRIu32 ":%02" PRIu32, h, m);
}

void commit() {
  // PicoGraphics commit is owned by the caller (ui_core) because the
  // ST7789 reference lives in the V1 monolith. See ui_core.cpp.
}

}  // namespace ui_v2::gfx
