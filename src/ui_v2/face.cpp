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

struct BeatSample {
  int dy = 0;
  int width_delta = 0;
  int height_delta = 0;
  int lid_top = 0;
  int lid_bot = 0;
  int squish_delta = 0;
  int brow_shift = 0;
  int tilt_delta = 0;
};

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

float clamp01(float t) {
  return std::max(0.0f, std::min(1.0f, t));
}

int lerp_i(int a, int b, float t) {
  return a + static_cast<int>(std::lround(static_cast<float>(b - a) * clamp01(t)));
}

uint32_t beat_default_duration(ActingBeat beat) {
  switch(beat) {
    case ActingBeat::Arrival:       return 240;
    case ActingBeat::WaitTighten:   return 200;
    case ActingBeat::ApproveQuick:  return 300;
    case ActingBeat::ApproveNormal: return 280;
    case ActingBeat::ApproveSlow:   return 260;
    case ActingBeat::Deny:          return 260;
    case ActingBeat::Dismiss:       return 220;
    case ActingBeat::Timeout:       return 220;
    case ActingBeat::None:          return 0;
  }
  return 0;
}

BeatSample scale_beat(const BeatSample& sample, MotionProfile profile) {
  if(profile != MotionProfile::Reduced) return sample;
  BeatSample out = sample;
  out.dy /= 2;
  out.width_delta /= 2;
  out.height_delta /= 2;
  out.lid_top /= 2;
  out.lid_bot /= 2;
  out.squish_delta /= 2;
  out.brow_shift /= 2;
  out.tilt_delta /= 2;
  return out;
}

BeatSample sample_beat(const FaceState& f, uint32_t now_ms) {
  if(f.beat == ActingBeat::None || f.beat_end_ms <= f.beat_start_ms) return {};
  if(now_ms >= f.beat_end_ms) return {};

  const float span = static_cast<float>(f.beat_end_ms - f.beat_start_ms);
  const float t = clamp01(static_cast<float>(now_ms - f.beat_start_ms) / span);
  BeatSample sample;

  auto settle_triplet = [&](float a_cut, float b_cut,
                            const BeatSample& a0, const BeatSample& a1,
                            const BeatSample& b1, const BeatSample& b2,
                            const BeatSample& c1, const BeatSample& c2) {
    const float ea = ease_apply(Easing::EaseIn, clamp01(t / a_cut));
    const float eb = ease_apply(Easing::EaseOut, clamp01((t - a_cut) / (b_cut - a_cut)));
    const float ec = ease_apply(Easing::EaseOut, clamp01((t - b_cut) / (1.0f - b_cut)));
    if(t < a_cut) {
      sample.dy = lerp_i(a0.dy, a1.dy, ea);
      sample.width_delta = lerp_i(a0.width_delta, a1.width_delta, ea);
      sample.height_delta = lerp_i(a0.height_delta, a1.height_delta, ea);
      sample.lid_top = lerp_i(a0.lid_top, a1.lid_top, ea);
      sample.lid_bot = lerp_i(a0.lid_bot, a1.lid_bot, ea);
      sample.squish_delta = lerp_i(a0.squish_delta, a1.squish_delta, ea);
      sample.brow_shift = lerp_i(a0.brow_shift, a1.brow_shift, ea);
      sample.tilt_delta = lerp_i(a0.tilt_delta, a1.tilt_delta, ea);
    } else if(t < b_cut) {
      sample.dy = lerp_i(b1.dy, b2.dy, eb);
      sample.width_delta = lerp_i(b1.width_delta, b2.width_delta, eb);
      sample.height_delta = lerp_i(b1.height_delta, b2.height_delta, eb);
      sample.lid_top = lerp_i(b1.lid_top, b2.lid_top, eb);
      sample.lid_bot = lerp_i(b1.lid_bot, b2.lid_bot, eb);
      sample.squish_delta = lerp_i(b1.squish_delta, b2.squish_delta, eb);
      sample.brow_shift = lerp_i(b1.brow_shift, b2.brow_shift, eb);
      sample.tilt_delta = lerp_i(b1.tilt_delta, b2.tilt_delta, eb);
    } else {
      sample.dy = lerp_i(c1.dy, c2.dy, ec);
      sample.width_delta = lerp_i(c1.width_delta, c2.width_delta, ec);
      sample.height_delta = lerp_i(c1.height_delta, c2.height_delta, ec);
      sample.lid_top = lerp_i(c1.lid_top, c2.lid_top, ec);
      sample.lid_bot = lerp_i(c1.lid_bot, c2.lid_bot, ec);
      sample.squish_delta = lerp_i(c1.squish_delta, c2.squish_delta, ec);
      sample.brow_shift = lerp_i(c1.brow_shift, c2.brow_shift, ec);
      sample.tilt_delta = lerp_i(c1.tilt_delta, c2.tilt_delta, ec);
    }
  };

  switch(f.beat) {
    case ActingBeat::Arrival:
      settle_triplet(0.24f, 0.62f,
                     {}, BeatSample{2, 0, -4, 1, 1, 22, 0, 0},
                     BeatSample{2, 0, -4, 1, 1, 22, 0, 0},
                     BeatSample{-3, 0, 4, -2, -1, -14, -1, 0},
                     BeatSample{-3, 0, 4, -2, -1, -14, -1, 0},
                     {});
      break;
    case ActingBeat::WaitTighten:
      settle_triplet(0.34f, 0.68f,
                     {}, BeatSample{1, -1, -2, 4, 2, -18, 1, 0},
                     BeatSample{1, -1, -2, 4, 2, -18, 1, 0},
                     BeatSample{0, 0, 1, 1, 1, -6, 0, 0},
                     BeatSample{0, 0, 1, 1, 1, -6, 0, 0},
                     {});
      break;
    case ActingBeat::ApproveQuick:
      settle_triplet(0.18f, 0.58f,
                     {}, BeatSample{2, 0, -4, 1, 1, 18, 0, 0},
                     BeatSample{2, 0, -4, 1, 1, 18, 0, 0},
                     BeatSample{-4, 1, 6, -2, -1, 18, -1, 0},
                     BeatSample{-4, 1, 6, -2, -1, 18, -1, 0},
                     {});
      break;
    case ActingBeat::ApproveNormal:
      settle_triplet(0.22f, 0.60f,
                     {}, BeatSample{1, 0, -2, 0, 1, 10, 0, 0},
                     BeatSample{1, 0, -2, 0, 1, 10, 0, 0},
                     BeatSample{-2, 0, 3, -1, 0, 8, -1, 0},
                     BeatSample{-2, 0, 3, -1, 0, 8, -1, 0},
                     {});
      break;
    case ActingBeat::ApproveSlow:
      settle_triplet(0.24f, 0.62f,
                     {}, BeatSample{1, 0, -1, 0, 1, 6, 0, 0},
                     BeatSample{1, 0, -1, 0, 1, 6, 0, 0},
                     BeatSample{-1, 0, 2, -1, 0, 5, 0, 0},
                     BeatSample{-1, 0, 2, -1, 0, 5, 0, 0},
                     {});
      break;
    case ActingBeat::Deny:
      settle_triplet(0.20f, 0.56f,
                     {}, BeatSample{1, 0, -3, 2, 2, -12, 1, 0},
                     BeatSample{1, 0, -3, 2, 2, -12, 1, 0},
                     BeatSample{0, -1, -1, 6, 6, -22, 1, 0},
                     BeatSample{0, -1, -1, 6, 6, -22, 1, 0},
                     {});
      break;
    case ActingBeat::Dismiss:
    case ActingBeat::Timeout:
      settle_triplet(0.24f, 0.62f,
                     {}, BeatSample{1, 0, -2, 3, 1, -6, 0, 0},
                     BeatSample{1, 0, -2, 3, 1, -6, 0, 0},
                     BeatSample{2, 0, 0, 6, 4, -10, 1, 0},
                     BeatSample{2, 0, 0, 6, 4, -10, 1, 0},
                     {});
      break;
    case ActingBeat::None:
      break;
  }

  return scale_beat(sample, f.motion_profile);
}

void apply_beat(Eye& e, const BeatSample& sample) {
  e.cy = static_cast<int16_t>(e.cy + sample.dy);
  e.w = static_cast<int16_t>(std::max<int>(14, e.w + sample.width_delta));
  e.h = static_cast<int16_t>(std::max<int>(18, e.h + sample.height_delta));
  e.squish = static_cast<uint8_t>(std::max<int>(72, std::min<int>(220, e.squish + sample.squish_delta)));
  e.tilt = static_cast<int16_t>(e.tilt + sample.tilt_delta);
  if(e.has_brow()) e.brow_y = static_cast<int16_t>(e.brow_y + sample.brow_shift);
  const int lid_top = std::max<int>(0, std::min<int>(e.h, static_cast<int>(e.lid_top) + sample.lid_top));
  const int lid_bot = std::max<int>(0, std::min<int>(e.h - lid_top, static_cast<int>(e.lid_bot) + sample.lid_bot));
  e.lid_top = static_cast<uint8_t>(lid_top);
  e.lid_bot = static_cast<uint8_t>(lid_bot);
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
  f.next_blink_ms = now_ms + rng_range(4500, 8500);
}

void schedule_saccade(FaceState& f, uint32_t now_ms) {
  f.next_saccade_ms = now_ms + rng_range(2500, 4500);
}

void render_sample(const FaceState& f, uint32_t now_ms, bool in_blink) {
  Eye cur_l, cur_r, tgt_l, tgt_r;
  apply_expression(f.current, f.base_cx_left, f.base_cx_right, f.base_cy, cur_l, cur_r);
  apply_expression(f.target,  f.base_cx_left, f.base_cx_right, f.base_cy, tgt_l, tgt_r);

  const float tf = tween_progress(f, now_ms);
  const uint8_t t8 = static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, tf)) * 255.0f);
  Eye l = eye::lerp(cur_l, tgt_l, t8);
  Eye r = eye::lerp(cur_r, tgt_r, t8);

  const BeatSample beat = sample_beat(f, now_ms);
  apply_beat(l, beat);
  apply_beat(r, beat);

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
    const float phase = static_cast<float>(now_ms % 1600) / 1600.0f * 6.2832f;
    gaze_x = 0.42f * std::sin(phase);
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
  f.beat = ActingBeat::None;
  f.beat_start_ms = 0;
  f.beat_end_ms = 0;
}

void set_ambient(FaceState& f, bool blinks, bool saccades) {
  f.blinks_enabled = blinks;
  f.saccades_enabled = saccades;
}

void set_motion_profile(FaceState& f, MotionProfile profile) {
  f.motion_profile = profile;
  if(profile == MotionProfile::Static) {
    f.beat = ActingBeat::None;
    f.beat_start_ms = 0;
    f.beat_end_ms = 0;
  }
}

void play_beat(FaceState& f, ActingBeat beat, uint32_t now_ms,
               uint32_t duration_ms) {
  if(f.motion_profile == MotionProfile::Static || beat == ActingBeat::None) {
    f.beat = ActingBeat::None;
    f.beat_start_ms = 0;
    f.beat_end_ms = 0;
    return;
  }
  if(duration_ms == 0) duration_ms = beat_default_duration(beat);
  if(duration_ms == 0) return;
  f.beat = beat;
  f.beat_start_ms = now_ms;
  f.beat_end_ms = now_ms + duration_ms;
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
      f.blink_end_ms = now_ms + (f.motion_profile == MotionProfile::Reduced ? 120u : 160u);
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
      f.gaze_x = (static_cast<int>(raw & 0xff) - 128) / 768.0f;
      f.gaze_y = (static_cast<int>((raw >> 8) & 0xff) - 128) / 1024.0f;
      f.saccade_end_ms = now_ms + rng_range(160, 260);
    } else if(now_ms >= f.saccade_end_ms) {
      f.gaze_x *= 0.55f;
      f.gaze_y *= 0.55f;
      if(std::fabs(f.gaze_x) < 0.015f && std::fabs(f.gaze_y) < 0.015f) {
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

  if(f.beat != ActingBeat::None && now_ms >= f.beat_end_ms) {
    f.beat = ActingBeat::None;
  }

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
