// usage.cpp — tokens today / total.
#include "usage.h"

#include <cstdio>

#include "../chrome.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"

namespace ui_v2::screens::usage {

namespace {

void enter(uint32_t /*now_ms*/) {}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& /*face*/,
          CoreIntent& /*intent*/, uint32_t now_ms) {
  gfx::clear(kBgInk);

  gfx::text("TOKENS", geom::kPadOuter, geom::kContentY + 4, kTextDim, 1.0f);

  // Big "today" number, centered.
  char today[24];
  gfx::format_thousands(today, sizeof(today), in.tokens_today);
  gfx::text_aligned(today, 0, 52, geom::kPanelW, gfx::Align::Center,
                    kOkGreen, 4.0f);
  gfx::text_aligned("today", 0, 96, geom::kPanelW, gfx::Align::Center,
                    kTextDim, 1.0f);

  // Total, smaller, below.
  char total[24];
  gfx::format_thousands(total, sizeof(total), in.tokens_total);
  gfx::text_aligned(total, 0, 118, geom::kPanelW, gfx::Align::Center,
                    kTextPrimary, 2.5f);
  gfx::text_aligned("lifetime", 0, 150, geom::kPanelW, gfx::Align::Center,
                    kTextDim, 1.0f);

  // Today-vs-total proportion bar. Honest: no quota, so the bar
  // is today / total only.
  if(in.tokens_total > 0) {
    const int bx = geom::kPadOuter + 20;
    const int bw = geom::kPanelW - (geom::kPadOuter + 20) * 2;
    gfx::progress_bar(bx, 172, bw, 6, in.tokens_today, in.tokens_total,
                      kCaseOrange, kChipPanel);
    char caption[48];
    std::snprintf(caption, sizeof(caption), "today / lifetime");
    gfx::text_aligned(caption, 0, 184, geom::kPanelW, gfx::Align::Center,
                      kTextDim, 1.0f);
  }

  // Note: deliberately no "usage limit" — upstream doesn't expose one.
  gfx::text_aligned("no quota data from host", 0, 202, geom::kPanelW,
                    gfx::Align::Center, kTextDim, 1.0f);

  // X goes to Permissions, Y goes to Stats — label accordingly. "Back" was
  // wrong because Y does not return to the prior screen, it navigates to
  // a specific sibling.
  chrome::draw_status_rail(in, "Menu", "Perms", now_ms);
  chrome::draw_footer("Buddy", "Stats");
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& /*out*/,
               CoreIntent& intent, uint32_t /*now_ms*/) {
  if(edge != ButtonEdge::ShortTap) return;
  if(btn == Button::A) { intent.push = true; intent.target = ScreenId::SystemMenu; }
  else if(btn == Button::B) { intent.replace = true; intent.target = ScreenId::Buddy; }
  else if(btn == Button::X) { intent.replace = true; intent.target = ScreenId::Permissions; }
  else if(btn == Button::Y) { intent.replace = true; intent.target = ScreenId::Stats; }
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::usage
