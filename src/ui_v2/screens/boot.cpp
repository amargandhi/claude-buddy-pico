// boot.cpp — implementation of the boot sequence.
#include "boot.h"

#include "../chrome.h"
#include "../face.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"
#include "../transitions.h"

namespace ui_v2::screens::boot {

namespace {

uint32_t g_start_ms = 0;

void enter(uint32_t now_ms) {
  g_start_ms = now_ms;
}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& face,
          CoreIntent& intent, uint32_t now_ms) {
  const uint32_t t = now_ms - g_start_ms;

  gfx::clear(kBgInk);
  face::set_ambient(face, false, false);

  if(t < 400) {
    gfx::fill_rect(0, 0, geom::kPanelW, geom::kPanelH, kCaseOrange);
  } else if(t < 720) {
    const float k = 1.0f - (static_cast<float>(t - 400) / 320.0f);
    const int rw = static_cast<int>(static_cast<float>(geom::kPanelW) * k);
    const int rh = static_cast<int>(static_cast<float>(geom::kPanelH) * k);
    const int x0 = geom::kPanelW / 2 - rw / 2;
    const int y0 = geom::kPanelH / 2 - rh / 2;
    gfx::fill_rect(x0, y0, rw, rh, kCaseOrange);
  }

  if(t >= 400) {
    const bool glyph_settle = (in.face_style != FaceStyle::Classic);
    if(t < 720) {
      face::set_ink(face, kBgInkDeep);
      if(face.target != ExpressionId::Attentive) {
        face::begin_transition(face, ExpressionId::Attentive, now_ms, 180);
      }
    } else if(t < 960) {
      face::set_ink(face, glyph_settle ? kBgInkDeep : kTextPrimary);
      const ExpressionId settle = glyph_settle ? ExpressionId::GlyphArcUp
                                               : ExpressionId::Happy;
      if(face.target != settle) {
        face::begin_transition(face, settle, now_ms, 180);
      }
    } else {
      face::set_ink(face, kTextPrimary);
      if(face.target != ExpressionId::Neutral) {
        face::begin_transition(face, ExpressionId::Neutral, now_ms, 220);
      }
    }
    face::tick_and_render(face, now_ms);
  }

  if(t >= 1100) {
    const float k = static_cast<float>(t - 1100) / 400.0f;
    const float e = (k < 0.0f) ? 0.0f : (k > 1.0f ? 1.0f : 1.0f - (1.0f - k) * (1.0f - k));
    const int final_y = geom::kBelowFaceY + 20;
    const int start_y = geom::kPanelH + 4;
    const int y = start_y + static_cast<int>(static_cast<float>(final_y - start_y) * e);
    const int w = 160;
    const int x = (geom::kPanelW - w) / 2;
    gfx::rounded_rect(x, y, w, 22, 4, kChipPanel);
    const char* name = (in.device_name[0] ? in.device_name : "Claude Pico");
    gfx::text_aligned(name, x, y + 6, w, gfx::Align::Center, kTextPrimary, 1.0f);
  }

  // Chrome stays hidden during boot — it arrives with Home.

  if(t >= 1500) {
    // Pick next screen: Sync if not paired, Home otherwise.
    intent.replace = true;
    const bool paired = (in.link == LinkState::LinkedSecure ||
                         in.link == LinkState::LinkedInsecure);
    intent.target = paired ? ScreenId::Home : ScreenId::Sync;
  }
}

void on_button(Button /*btn*/, ButtonEdge /*edge*/, BuddyOutputs& /*out*/,
               CoreIntent& /*intent*/, uint32_t /*now_ms*/) {
  // Boot consumes all input.
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::boot
