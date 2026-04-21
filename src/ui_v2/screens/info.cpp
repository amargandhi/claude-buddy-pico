// info.cpp — device info.
#include "info.h"

#include <cstdio>

#include "../chrome.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"

namespace ui_v2::screens::info {

namespace {

#ifndef BUDDY_V2_VERSION_STRING
#define BUDDY_V2_VERSION_STRING "v2.0.0-dev"
#endif

void enter(uint32_t /*now_ms*/) {}

void draw_row(int y, const char* label, const char* value) {
  gfx::text(label, geom::kPadOuter, y, kTextDim, 1.0f);
  gfx::text_aligned(value, 0, y, geom::kPanelW - geom::kPadOuter,
                    gfx::Align::Right, kTextPrimary, 1.0f);
}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& /*face*/,
          CoreIntent& /*intent*/, uint32_t now_ms) {
  gfx::clear(kBgInk);

  gfx::text("INFO", geom::kPadOuter, geom::kContentY + 4, kTextDim, 1.0f);

  int y = 40;
  draw_row(y, "Name",  in.device_name); y += 18;
  draw_row(y, "Owner", in.owner[0] ? in.owner : "(not set)"); y += 18;
  draw_row(y, "FW",    BUDDY_V2_VERSION_STRING); y += 18;

  char up[24];
  gfx::format_seconds_hms(up, sizeof(up), now_ms / 1000);
  draw_row(y, "Uptime", up); y += 18;

  char batt[32];
  if(in.battery_mv > 0) {
    std::snprintf(batt, sizeof(batt), "%u%%  %u mV",
                  in.battery_pct, in.battery_mv);
  } else {
    std::snprintf(batt, sizeof(batt), "%u%%", in.battery_pct);
  }
  draw_row(y, "Battery", batt); y += 18;

  const char* pwr = (in.power == PowerState::UsbCharging) ? "USB +"
                 : (in.power == PowerState::UsbPowered)  ? "USB"
                 : "Battery";
  draw_row(y, "Power", pwr); y += 18;

  if(in.rtc_valid) {
    char clk[16];
    gfx::format_clock_hhmm(clk, sizeof(clk), in.rtc_local_epoch);
    draw_row(y, "Clock", clk);
  } else {
    draw_row(y, "Clock", "(no RTC)");
  }

  // Rail labels mirror the actual handlers below.
  //   A (top-left)    → push SystemMenu   → "Menu"
  //   X (top-right)   → unbound           → (blank)
  //   B (bottom-left) → replace Buddy     → "Buddy"
  //   Y (bottom-right)→ replace Home      → "Home"
  // Previously X showed "Next" but the handler went Home, duplicating Y.
  chrome::draw_status_rail(in, "Menu", nullptr, now_ms);
  chrome::draw_footer("Buddy", "Home");
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& /*out*/,
               CoreIntent& intent, uint32_t /*now_ms*/) {
  if(edge != ButtonEdge::ShortTap) return;
  if(btn == Button::A) { intent.push = true; intent.target = ScreenId::SystemMenu; }
  else if(btn == Button::B) { intent.replace = true; intent.target = ScreenId::Buddy; }
  else if(btn == Button::Y) { intent.replace = true; intent.target = ScreenId::Home; }
  // X deliberately unbound (see rail labels).
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::info
