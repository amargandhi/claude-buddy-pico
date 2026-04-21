// expression.cpp — 18 expression parameter sets.
//
// Each entry defines the DELTAS from the base eye rect (24 w × 48 h × r 4).
// apply_expression() overlays the deltas onto a base center-x per eye, so the
// same expression library works on Home, Pet, Approval, etc., regardless of
// where the face is drawn on that screen.
#include "expression.h"

#include "geometry.h"

namespace ui_v2 {

namespace {

constexpr Eye kBase() {
  Eye e;
  e.w = 24; e.h = 48; e.r = 4;
  e.lid_top = 0; e.lid_bot = 0;
  e.squish = 128;
  e.tilt = 0;
  e.brow_y = 0x7fff;  // no brow by default
  e.brow_tilt = 0;
  e.brow_len = 18;
  return e;
}

constexpr Eye kGlyphBase(EyeKind kind, int16_t w = 28, int16_t h = 34) {
  Eye e = kBase();
  e.kind = kind;
  e.w = w;
  e.h = h;
  e.r = 0;
  e.stroke_px = 2;
  e.lid_top = 0;
  e.lid_bot = 0;
  e.brow_y = 0x7fff;
  return e;
}

struct Entry {
  ExpressionId id;
  const char* name;
  ExpressionParams params;
};

ExpressionParams make(Eye l, Eye r, uint16_t dur, Easing ease) {
  return ExpressionParams{l, r, dur, ease};
}

// Build the static table at first call.
const Entry* table() {
  static const Entry kTable[] = {
      // NEUTRAL — default idle rect.
      {ExpressionId::Neutral, "neutral",
       make(kBase(), kBase(), 150, Easing::EaseOut)},

      // HAPPY — lids closed a bit, slight squash, outward tilt.
      {ExpressionId::Happy, "happy",
       [] {
         Eye l = kBase(); l.lid_top = 4; l.lid_bot = 4; l.squish = 140; l.tilt = -60;
         Eye r = kBase(); r.lid_top = 4; r.lid_bot = 4; r.squish = 140; r.tilt = 60;
         return make(l, r, 120, Easing::EaseOut);
       }()},

      // JOY — happy + upward bounce. The "bounce" is a cy offset handled by face engine.
      {ExpressionId::Joy, "joy",
       [] {
         Eye l = kBase(); l.lid_top = 6; l.lid_bot = 6; l.squish = 150; l.tilt = -80;
         Eye r = kBase(); r.lid_top = 6; r.lid_bot = 6; r.squish = 150; r.tilt = 80;
         return make(l, r, 180, Easing::Spring);
       }()},

      // FOCUSED — taller, narrower.
      {ExpressionId::Focused, "focused",
       [] {
         Eye l = kBase(); l.h = 52; l.w = 22;
         Eye r = kBase(); r.h = 52; r.w = 22;
         return make(l, r, 80, Easing::Linear);
       }()},

      // ATTENTIVE — slightly wider and taller.
      {ExpressionId::Attentive, "attentive",
       [] {
         Eye l = kBase(); l.w = 26; l.h = 52;
         Eye r = kBase(); r.w = 26; r.h = 52;
         return make(l, r, 120, Easing::EaseIn);
       }()},

      // ALERT — pop: big and slightly tilted inward.
      {ExpressionId::Alert, "alert",
       [] {
         Eye l = kBase(); l.w = 28; l.h = 54; l.tilt = 30;
         Eye r = kBase(); r.w = 28; r.h = 54; r.tilt = -30;
         return make(l, r, 60, Easing::EaseOut);
       }()},

      // SKEPTICAL — asymmetric brows.
      {ExpressionId::Skeptical, "skeptical",
       [] {
         Eye l = kBase(); l.brow_y = 4;  l.brow_tilt =  60; l.brow_len = 18;
         Eye r = kBase(); r.brow_y = -4; r.brow_tilt = -60; r.brow_len = 18;
         return make(l, r, 200, Easing::EaseInOut);
       }()},

      // SLEEPY — heavy top lids.
      {ExpressionId::Sleepy, "sleepy",
       [] {
         Eye l = kBase(); l.lid_top = 12; l.lid_bot = 4;
         Eye r = kBase(); r.lid_top = 12; r.lid_bot = 4;
         return make(l, r, 300, Easing::EaseInOut);
       }()},

      // ASLEEP — closed.
      {ExpressionId::Asleep, "asleep",
       [] {
         Eye l = kBase(); l.lid_top = 48;
         Eye r = kBase(); r.lid_top = 48;
         return make(l, r, 400, Easing::EaseOut);
       }()},

      // BLINK — transient, lids meet in middle.
      {ExpressionId::Blink, "blink",
       [] {
         Eye l = kBase(); l.lid_top = 24; l.lid_bot = 24;
         Eye r = kBase(); r.lid_top = 24; r.lid_bot = 24;
         return make(l, r, 80, Easing::Linear);
       }()},

      // WINK_L — only left eye blinks.
      {ExpressionId::WinkL, "wink_l",
       [] {
         Eye l = kBase(); l.lid_top = 24; l.lid_bot = 24;
         Eye r = kBase();
         return make(l, r, 100, Easing::Linear);
       }()},

      // WINK_R — only right eye blinks.
      {ExpressionId::WinkR, "wink_r",
       [] {
         Eye l = kBase();
         Eye r = kBase(); r.lid_top = 24; r.lid_bot = 24;
         return make(l, r, 100, Easing::Linear);
       }()},

      // SQUINT — tight.
      {ExpressionId::Squint, "squint",
       [] {
         Eye l = kBase(); l.w = 20; l.lid_top = 8; l.lid_bot = 8;
         Eye r = kBase(); r.w = 20; r.lid_top = 8; r.lid_bot = 8;
         return make(l, r, 80, Easing::EaseIn);
       }()},

      // SAD — bottom lids high, inward tilt.
      {ExpressionId::Sad, "sad",
       [] {
         Eye l = kBase(); l.lid_bot = 8; l.tilt = 30;
         Eye r = kBase(); r.lid_bot = 8; r.tilt = -30;
         return make(l, r, 250, Easing::EaseOut);
       }()},

      // DIZZY — exaggerated drift; the circular motion is applied by face engine.
      {ExpressionId::Dizzy, "dizzy",
       [] {
         Eye l = kBase(); l.w = 22; l.h = 42;
         Eye r = kBase(); r.w = 22; r.h = 42;
         return make(l, r, 1000, Easing::Linear);
       }()},

      // HEART — taller + squish + double-blink cadence handled by engine.
      {ExpressionId::Heart, "heart",
       [] {
         Eye l = kBase(); l.h = 52; l.squish = 160;
         Eye r = kBase(); r.h = 52; r.squish = 160;
         return make(l, r, 200, Easing::EaseOut);
       }()},

      // SCAN — base pose; face engine sweeps gaze left-right while this is active.
      {ExpressionId::Scan, "scan",
       [] {
         Eye l = kBase(); l.h = 44;
         Eye r = kBase(); r.h = 44;
         return make(l, r, 600, Easing::Linear);
       }()},

      // SURPRISE — large punch.
      {ExpressionId::Surprise, "surprise",
       [] {
         Eye l = kBase(); l.w = 28; l.h = 56;
         Eye r = kBase(); r.w = 28; r.h = 56;
         return make(l, r, 40, Easing::EaseOut);
       }()},

      // PROUD — happy + chin-up (eyes down 2, brow up 2).
      {ExpressionId::Proud, "proud",
       [] {
         Eye l = kBase(); l.lid_top = 4; l.lid_bot = 4; l.brow_y = -2; l.brow_tilt = -40; l.brow_len = 18;
         Eye r = kBase(); r.lid_top = 4; r.lid_bot = 4; r.brow_y = -2; r.brow_tilt = 40;  r.brow_len = 18;
         return make(l, r, 200, Easing::EaseOut);
       }()},

      {ExpressionId::GlyphCaretIn, "glyph_caret",
       [] {
         Eye l = kGlyphBase(EyeKind::CaretIn); l.tilt = -10;
         Eye r = kGlyphBase(EyeKind::CaretIn); r.tilt = 10;
         return make(l, r, 160, Easing::EaseOut);
       }()},

      {ExpressionId::GlyphArcUp, "glyph_arc",
       [] {
         Eye l = kGlyphBase(EyeKind::ArcUp, 28, 28);
         Eye r = kGlyphBase(EyeKind::ArcUp, 28, 28);
         return make(l, r, 160, Easing::EaseOut);
       }()},

      {ExpressionId::GlyphDash, "glyph_dash",
       [] {
         Eye l = kGlyphBase(EyeKind::Dash, 24, 16);
         Eye r = kGlyphBase(EyeKind::Dash, 24, 16);
         return make(l, r, 120, Easing::EaseOut);
       }()},

      {ExpressionId::GlyphCircle, "glyph_circle",
       [] {
         Eye l = kGlyphBase(EyeKind::Circle, 26, 30);
         Eye r = kGlyphBase(EyeKind::Circle, 26, 30);
         return make(l, r, 120, Easing::EaseOut);
       }()},

      {ExpressionId::GlyphWideCircle, "glyph_wide",
       [] {
         Eye l = kGlyphBase(EyeKind::WideCircle, 30, 30);
         Eye r = kGlyphBase(EyeKind::WideCircle, 30, 30);
         return make(l, r, 120, Easing::EaseOut);
       }()},

      {ExpressionId::GlyphCross, "glyph_cross",
       [] {
         Eye l = kGlyphBase(EyeKind::Cross, 24, 24);
         Eye r = kGlyphBase(EyeKind::Cross, 24, 24);
         return make(l, r, 140, Easing::EaseOut);
       }()},
  };
  return kTable;
}
}  // namespace

const ExpressionParams& get_expression(ExpressionId id) {
  const Entry* t = table();
  const auto idx = static_cast<uint8_t>(id);
  if(idx >= static_cast<uint8_t>(ExpressionId::Count)) {
    return t[0].params;
  }
  return t[idx].params;
}

const char* expression_name(ExpressionId id) {
  const Entry* t = table();
  const auto idx = static_cast<uint8_t>(id);
  if(idx >= static_cast<uint8_t>(ExpressionId::Count)) return "?";
  return t[idx].name;
}

void apply_expression(ExpressionId id,
                      int base_left_cx, int base_right_cx, int base_cy,
                      Eye& out_left, Eye& out_right) {
  const ExpressionParams& p = get_expression(id);
  out_left = p.left;
  out_right = p.right;
  out_left.cx  = static_cast<int16_t>(base_left_cx);
  out_left.cy  = static_cast<int16_t>(base_cy);
  out_right.cx = static_cast<int16_t>(base_right_cx);
  out_right.cy = static_cast<int16_t>(base_cy);
}

}  // namespace ui_v2
