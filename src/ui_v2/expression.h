// expression.h — 18 base expressions.
//
// Each expression is a target parameter pair (left eye, right eye). The face
// engine tweens between two adjacent expressions over a duration with a
// named easing curve. This header is pure data + the lookup function.
#pragma once

#include <cstdint>

#include "eye.h"

namespace ui_v2 {

enum class ExpressionId : uint8_t {
  Neutral,
  Happy,
  Joy,
  Focused,
  Attentive,
  Alert,
  Skeptical,
  Sleepy,
  Asleep,
  Blink,
  WinkL,
  WinkR,
  Squint,
  Sad,
  Dizzy,
  Heart,
  Scan,
  Surprise,
  Proud,
  GlyphCaretIn,
  GlyphArcUp,
  GlyphDash,
  GlyphCircle,
  GlyphWideCircle,
  GlyphCross,
  Count
};

enum class Easing : uint8_t {
  Linear,
  EaseIn,
  EaseOut,
  EaseInOut,
  Spring,
};

struct ExpressionParams {
  Eye       left;
  Eye       right;
  uint16_t  default_duration_ms;
  Easing    default_easing;
};

// Get parameter set for id. Positions (cx, cy) are NOT baked in here —
// the face engine applies the per-screen eye position on top of these
// shape-only deltas. w/h/r/lids/brow come from the expression; cx/cy
// from the face base position.
const ExpressionParams& get_expression(ExpressionId id);

// Name for debug/demo chip ("neutral", "happy", ...).
const char* expression_name(ExpressionId id);

// Apply expression `id` on top of a base eye position: returns eyes
// centered at (base_left_cx, base_cy) and (base_right_cx, base_cy) with
// the expression's deltas overlaid.
void apply_expression(ExpressionId id,
                      int base_left_cx, int base_right_cx, int base_cy,
                      Eye& out_left, Eye& out_right);

}  // namespace ui_v2
