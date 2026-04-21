// eye.h — Eye parameter struct and renderer.
//
// Each eye is a parameterized rounded rectangle. Two eyes, independent,
// usually synchronized. Asymmetry is the micro-expression channel —
// a brow raised on one side reads as "skeptical" with zero other changes.
#pragma once

#include <cstdint>

#include "palette.h"

namespace ui_v2 {

enum class EyeKind : uint8_t {
  RoundedRect,
  CaretIn,
  ArcUp,
  Dash,
  Circle,
  WideCircle,
  Cross,
};

struct Eye {
  // Center position, in screen pixels.
  int16_t cx = 0;
  int16_t cy = 0;
  // Dimensions.
  int16_t w = 24;
  int16_t h = 48;
  // Corner radius (0 = hard rect, r >= h/2 = full pill).
  int8_t  r = 4;
  // Rotation, tenths of a degree, -900..+900. Renderer applies small tilts
  // by shearing the rectangle top/bottom edges; large tilts are dropped
  // to keep render cost bounded.
  int16_t tilt = 0;
  // Lid occlusion in pixels. 0 = full open; h = closed.
  uint8_t lid_top = 0;
  uint8_t lid_bot = 0;
  // Brow bar. INT16_MIN disables drawing.
  int16_t brow_y = 0x7fff;  // sentinel "no brow"
  int16_t brow_tilt = 0;
  uint8_t brow_len = 18;
  // Horizontal squish factor. 128 = neutral. 64 = half width, 255 = 2x.
  uint8_t squish = 128;
  EyeKind kind = EyeKind::RoundedRect;
  uint8_t stroke_px = 2;

  // Helpers.
  bool has_brow() const { return brow_y != 0x7fff; }
};

namespace eye {

// Linear-interpolate every field a→b by t in [0,255].
Eye lerp(const Eye& a, const Eye& b, uint8_t t);

// Render one eye in the supplied ink color. Background is NOT cleared — the
// caller owns the dirty rect. A small blit area around (cx, cy) of roughly
// (w*squish/128 + 8, h + 16) is touched.
void render(const Eye& e, bool draw_brow, Rgb24 ink);

// Return the bounding rect (inclusive) the renderer touches for partial
// redraw. Values are clipped to panel bounds by the caller.
void bounds(const Eye& e, int& out_x, int& out_y, int& out_w, int& out_h);

// Gaze offset helper: shifts eye center by (dx, dy) clamped to ±6 px.
void apply_gaze(Eye& left, Eye& right, float gaze_x, float gaze_y);

}  // namespace eye
}  // namespace ui_v2
