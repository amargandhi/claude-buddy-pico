// face.cpp — face engine implementation.
#include "face.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "eye.h"
#include "gfx.h"
#include "palette.h"

namespace ui_v2::face {

namespace {

uint32_t g_rng = 0xA55A1234u;

uint32_t rng_next() {
  // xorshift32
  uint32_t x = g_rng;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  g_rng = x;
  return x;
}

uint32_t rng_range(uint32_t min_v, uint32_t max_v) {
  if(max_v <= min_v) return min_v;
  return min_v + (rng_next() % (max_v - min_v + 1));
}

float ease_apply(Easing e, float t) {
  switch(e) {
    case Easing::Linear:    return t;
    case Easing::EaseIn:    return t * t;
    case Easing::EaseOut:   return 1.0f - (1.0f - t) * (1.0f - t);
    case Easing::EaseInOut: return t < 0.5f ? 2.0f * t * t
                                            : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
    case Easing::Spring: {
      // Damped cosine-based spring. Not physically accurate, but reads right.
      const float damp = std::exp(-3.0f * t);
      return 1.0f - damp * std::cos(12.0f * t);
    }
  }
  return t;
}

// Compute a tween eye pair for the current state. Returns t in [0, 1]
// representing how far from `current` to `target` we are.
float tween_progress(const FaceState& f, uint32_t now_ms) {
  if(f.tween_end_ms <= f.tween_start_ms) return 1.0f;
  if(now_ms >= f.tween_end_ms) return 1.0f;
  if(now_ms <= f.tween_start_ms) return 0.0f;
  const float span = static_cast<float>(f.tween_end_ms - f.tween_start_ms);
  const float elapsed = static_cast<float>(now_ms - f.tween_start_ms);
  return ease_apply(f.easing, elapsed / span);
}

void schedule_blink(FaceState& f, uint32_t now_ms) {
  f.next_blink_ms = now_ms + rng_range(4000, 8000);
}

void schedule_saccade(FaceState& f, uint32_t now_ms) {
  f.next_saccade_ms = now_ms + rng_range(1000, 3000);
}

void render_sample(const FaceState& f, uint32_t now_ms, bool in_blink) {
  Eye cur_l, cur_r, tgt_l, tgt_r;
  apply_expression(f.current, f.base_cx_left, f.base_cx_right, f.base_cy, cur_l, cur_r);
  apply_expression(f.target,  f.base_cx_left, f.base_cx_right, f.base_cy, tgt_l, tgt_r);

  const float tf = tween_progress(f, now_ms);
  const uint8_t t8 = static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, tf)) * 255.0f);
  Eye l = eye::lerp(cur_l, tgt_l, t8);
  Eye r = eye::lerp(cur_r, tgt_r, t8);

  if(f.target == ExpressionId::Dizzy || f.current == ExpressionId::Dizzy) {
    const float phase = static_cast<float>(now_ms % 1000) / 1000.0f * 6.2832f;
    const int dx = static_cast<int>(6.0f * std::cos(phase));
    const int dy = static_cast<int>(4.0f * std::sin(phase));
    l.cx = static_cast<int16_t>(l.cx + dx);
    l.cy = static_cast<int16_t>(l.cy + dy);
    r.cx = static_cast<int16_t>(r.cx - dx);
    r.cy = static_cast<int16_t>(r.cy + dy);
  }

  float gaze_x = f.gaze_x;
  if(f.target == ExpressionId::Scan) {
    const float phase = static_cast<float>(now_ms % 1200) / 1200.0f * 6.2832f;
    gaze_x = 0.6f * std::sin(phase);
  }
  eye::apply_gaze(l, r, gaze_x, f.gaze_y);

  if(in_blink) {
    l.lid_top = static_cast<uint8_t>(std::max<int>(l.lid_top, l.h / 2));
    l.lid_bot = static_cast<uint8_t>(std::max<int>(l.lid_bot, l.h / 2));
    r.lid_top = static_cast<uint8_t>(std::max<int>(r.lid_top, r.h / 2));
    r.lid_bot = static_cast<uint8_t>(std::max<int>(r.lid_bot, r.h / 2));
  }

  eye::render(l, l.has_brow(), f.ink);
  eye::render(r, r.has_brow(), f.ink);
}

}  // namespace

void seed(uint32_t seed_value) { g_rng = seed_value ? seed_value : 1; }

void set_base(FaceState& f, int cx_left, int cx_right, int cy) {
  f.base_cx_left = cx_left;
  f.base_cx_right = cx_right;
  f.base_cy = cy;
}

void set_ink(FaceState& f, const Rgb24 ink) { f.ink = ink; }

void begin_transition(FaceState& f, ExpressionId target, uint32_t now_ms,
                      uint32_t duration_ms) {
  if(target == f.target) return;
  const ExpressionParams& p = get_expression(target);
  if(duration_ms == 0) duration_ms = p.default_duration_ms;
  f.current = f.target;  // snap pending transitions closed
  f.target = target;
  f.tween_start_ms = now_ms;
  f.tween_end_ms = now_ms + duration_ms;
  f.easing = p.default_easing;
}

void snap(FaceState& f, ExpressionId target, uint32_t now_ms) {
  f.current = target;
  f.target = target;
  f.tween_start_ms = now_ms;
  f.tween_end_ms = now_ms;
}

void set_ambient(FaceState& f, bool blinks, bool saccades) {
  f.blinks_enabled = blinks;
  f.saccades_enabled = saccades;
}

void set_motion_profile(FaceState& f, MotionProfile profile) {
  f.motion_profile = profile;
}

void tick_and_render(FaceState& f, uint32_t now_ms) {
  // Lazy-initialize blink / saccade schedules on first tick.
  if(f.next_blink_ms == 0) schedule_blink(f, now_ms);
  if(f.next_saccade_ms == 0) schedule_saccade(f, now_ms);

  // Blink overlay (not a transition — a short lid closure on top of the current frame).
  bool in_blink = false;
  const bool allow_blinks = f.blinks_enabled && f.motion_profile != MotionProfile::Static;
  if(allow_blinks) {
    if(now_ms >= f.next_blink_ms && f.blink_end_ms <= f.next_blink_ms) {
      f.blink_end_ms = now_ms + 90;
      in_blink = true;
    } else if(now_ms < f.blink_end_ms) {
      in_blink = true;
    } else if(f.blink_end_ms != 0 && now_ms >= f.blink_end_ms) {
      schedule_blink(f, now_ms);
      f.blink_end_ms = 0;
    }
  }

  // Saccade update: small gaze shifts.
  const bool allow_saccades = f.saccades_enabled && f.motion_profile == MotionProfile::Full;
  if(allow_saccades) {
    if(now_ms >= f.next_saccade_ms && now_ms >= f.saccade_end_ms) {
      // Begin a saccade.
      const uint32_t raw = rng_next();
      f.gaze_x = (static_cast<int>(raw & 0xff) - 128) / 512.0f;   // ±0.25
      f.gaze_y = (static_cast<int>((raw >> 8) & 0xff) - 128) / 512.0f;
      f.saccade_end_ms = now_ms + rng_range(200, 400);
    } else if(now_ms >= f.saccade_end_ms) {
      f.gaze_x *= 0.7f;  // decay back toward center
      f.gaze_y *= 0.7f;
      if(std::fabs(f.gaze_x) < 0.02f && std::fabs(f.gaze_y) < 0.02f) {
        f.gaze_x = 0.0f;
        f.gaze_y = 0.0f;
        schedule_saccade(f, now_ms);
        f.saccade_end_ms = 0;
      }
    }
  } else {
    f.gaze_x = 0.0f;
    f.gaze_y = 0.0f;
  }

  render_sample(f, now_ms, in_blink);

  // When tween reaches 1, current becomes target.
  if(tween_progress(f, now_ms) >= 1.0f) {
    f.current = f.target;
  }
}

void render_only(const FaceState& f, uint32_t now_ms) {
  const bool in_blink = (f.blink_end_ms != 0 && now_ms < f.blink_end_ms);
  render_sample(f, now_ms, in_blink);
}

}  // namespace ui_v2::face
