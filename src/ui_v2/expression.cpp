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
       [] {
         Eye l = kBase(); l.h = 46; l.lid_top = 2; l.lid_bot = 1;
         Eye r = kBase(); r.h = 46; r.lid_top = 2; r.lid_bot = 1;
         return make(l, r, 180, Easing::EaseOut);
       }()},

      // HAPPY — relaxed lids and a softer outward lift.
      {ExpressionId::Happy, "happy",
       [] {
         Eye l = kBase(); l.w = 26; l.h = 40; l.lid_top = 10; l.lid_bot = 4; l.squish = 146; l.tilt = -40;
         Eye r = kBase(); r.w = 26; r.h = 40; r.lid_top = 10; r.lid_bot = 4; r.squish = 146; r.tilt = 40;
         return make(l, r, 150, Easing::EaseOut);
       }()},

      // JOY — a fuller, more playful squish than happy.
      {ExpressionId::Joy, "joy",
       [] {
         Eye l = kBase(); l.w = 28; l.h = 40; l.lid_top = 12; l.lid_bot = 6; l.squish = 170; l.tilt = -70;
         Eye r = kBase(); r.w = 28; r.h = 40; r.lid_top = 12; r.lid_bot = 6; r.squish = 170; r.tilt = 70;
         return make(l, r, 180, Easing::Spring);
       }()},

      // FOCUSED — narrower slit with a deliberate brow.
      {ExpressionId::Focused, "focused",
       [] {
         Eye l = kBase(); l.h = 52; l.w = 20; l.lid_top = 4; l.lid_bot = 2;
         l.brow_y = -3; l.brow_tilt = -24; l.brow_len = 16;
         Eye r = kBase(); r.h = 52; r.w = 20; r.lid_top = 4; r.lid_bot = 2;
         r.brow_y = -3; r.brow_tilt = 24; r.brow_len = 16;
         return make(l, r, 120, Easing::EaseOut);
       }()},

      // ATTENTIVE — taller and a touch wider with bright, open lids.
      {ExpressionId::Attentive, "attentive",
       [] {
         Eye l = kBase(); l.w = 26; l.h = 54; l.r = 3; l.lid_top = 2;
         Eye r = kBase(); r.w = 26; r.h = 54; r.r = 3; r.lid_top = 2;
         return make(l, r, 160, Easing::EaseOut);
       }()},

      // ALERT — biggest, clearest open pose.
      {ExpressionId::Alert, "alert",
       [] {
         Eye l = kBase(); l.w = 30; l.h = 56; l.r = 3; l.tilt = 24;
         l.brow_y = -6; l.brow_tilt = 42; l.brow_len = 20;
         Eye r = kBase(); r.w = 30; r.h = 56; r.r = 3; r.tilt = -24;
         r.brow_y = -6; r.brow_tilt = -42; r.brow_len = 20;
         return make(l, r, 110, Easing::EaseOut);
       }()},

      // SKEPTICAL — asymmetry does the acting work.
      {ExpressionId::Skeptical, "skeptical",
       [] {
         Eye l = kBase(); l.lid_top = 7; l.lid_bot = 1;
         l.brow_y = 1; l.brow_tilt = 80; l.brow_len = 18;
         Eye r = kBase(); r.lid_top = 2; r.lid_bot = 6;
         r.brow_y = -5; r.brow_tilt = -70; r.brow_len = 18;
         return make(l, r, 200, Easing::EaseInOut);
       }()},

      // SLEEPY — heavy top lids.
      {ExpressionId::Sleepy, "sleepy",
       [] {
         Eye l = kBase(); l.h = 44; l.lid_top = 16; l.lid_bot = 4;
         Eye r = kBase(); r.h = 44; r.lid_top = 16; r.lid_bot = 4;
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
         Eye l = kBase(); l.w = 22; l.h = 30; l.lid_top = 12; l.lid_bot = 12;
         l.brow_y = -3; l.brow_tilt = 26; l.brow_len = 16;
         Eye r = kBase(); r.w = 22; r.h = 30; r.lid_top = 12; r.lid_bot = 12;
         r.brow_y = -3; r.brow_tilt = -26; r.brow_len = 16;
         return make(l, r, 90, Easing::EaseIn);
       }()},

      // SAD — heavier lower lid and softened angle.
      {ExpressionId::Sad, "sad",
       [] {
         Eye l = kBase(); l.h = 44; l.lid_top = 4; l.lid_bot = 10; l.tilt = 25;
         l.brow_y = -2; l.brow_tilt = 34; l.brow_len = 18;
         Eye r = kBase(); r.h = 44; r.lid_top = 4; r.lid_bot = 10; r.tilt = -25;
         r.brow_y = -2; r.brow_tilt = -34; r.brow_len = 18;
         return make(l, r, 250, Easing::EaseOut);
       }()},

      // DIZZY — exaggerated drift; the circular motion is applied by face engine.
      {ExpressionId::Dizzy, "dizzy",
       [] {
         Eye l = kBase(); l.w = 22; l.h = 42;
         Eye r = kBase(); r.w = 22; r.h = 42;
         return make(l, r, 1000, Easing::Linear);
       }()},

      // HEART — fullest positive pose; the cadence lives in the beat layer.
      {ExpressionId::Heart, "heart",
       [] {
         Eye l = kBase(); l.w = 26; l.h = 52; l.lid_top = 5; l.lid_bot = 1; l.squish = 170;
         l.brow_y = -4; l.brow_tilt = -18; l.brow_len = 18;
         Eye r = kBase(); r.w = 26; r.h = 52; r.lid_top = 5; r.lid_bot = 1; r.squish = 170;
         r.brow_y = -4; r.brow_tilt = 18; r.brow_len = 18;
         return make(l, r, 220, Easing::EaseOut);
       }()},

      // SCAN — narrower sensor-like gaze pose.
      {ExpressionId::Scan, "scan",
       [] {
         Eye l = kBase(); l.w = 22; l.h = 44; l.r = 2; l.lid_top = 3; l.lid_bot = 3;
         Eye r = kBase(); r.w = 22; r.h = 44; r.r = 2; r.lid_top = 3; r.lid_bot = 3;
         return make(l, r, 260, Easing::EaseInOut);
       }()},

      // SURPRISE — large punch.
      {ExpressionId::Surprise, "surprise",
       [] {
         Eye l = kBase(); l.w = 28; l.h = 56;
         Eye r = kBase(); r.w = 28; r.h = 56;
         return make(l, r, 40, Easing::EaseOut);
       }()},

      // PROUD — settled confidence.
      {ExpressionId::Proud, "proud",
       [] {
         Eye l = kBase(); l.w = 26; l.h = 44; l.lid_top = 6; l.lid_bot = 4;
         l.brow_y = -4; l.brow_tilt = -55; l.brow_len = 18;
         Eye r = kBase(); r.w = 26; r.h = 44; r.lid_top = 6; r.lid_bot = 4;
         r.brow_y = -4; r.brow_tilt = 55;  r.brow_len = 18;
         return make(l, r, 220, Easing::EaseOut);
       }()},

      // CONCERNED — delayed approval with worry instead of panic.
      {ExpressionId::Concerned, "concerned",
       [] {
         Eye l = kBase(); l.w = 22; l.h = 46; l.lid_top = 8; l.lid_bot = 4;
         l.brow_y = -2; l.brow_tilt = 64; l.brow_len = 18;
         Eye r = kBase(); r.w = 22; r.h = 46; r.lid_top = 8; r.lid_bot = 4;
         r.brow_y = -2; r.brow_tilt = -64; r.brow_len = 18;
         return make(l, r, 220, Easing::EaseInOut);
       }()},

      // RELIEF — a softer reward than heart, used after non-instant approvals.
      {ExpressionId::Relief, "relief",
       [] {
         Eye l = kBase(); l.w = 26; l.h = 46; l.lid_top = 5; l.lid_bot = 3; l.squish = 146; l.tilt = -15;
         l.brow_y = -3; l.brow_tilt = -25; l.brow_len = 18;
         Eye r = kBase(); r.w = 26; r.h = 46; r.lid_top = 5; r.lid_bot = 3; r.squish = 146; r.tilt = 15;
         r.brow_y = -3; r.brow_tilt = 25; r.brow_len = 18;
         return make(l, r, 180, Easing::EaseOut);
       }()},

      {ExpressionId::GlyphCaretIn, "glyph_caret",
       [] {
         Eye l = kGlyphBase(EyeKind::CaretIn, 22, 28); l.tilt = -10; l.stroke_px = 3;
         Eye r = kGlyphBase(EyeKind::CaretIn, 22, 28); r.tilt = 10; r.stroke_px = 3;
         return make(l, r, 180, Easing::EaseInOut);
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
