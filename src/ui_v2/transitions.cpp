// transitions.cpp — implementations.
//
// PicoGraphics has no alpha; all fades are simulated by painting over
// screen content with the target color. For merge/wipe transitions this is
// the correct behavior anyway — we're painting orange over everything.
#include "transitions.h"

#include <algorithm>

#include "geometry.h"
#include "gfx.h"

namespace ui_v2::transitions {

namespace {

// Clamp helper.
inline uint32_t elapsed_ms(uint32_t start, uint32_t now) {
  return (now >= start) ? (now - start) : 0;
}

// Ease in-out on [0,1].
inline float ease_in_out(float t) {
  if(t < 0.5f) return 2.0f * t * t;
  return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
}

}  // namespace

// ----- MergeWipe ---------------------------------------------------

void MergeWipe::start(uint32_t now_ms, uint32_t duration_ms) {
  start_ms_    = now_ms;
  duration_ms_ = duration_ms ? duration_ms : 400;
  armed_       = true;
}

bool MergeWipe::is_active(uint32_t now_ms) const {
  if(!armed_) return false;
  return elapsed_ms(start_ms_, now_ms) < duration_ms_;
}

bool MergeWipe::is_done(uint32_t now_ms) const {
  return armed_ && elapsed_ms(start_ms_, now_ms) >= duration_ms_;
}

uint8_t MergeWipe::occlusion(uint32_t now_ms) const {
  if(!is_active(now_ms)) return 0;
  const float t = static_cast<float>(elapsed_ms(start_ms_, now_ms)) /
                  static_cast<float>(duration_ms_);
  // Triangle profile: ramp up to full by t=0.5, hold briefly, ramp down.
  if(t < 0.5f) {
    return static_cast<uint8_t>(std::min(1.0f, t * 2.0f) * 255.0f);
  }
  return static_cast<uint8_t>(std::min(1.0f, (1.0f - t) * 2.0f) * 255.0f);
}

void MergeWipe::draw(uint32_t now_ms) {
  if(!is_active(now_ms)) return;
  const float t = static_cast<float>(elapsed_ms(start_ms_, now_ms)) /
                  static_cast<float>(duration_ms_);

  // We divide the duration into three phases:
  //   in   : 0.0 - 0.45  — expand orange rectangle from center outward
  //   hold : 0.45 - 0.55 — full orange
  //   out  : 0.55 - 1.0  — shrink back. Used for *exit* transitions; for
  //                        enter-only transitions the caller starts a new
  //                        screen and this phase is covered by the new draw.
  const int W = geom::kPanelW;
  const int H = geom::kPanelH;
  const int cx = W / 2;
  const int cy = H / 2;

  float k;  // 0..1 how much of the panel is currently covered
  if(t < 0.45f) {
    k = ease_in_out(t / 0.45f);
  } else if(t < 0.55f) {
    k = 1.0f;
  } else {
    k = 1.0f - ease_in_out((t - 0.55f) / 0.45f);
  }

  const int rw = static_cast<int>(static_cast<float>(W) * k);
  const int rh = static_cast<int>(static_cast<float>(H) * k);
  const int x0 = cx - rw / 2;
  const int y0 = cy - rh / 2;
  gfx::fill_rect(x0, y0, rw, rh, kCaseOrange);
}

// ----- ChipPulse ---------------------------------------------------

void ChipPulse::start(uint32_t now_ms, Rgb24 pulse_color) {
  start_ms_    = now_ms;
  pulse_color_ = pulse_color;
  armed_       = true;
}

bool ChipPulse::is_active(uint32_t now_ms) const {
  if(!armed_) return false;
  return elapsed_ms(start_ms_, now_ms) < 150;
}

bool ChipPulse::sample(uint32_t now_ms, Rgb24 base_color, Rgb24& out_color) const {
  if(!is_active(now_ms)) return false;
  const float t = static_cast<float>(elapsed_ms(start_ms_, now_ms)) / 150.0f;
  // Triangle profile: peak at t=0, decay to base at t=1.
  const uint8_t blend = static_cast<uint8_t>((1.0f - t) * 255.0f);
  out_color = lerp(base_color, pulse_color_, blend);
  return true;
}

// ----- Wipe --------------------------------------------------------

void Wipe::start(uint32_t now_ms, WipeDirection dir) {
  start_ms_ = now_ms;
  dir_      = dir;
  armed_    = true;
}

bool Wipe::is_active(uint32_t now_ms) const {
  if(!armed_) return false;
  return elapsed_ms(start_ms_, now_ms) < 200;
}

bool Wipe::is_done(uint32_t now_ms) const {
  return armed_ && elapsed_ms(start_ms_, now_ms) >= 200;
}

void Wipe::draw(uint32_t now_ms) {
  if(!is_active(now_ms)) return;
  const float t = static_cast<float>(elapsed_ms(start_ms_, now_ms)) / 200.0f;
  // 4 px band swept across 240 px (screen height) in 200 ms.
  const int band_h = 4;
  const int travel = geom::kPanelH + band_h;
  int y;
  if(dir_ == WipeDirection::Down) {
    y = -band_h + static_cast<int>(static_cast<float>(travel) * t);
  } else {
    y = geom::kPanelH - static_cast<int>(static_cast<float>(travel) * t);
  }
  gfx::fill_rect(0, y, geom::kPanelW, band_h, kCaseOrange);
}

// ----- Fade --------------------------------------------------------

void Fade::start(uint32_t now_ms, uint32_t duration_ms, Rgb24 color,
                 uint8_t peak_alpha) {
  start_ms_    = now_ms;
  duration_ms_ = duration_ms ? duration_ms : 200;
  color_       = color;
  peak_alpha_  = peak_alpha;
  armed_       = true;
}

bool Fade::is_active(uint32_t now_ms) const {
  if(!armed_) return false;
  return elapsed_ms(start_ms_, now_ms) < duration_ms_;
}

bool Fade::is_done(uint32_t now_ms) const {
  return armed_ && elapsed_ms(start_ms_, now_ms) >= duration_ms_;
}

void Fade::draw(uint32_t now_ms) {
  if(!is_active(now_ms)) return;
  const float t = static_cast<float>(elapsed_ms(start_ms_, now_ms)) /
                  static_cast<float>(duration_ms_);
  // Triangle profile.
  const float a = (t < 0.5f) ? (t * 2.0f) : ((1.0f - t) * 2.0f);
  const uint8_t alpha = static_cast<uint8_t>(a * static_cast<float>(peak_alpha_));

  // No true alpha: approximate by sampling black→color lerp and painting a
  // dithered overlay. For simplicity, we paint a scaled rect-grid: the panel
  // is covered in `alpha/255` fraction of 2x2 blocks. For alpha > 200 we
  // just do a full fill. This looks reasonable for 200 ms fades.
  if(alpha > 200) {
    gfx::fill_rect(0, 0, geom::kPanelW, geom::kPanelH, color_);
    return;
  }
  // Ordered dither. Skip rows based on alpha for a cheap fade approximation.
  const int step = 2 + static_cast<int>((1.0f - static_cast<float>(alpha) / 255.0f) * 6.0f);
  for(int y = 0; y < geom::kPanelH; y += step) {
    for(int x = (y & 2); x < geom::kPanelW; x += step) {
      gfx::fill_rect(x, y, 1, 1, color_);
    }
  }
}

}  // namespace ui_v2::transitions
