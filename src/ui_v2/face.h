// face.h — face engine: tween, blink cadence, saccades.
//
// The engine holds the current expression and a target; on every tick it
// interpolates and draws. Blink and saccade behaviors layer on top and
// never drop below zero — they're ambient.
#pragma once

#include <cstdint>

#include "expression.h"
#include "palette.h"
#include "ui_state.h"

namespace ui_v2 {

enum class ActingBeat : uint8_t {
  None,
  Arrival,
  WaitTighten,
  ApproveQuick,
  ApproveNormal,
  ApproveSlow,
  Deny,
  Dismiss,
  Timeout,
};

struct FaceState {
  // Where the face lives on the current screen. Home uses (51, 269, 73);
  // approval overlay might use (180, 260, 80) to sit top-right.
  int base_cx_left  = 51;
  int base_cx_right = 269;
  int base_cy       = 73;

  // Active expression and transition target.
  ExpressionId current = ExpressionId::Neutral;
  ExpressionId target  = ExpressionId::Neutral;

  // Tween progress.
  uint32_t tween_start_ms = 0;
  uint32_t tween_end_ms   = 0;
  Easing   easing         = Easing::EaseOut;

  // Ambient behaviors.
  bool     blinks_enabled   = true;
  bool     saccades_enabled = true;
  uint32_t next_blink_ms    = 0;
  uint32_t blink_end_ms     = 0;
  uint32_t next_saccade_ms  = 0;
  uint32_t saccade_end_ms   = 0;
  float    gaze_x = 0.0f;
  float    gaze_y = 0.0f;

  // Default face ink is cream so the eyes remain visible on the dark UI.
  Rgb24    ink = kTextPrimary;
  MotionProfile motion_profile = MotionProfile::Full;

  // One-shot jitters for dizzy / surprise / heart.
  uint32_t one_shot_until_ms = 0;

  // Short acting beats layered on top of the base expression tween.
  ActingBeat beat = ActingBeat::None;
  uint32_t   beat_start_ms = 0;
  uint32_t   beat_end_ms   = 0;
};

namespace face {

// Seed RNG for randomized blink intervals; call once at boot.
void seed(uint32_t seed_value);

// Set the face's on-screen base position. Screens call this when they
// take over rendering.
void set_base(FaceState& f, int cx_left, int cx_right, int cy);

// Override the eye/brow ink. Orange-merge scenes can switch this to kBgInk if
// they want literal black details over orange; the default is cream.
void set_ink(FaceState& f, Rgb24 ink);

// Begin a transition to target over duration_ms; 0 uses expression default.
void begin_transition(FaceState& f, ExpressionId target, uint32_t now_ms,
                      uint32_t duration_ms = 0);

// Pause ambient behaviors (blink + saccade). Used on approval overlay where
// jitter would steal attention from the decision.
void set_ambient(FaceState& f, bool blinks, bool saccades);
void set_motion_profile(FaceState& f, MotionProfile profile);
void play_beat(FaceState& f, ActingBeat beat, uint32_t now_ms,
               uint32_t duration_ms = 0);

// Called once per UI tick. Advances tween, blink cadence, saccades, and
// renders both eyes to the framebuffer.
void tick_and_render(FaceState& f, uint32_t now_ms);
void render_only(const FaceState& f, uint32_t now_ms);

// Immediate cut to target, no tween. Used by boot sequence.
void snap(FaceState& f, ExpressionId target, uint32_t now_ms);

}  // namespace face
}  // namespace ui_v2
