// eye.cpp — renderer for the Eye struct.
#include "eye.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "gfx.h"
#include "palette.h"

namespace ui_v2::eye {

namespace {
inline int16_t lerp_i16(int16_t a, int16_t b, uint8_t t) {
  return static_cast<int16_t>(a + ((static_cast<int>(b) - a) * t) / 255);
}
inline uint8_t lerp_u8(uint8_t a, uint8_t b, uint8_t t) {
  return static_cast<uint8_t>(a + ((static_cast<int>(b) - a) * t) / 255);
}
inline int8_t lerp_i8(int8_t a, int8_t b, uint8_t t) {
  return static_cast<int8_t>(a + ((static_cast<int>(b) - a) * t) / 255);
}
inline int effective_w(const Eye& e) {
  // squish 128 = neutral, so eff_w = w * squish / 128 clamped to [4, w*2].
  const int eff = (static_cast<int>(e.w) * static_cast<int>(e.squish)) / 128;
  return std::max(4, eff);
}
}  // namespace

Eye lerp(const Eye& a, const Eye& b, uint8_t t) {
  Eye o;
  o.cx = lerp_i16(a.cx, b.cx, t);
  o.cy = lerp_i16(a.cy, b.cy, t);
  o.w  = lerp_i16(a.w,  b.w,  t);
  o.h  = lerp_i16(a.h,  b.h,  t);
  o.r  = lerp_i8 (a.r,  b.r,  t);
  o.tilt = lerp_i16(a.tilt, b.tilt, t);
  o.lid_top = lerp_u8(a.lid_top, b.lid_top, t);
  o.lid_bot = lerp_u8(a.lid_bot, b.lid_bot, t);
  o.squish  = lerp_u8(a.squish,  b.squish,  t);
  o.kind = (t >= 128) ? b.kind : a.kind;
  o.stroke_px = lerp_u8(a.stroke_px, b.stroke_px, t);
  o.brow_len = lerp_u8(a.brow_len, b.brow_len, t);
  o.brow_tilt = lerp_i16(a.brow_tilt, b.brow_tilt, t);
  // brow_y: if either side has "no brow" sentinel, keep whichever is target-side dominant.
  if(a.has_brow() && b.has_brow()) {
    o.brow_y = lerp_i16(a.brow_y, b.brow_y, t);
  } else {
    o.brow_y = (t >= 128 ? b.brow_y : a.brow_y);
  }
  return o;
}

void bounds(const Eye& e, int& out_x, int& out_y, int& out_w, int& out_h) {
  const int eff_w = effective_w(e);
  const int pad = (e.kind == EyeKind::RoundedRect) ? 6 : 10;
  out_x = e.cx - eff_w / 2 - pad;
  out_y = e.cy - e.h / 2 - pad - 10;  // brow can sit above
  out_w = eff_w + pad * 2;
  out_h = e.h + pad * 2 + 10;
}

void render_circle_outline(int cx, int cy, int rx, int ry, Rgb24 ink, int stroke) {
  const int samples = 24;
  int prev_x = cx + rx;
  int prev_y = cy;
  for(int i = 1; i <= samples; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(samples);
    const float a = t * 6.2831853f;
    const int x = cx + static_cast<int>(std::cos(a) * static_cast<float>(rx));
    const int y = cy + static_cast<int>(std::sin(a) * static_cast<float>(ry));
    gfx::line(prev_x, prev_y, x, y, ink, stroke);
    prev_x = x;
    prev_y = y;
  }
}

void render_caret(const Eye& e, bool apex_left, Rgb24 ink) {
  const int stroke = std::max<int>(1, e.stroke_px);
  const int hw = effective_w(e) / 2;
  const int hh = e.h / 2;
  const int x_left = e.cx - hw;
  const int x_right = e.cx + hw;
  const int y_top = e.cy - hh;
  const int y_bot = e.cy + hh;
  const int ax = apex_left ? x_left : x_right;
  const int bx = apex_left ? x_right : x_left;
  gfx::line(bx, y_top, ax, e.cy, ink, stroke);
  gfx::line(ax, e.cy, bx, y_bot, ink, stroke);
}

void render_arc_up(const Eye& e, Rgb24 ink) {
  const int stroke = std::max<int>(1, e.stroke_px);
  const int hw = effective_w(e) / 2;
  const int hh = e.h / 2;
  const int x_left = e.cx - hw;
  const int x_right = e.cx + hw;
  const int y_top = e.cy - hh / 2;
  const int y_mid = e.cy - hh;
  gfx::line(x_left, e.cy + hh / 2, e.cx, y_mid, ink, stroke);
  gfx::line(e.cx, y_mid, x_right, e.cy + hh / 2, ink, stroke);
  // small support ticks so the glyph reads at desk distance.
  gfx::line(x_left, e.cy + hh / 2, x_left + hw / 4, y_top, ink, 1);
  gfx::line(x_right - hw / 4, y_top, x_right, e.cy + hh / 2, ink, 1);
}

void render(const Eye& e, bool draw_brow, const Rgb24 ink) {
  const int eff_w = effective_w(e);
  if(e.kind != EyeKind::RoundedRect) {
    const int stroke = std::max<int>(1, e.stroke_px);
    switch(e.kind) {
      case EyeKind::CaretIn:
        render_caret(e, e.tilt > 0, ink);
        break;
      case EyeKind::ArcUp:
        render_arc_up(e, ink);
        break;
      case EyeKind::Dash:
        gfx::line(e.cx - eff_w / 2, e.cy, e.cx + eff_w / 2, e.cy, ink, stroke);
        break;
      case EyeKind::Circle:
        render_circle_outline(e.cx, e.cy, std::max(5, eff_w / 2), std::max(5, e.h / 2),
                              ink, stroke);
        break;
      case EyeKind::WideCircle:
        render_circle_outline(e.cx, e.cy, std::max(7, eff_w / 2 + 4),
                              std::max(5, e.h / 2), ink, stroke);
        break;
      case EyeKind::Cross: {
        const int hw = eff_w / 2;
        const int hh = e.h / 2;
        gfx::line(e.cx - hw, e.cy - hh, e.cx + hw, e.cy + hh, ink, stroke);
        gfx::line(e.cx + hw, e.cy - hh, e.cx - hw, e.cy + hh, ink, stroke);
        break;
      }
      case EyeKind::RoundedRect:
        break;
    }
    return;
  }

  const int x0 = e.cx - eff_w / 2;
  const int y0 = e.cy - e.h / 2;

  // Apply lids as a vertical crop of the rect.
  const int top_crop = std::min<int>(e.lid_top, e.h);
  const int bot_crop = std::min<int>(e.lid_bot, static_cast<int>(e.h) - top_crop);
  const int visible_h = e.h - top_crop - bot_crop;
  if(visible_h <= 0) {
    // Eye fully closed; draw a 2 px line to suggest a seam.
    gfx::fill_rect(x0, e.cy - 1, eff_w, 2, ink);
    return;
  }

  // Handle small tilts by drawing the rect as a trapezoid (parallelogram
  // in the horizontal axis). For larger tilts the visual impact isn't worth
  // the fidelity; cap at ±20 deg effective.
  const int tilt_clamped = std::max<int16_t>(-200, std::min<int16_t>(200, e.tilt));
  const float tilt_rad = static_cast<float>(tilt_clamped) * (3.14159265f / 180.0f) / 10.0f;
  const float skew = std::tan(tilt_rad);  // dx per row

  // Rounded top and bottom edges: we fill row by row, adjusting x inset
  // for corner rounding and skew.
  const int r_effective = std::min<int>(e.r, std::min<int>(eff_w / 2, e.h / 2));

  const int draw_y0 = y0 + top_crop;
  const int draw_y1 = y0 + e.h - bot_crop;  // exclusive

  for(int y = draw_y0; y < draw_y1; ++y) {
    // Row-relative position for corner rounding (distance from nearest vertical edge).
    int inset = 0;
    if(r_effective > 0) {
      const int from_top = y - y0;                 // 0..h-1
      const int from_bot = (y0 + e.h - 1) - y;     // 0..h-1
      const int corner_y = std::min(from_top, from_bot);
      if(corner_y < r_effective) {
        const int dy = r_effective - corner_y;
        inset = r_effective - static_cast<int>(
            __builtin_sqrtf(static_cast<float>(r_effective * r_effective - dy * dy)));
      }
    }
    const int row_x = x0 + inset
                      + static_cast<int>(skew * static_cast<float>(y - e.cy));
    const int row_w = eff_w - 2 * inset;
    if(row_w <= 0) continue;
    gfx::fill_rect(row_x, y, row_w, 1, ink);
  }

  // Brow bar above the eye.
  if(draw_brow && e.has_brow()) {
    const int by = e.cy - e.h / 2 - 6 + e.brow_y;
    const int bw = std::max<int>(6, e.brow_len);
    const int bx = e.cx - bw / 2;
    const float brow_rad = static_cast<float>(e.brow_tilt) * (3.14159265f / 180.0f) / 10.0f;
    // Draw a 2 px-tall line with brow_tilt applied via simple skew across its width.
    for(int i = 0; i < bw; ++i) {
      const int dy = static_cast<int>(
          std::tan(brow_rad) * static_cast<float>(i - bw / 2));
      gfx::fill_rect(bx + i, by + dy, 1, 2, ink);
    }
  }
}

void apply_gaze(Eye& left, Eye& right, float gaze_x, float gaze_y) {
  gaze_x = std::max(-1.0f, std::min(1.0f, gaze_x));
  gaze_y = std::max(-1.0f, std::min(1.0f, gaze_y));
  const int dx = static_cast<int>(gaze_x * 6.0f);
  const int dy = static_cast<int>(gaze_y * 6.0f);
  left.cx  = static_cast<int16_t>(left.cx + dx);
  left.cy  = static_cast<int16_t>(left.cy + dy);
  right.cx = static_cast<int16_t>(right.cx + dx);
  right.cy = static_cast<int16_t>(right.cy + dy);
}

}  // namespace ui_v2::eye
