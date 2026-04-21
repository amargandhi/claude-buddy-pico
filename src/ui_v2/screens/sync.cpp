// sync.cpp — pairing / passkey screen.
#include "sync.h"

#include <cstdio>
#include <cstring>

#include "../chrome.h"
#include "../face.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"
#include "../transitions.h"

namespace ui_v2::screens::sync {

namespace {

uint32_t g_linked_ms = 0;  // timestamp of first LinkedSecure observation
bool     g_handoff_armed = false;
transitions::MergeWipe g_handoff_wipe;

void enter(uint32_t now_ms) {
  g_linked_ms      = 0;
  g_handoff_armed  = false;
  g_handoff_wipe   = transitions::MergeWipe();
  (void)now_ms;
}

void draw_passkey(const char* passkey) {
  // Large, centered, monospaced look. We rely on the fixed-width font
  // path at a larger scale.
  const float scale = 4.0f;
  const int w = gfx::text_width(passkey, scale, true);
  const int x = (geom::kPanelW - w) / 2;
  const int y = 116;
  gfx::text(passkey, x, y, kTextPrimary, scale, true);

  // Baseline underline to make the digits feel like a display.
  gfx::fill_rect(x - 4, y + 32, w + 8, 2, kCaseOrange);

  // Sub-copy.
  gfx::text_aligned("Enter on your Mac", 0, y + 44, geom::kPanelW,
                    gfx::Align::Center, kTextDim, 1.0f);
}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& face,
          CoreIntent& intent, uint32_t now_ms) {
  gfx::clear(kBgInk);

  // Face: hold Attentive while pairing; Happy when linked.
  if(in.link == LinkState::LinkedSecure || in.link == LinkState::LinkedInsecure) {
    const ExpressionId want = (in.face_style == FaceStyle::Classic)
        ? ExpressionId::Happy
        : ExpressionId::GlyphArcUp;
    if(face.target != want) {
      face::begin_transition(face, want, now_ms, 200);
    }
  } else if(in.link == LinkState::PairingPasskey) {
    const ExpressionId want = (in.face_style == FaceStyle::Classic)
        ? ExpressionId::Attentive
        : ExpressionId::GlyphCircle;
    if(face.target != want) {
      face::begin_transition(face, want, now_ms, 150);
    }
  } else if(in.link == LinkState::Advertising || in.link == LinkState::Connecting) {
    if(face.target != ExpressionId::Scan) {
      face::begin_transition(face, ExpressionId::Scan, now_ms, 200);
    }
  } else {
    if(face.target != ExpressionId::Sleepy) {
      face::begin_transition(face, ExpressionId::Sleepy, now_ms, 250);
    }
  }

  // Ambient saccades off while pairing — passkey must be steady.
  face::set_ambient(face, true, in.link != LinkState::PairingPasskey);

  face::tick_and_render(face, now_ms);

  // Below-face content.
  const char* header = nullptr;
  switch(in.link) {
    case LinkState::NotAdvertising: header = "Idle"; break;
    case LinkState::Advertising:    header = "Looking for Claude..."; break;
    case LinkState::Connecting:     header = "Connecting..."; break;
    case LinkState::PairingPasskey: header = "Pairing"; break;
    case LinkState::LinkedInsecure: header = "Linked (unencrypted)"; break;
    case LinkState::LinkedSecure:   header = "Linked"; break;
  }
  gfx::text_aligned(header, 0, geom::kBelowFaceY, geom::kPanelW,
                    gfx::Align::Center, kTextWarm, 1.0f);

  if(in.link == LinkState::PairingPasskey && in.passkey[0]) {
    draw_passkey(in.passkey);
  } else if(in.link == LinkState::LinkedSecure) {
    gfx::text_aligned("Ready.", 0, 132, geom::kPanelW,
                      gfx::Align::Center, kOkGreen, 2.0f);
  }

  const char* a_hint = nullptr;
  if(in.link == LinkState::NotAdvertising) a_hint = "Advertise";
  else if(in.link == LinkState::Advertising) a_hint = "Stop";
  chrome::draw_status_rail(in, a_hint, nullptr, now_ms);
  chrome::draw_footer(nullptr, "Skip");

  // Handoff to Home once paired for ~800 ms.
  if(in.link == LinkState::LinkedSecure) {
    if(g_linked_ms == 0) g_linked_ms = now_ms;
    if(!g_handoff_armed && (now_ms - g_linked_ms) >= 800) {
      g_handoff_wipe.start(now_ms, 500);
      g_handoff_armed = true;
    }
  } else {
    g_linked_ms = 0;
  }

  if(g_handoff_armed) {
    g_handoff_wipe.draw(now_ms);
    if(g_handoff_wipe.is_done(now_ms)) {
      intent.replace = true;
      intent.target  = ScreenId::Home;
    }
  }
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& out,
               CoreIntent& intent, uint32_t /*now_ms*/) {
  if(edge != ButtonEdge::ShortTap) return;

  if(btn == Button::A) {
    out.set_advertising = true;
    // V1 flips this into start/stop only while disconnected; every connected
    // state leaves A unlabeled and thus unreachable.
    out.advertising_enabled = true;
  } else if(btn == Button::Y) {
    intent.replace = true;
    intent.target  = ScreenId::Home;
  }
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::sync
