// stats.cpp — stats screen.
#include "stats.h"

#include <cstdio>

#include "../chrome.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"

namespace ui_v2::screens::stats {

namespace {

void enter(uint32_t /*now_ms*/) {}

void draw_stat(int x, int y, int w, const char* caption, const char* value,
               Rgb24 caption_c, Rgb24 value_c) {
  gfx::text(caption, x, y, caption_c, 1.0f);
  gfx::text_aligned(value, x, y + 12, w, gfx::Align::Left, value_c, 2.4f);
}

void format_heartbeat_age(char* out, size_t out_sz, uint32_t age_ms) {
  if(out_sz == 0) return;
  if(age_ms < 1000) {
    std::snprintf(out, out_sz, "now");
    return;
  }
  gfx::format_mmss(out, out_sz, age_ms / 1000u);
}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& /*face*/,
          CoreIntent& /*intent*/, uint32_t now_ms) {
  gfx::clear(kBgInk);

  const int lx = geom::kColLeftX + 4;
  const int rx = geom::kColRightX + 4;
  const int cw = geom::kColLeftW - 8;

  // Title strip — serif-ish vibe: scale 1.3 with a clay underline.
  gfx::text("STATS", lx, geom::kContentY + 4, kAccentClay, 1.3f);
  gfx::fill_rect(lx, geom::kContentY + 18, 52, 2, kAccentClay);

  // Left column values — scale 2.4 numerics, 42 px row pitch.
  char buf[24];

  gfx::format_thousands(buf, sizeof(buf), in.total);
  draw_stat(lx, 42, cw, "HEARTBEATS", buf, kTextDim, kTextPrimary);

  std::snprintf(buf, sizeof(buf), "%u", in.running);
  draw_stat(lx, 82, cw, "RUNNING", buf, kTextDim, kBusyBlue);

  std::snprintf(buf, sizeof(buf), "%u", in.waiting);
  draw_stat(lx, 122, cw, "WAITING", buf, kTextDim, kWarnAmber);

  gfx::format_compact(buf, sizeof(buf), in.tokens_today);
  draw_stat(lx, 162, cw, "TOKENS TODAY", buf, kTextDim, kOkGreen);

  // Right column: live activity + approve/deny.
  gfx::text("ACTIVITY", rx, geom::kContentY + 4, kTextDim, 1.2f);

  char rate[24];
  std::snprintf(rate, sizeof(rate), "%u / min", static_cast<unsigned>(in.token_rate_per_min));
  gfx::text("LIVE RATE", rx, 44, kTextDim, 1.0f);
  gfx::text_aligned(rate, rx, 56, geom::kColRightW - 8,
                    gfx::Align::Left, kAccentClay, 1.8f);

  char hb_age[24];
  format_heartbeat_age(hb_age, sizeof(hb_age), in.heartbeat_age_ms);
  gfx::text("LAST HEARTBEAT", rx, 92, kTextDim, 1.0f);
  gfx::text_aligned(hb_age, rx, 104, geom::kColRightW - 8,
                    gfx::Align::Left, kTextPrimary, 1.8f);

  // Approve/Deny bars.
  const uint32_t ad_max = (in.approvals > in.denials ? in.approvals : in.denials) + 1;
  gfx::text("APPROVE", rx, 138, kTextDim, 1.0f);
  gfx::progress_bar(rx, 150, geom::kColRightW - 8, 6, in.approvals, ad_max,
                    kOkGreen, kChipPanel);
  char ab[16];
  gfx::format_thousands(ab, sizeof(ab), in.approvals);
  gfx::text(ab, rx, 158, kOkGreen, 1.5f);

  gfx::text("DENY", rx, 176, kTextDim, 1.0f);
  gfx::progress_bar(rx, 188, geom::kColRightW - 8, 6, in.denials, ad_max,
                    kDangerRed, kChipPanel);
  char db[16];
  gfx::format_thousands(db, sizeof(db), in.denials);
  gfx::text(db, rx, 196, kDangerRed, 1.5f);

  // X goes to Usage, not some vague "Next" screen.
  chrome::draw_status_rail(in, "Menu", "Usage", now_ms);
  chrome::draw_footer("Buddy", "Home");
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& /*out*/,
               CoreIntent& intent, uint32_t /*now_ms*/) {
  if(edge != ButtonEdge::ShortTap) return;
  if(btn == Button::A) { intent.push = true; intent.target = ScreenId::SystemMenu; }
  else if(btn == Button::B) { intent.replace = true; intent.target = ScreenId::Buddy; }
  else if(btn == Button::X) { intent.replace = true; intent.target = ScreenId::Usage; }
  else if(btn == Button::Y) { intent.replace = true; intent.target = ScreenId::Home; }
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::stats
