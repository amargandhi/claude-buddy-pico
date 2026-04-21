// permissions.cpp — permissions history list.
#include "permissions.h"

#include <cstdio>

#include "../chrome.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"
#include "../permissions_log.h"

namespace ui_v2::screens::permissions {

namespace {

uint32_t g_scroll = 0;   // index of topmost row shown

void enter(uint32_t /*now_ms*/) { g_scroll = 0; }

Rgb24 tier_color(RiskTier t) {
  switch(t) {
    case RiskTier::Safe:    return kOkGreen;
    case RiskTier::Neutral: return kBusyBlue;
    case RiskTier::Caution: return kWarnAmber;
    case RiskTier::Danger:  return kDangerRed;
  }
  return kDangerRed;
}

const char* decision_glyph(perms::Decision d) {
  switch(d) {
    case perms::Decision::Approved:  return "OK";
    case perms::Decision::Denied:    return "NO";
    case perms::Decision::Timeout:   return "TO";
    case perms::Decision::Dismissed: return "--";
  }
  return "?";
}

Rgb24 decision_color(perms::Decision d) {
  switch(d) {
    case perms::Decision::Approved:  return kOkGreen;
    case perms::Decision::Denied:    return kDangerRed;
    case perms::Decision::Timeout:   return kWarnAmber;
    case perms::Decision::Dismissed: return kTextDim;
  }
  return kTextDim;
}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& /*face*/,
          CoreIntent& /*intent*/, uint32_t now_ms) {
  gfx::clear(kBgInk);

  const uint32_t total = perms::count();
  gfx::text("PERMISSIONS", geom::kPadOuter, geom::kContentY + 4, kTextDim, 1.0f);
  char count_buf[24];
  std::snprintf(count_buf, sizeof(count_buf), "%u recent", total);
  gfx::text_aligned(count_buf, 0, geom::kContentY + 4, geom::kPanelW - 12,
                    gfx::Align::Right, kTextDim, 1.0f);

  // Rows: 16 px tall, starts at y=34, 11 rows fit before footer.
  const int row_h = 16;
  const int y0    = 34;
  const int rows  = 11;

  if(total == 0) {
    gfx::text_aligned("no activity yet", 0, 108, geom::kPanelW,
                      gfx::Align::Center, kTextDim, 1.0f);
  } else {
    const uint32_t first = g_scroll;
    const uint32_t end   = (first + rows < total) ? (first + rows) : total;
    for(uint32_t i = first; i < end; ++i) {
      const perms::Entry e = perms::get(i);
      const int y = y0 + static_cast<int>(i - first) * row_h;

      // Tier color bar on the left.
      const RiskTier tier = perms::tier_of(e.tool_id);
      gfx::fill_rect(0, y, 4, row_h - 2, tier_color(tier));

      // Decision glyph.
      const perms::Decision d = static_cast<perms::Decision>(e.decision);
      gfx::text(decision_glyph(d), 8, y + 2, decision_color(d), 1.0f);

      // Tool name.
      gfx::text(perms::name_of(e.tool_id), 30, y + 2, kTextPrimary, 1.0f);

      // Wait.
      char wait[16];
      if(e.wait_ms >= 10000) std::snprintf(wait, sizeof(wait), "%us", e.wait_ms / 1000);
      else                   std::snprintf(wait, sizeof(wait), "%u.%us",
                                           e.wait_ms / 1000,
                                           (e.wait_ms % 1000) / 100);
      gfx::text_aligned(wait, 160, y + 2, 60, gfx::Align::Right, kTextDim, 1.0f);

      // Age.
      const uint32_t age_s = (now_ms >= e.timestamp_ms) ?
                             (now_ms - e.timestamp_ms) / 1000 : 0;
      char age[16];
      if(age_s < 60)       std::snprintf(age, sizeof(age), "%us", age_s);
      else if(age_s < 3600)std::snprintf(age, sizeof(age), "%um", age_s / 60);
      else                 std::snprintf(age, sizeof(age), "%uh", age_s / 3600);
      gfx::text_aligned(age, 240, y + 2, 72, gfx::Align::Right, kTextDim, 1.0f);
    }
  }

  // X goes to Info, not a generic "Next".
  chrome::draw_status_rail(in, "Menu", "Info", now_ms);
  chrome::draw_footer("Page", "Home");
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& /*out*/,
               CoreIntent& intent, uint32_t /*now_ms*/) {
  if(edge != ButtonEdge::ShortTap) return;
  if(btn == Button::A) { intent.push = true; intent.target = ScreenId::SystemMenu; }
  else if(btn == Button::B) {
    const uint32_t total = perms::count();
    const uint32_t rows  = 11;
    if(total > rows) {
      g_scroll += rows;
      if(g_scroll >= total) g_scroll = 0;
    }
  }
  else if(btn == Button::X) { intent.replace = true; intent.target = ScreenId::Info; }
  else if(btn == Button::Y) { intent.replace = true; intent.target = ScreenId::Home; }
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::permissions
