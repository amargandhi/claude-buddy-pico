// dock_clock.cpp — low-energy clock surface.
//
// Liveliness: a digital clock with no motion is indistinguishable from a
// screenshot. On a dock the user glances, not stares, so the "is this
// alive?" question has to be answered in under a second. We give the
// colon a 1 Hz blink (on 500 ms / off 500 ms) — the classic iPod-era
// digital-clock tell. A short date line ("Mon Apr 20") sits under the
// clock and the owner/device name moves below that; the pyramid reads
// top-to-bottom as decreasing urgency (current time → today → whose).
#include "dock_clock.h"

#include <cstdio>
#include <ctime>

#include "../chrome.h"
#include "../face.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"

namespace ui_v2::screens::dock_clock {

namespace {

constexpr const char* kDays[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
constexpr const char* kMonths[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};

void enter(uint32_t /*now_ms*/) {}

// Decompose H, M from a local epoch. Same math as gfx::format_clock_hhmm
// but returns the parts separately so we can draw the colon ourselves
// and blink it independently of the digits.
void epoch_to_hm(uint32_t epoch_local, int& h, int& m) {
  const uint32_t secs_of_day = epoch_local % 86400u;
  h = static_cast<int>(secs_of_day / 3600u);
  m = static_cast<int>((secs_of_day % 3600u) / 60u);
}

// Cheap date formatter. Uses std::gmtime on a time_t built from the
// already-local-adjusted epoch (V1 computes current_local_epoch so the
// epoch we receive is in local time even though we feed it to gmtime,
// which is intentional — gmtime decomposes its argument without applying
// another timezone offset).
void format_date_short(char* out, size_t n, uint32_t epoch_local) {
  if(!out || n == 0) return;
  const std::time_t t = static_cast<std::time_t>(epoch_local);
  std::tm* tm = std::gmtime(&t);
  if(!tm) { std::snprintf(out, n, " "); return; }
  const int wday  = tm->tm_wday  & 0x07;
  const int mon   = tm->tm_mon   % 12;
  std::snprintf(out, n, "%s %s %d", kDays[wday], kMonths[mon], tm->tm_mday);
}

void tick(const BuddyInputs& in, BuddyOutputs& out, FaceState& face,
          CoreIntent& intent, uint32_t now_ms) {
  gfx::clear(kBgInk);

  // Dock should feel calm and glanceable, not like an eye demo reel.
  // Render a snapped neutral pose only; the colon is the only regular
  // motion on this screen.
  face::set_ambient(face, false, false);
  face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kDockEyeCy);
  face::set_ink(face, kTextPrimary);
  face::snap(face, ExpressionId::Neutral, now_ms);
  face::render_only(face, now_ms);

  // Big clock. We draw HH, MM, and the colon separately so the colon
  // can blink at 1 Hz independent of the digits. 5.0x scale; bitmap6
  // glyph width at 5.0 ≈ 30 px; HH and MM are each ~60 px wide. The
  // colon itself is ~10 px wide. We center the composite on the panel.
  constexpr float kClkScale = 5.0f;
  const int clk_y = 116;
  char hh[4], mm[4];
  if(in.rtc_valid) {
    int h, m;
    epoch_to_hm(in.rtc_local_epoch, h, m);
    std::snprintf(hh, sizeof(hh), "%02d", h);
    std::snprintf(mm, sizeof(mm), "%02d", m);
  } else {
    std::snprintf(hh, sizeof(hh), "--");
    std::snprintf(mm, sizeof(mm), "--");
  }
  const int w_hh    = gfx::text_width(hh, kClkScale);
  const int w_mm    = gfx::text_width(mm, kClkScale);
  const int w_colon = gfx::text_width(":", kClkScale);
  const int clk_w   = w_hh + w_colon + w_mm;
  const int clk_x   = (geom::kPanelW - clk_w) / 2;

  gfx::text(hh, clk_x,                    clk_y, kTextPrimary, kClkScale);
  // Colon on for the first half of every 1000 ms window. If the RTC is
  // not valid, we still blink — blinking dashes beats static dashes for
  // signaling "clock is alive but time is unknown."
  const bool colon_on = (now_ms % 1000u) < 500u;
  gfx::text(":", clk_x + w_hh, clk_y,
            colon_on ? kTextPrimary : kBgInk, kClkScale);
  gfx::text(mm, clk_x + w_hh + w_colon, clk_y, kTextPrimary, kClkScale);

  // Date line — small, warm text under the clock. Only drawn when the
  // RTC is valid; otherwise a fake date would be a lie.
  if(in.rtc_valid) {
    char date_buf[32];
    format_date_short(date_buf, sizeof(date_buf), in.rtc_local_epoch);
    gfx::text_aligned(date_buf, 0, 168, geom::kPanelW, gfx::Align::Center,
                      kTextWarm, 1.2f);
  }

  // Owner line — pushed down 16 px to make room for the date above it.
  gfx::text_aligned(in.owner[0] ? in.owner : in.device_name,
                    0, 186, geom::kPanelW, gfx::Align::Center,
                    kTextDim, 1.0f);

  // Dim the backlight a notch while on dock.
  out.set_backlight = true;
  out.backlight_level = 140;

  chrome::draw_status_rail(in, "Menu", nullptr, now_ms);
  chrome::draw_footer(nullptr, "Home");

  // If battery-powered (unplugged), hand back to Home.
  if(in.power == PowerState::Battery) {
    intent.replace = true;
    intent.target  = ScreenId::Home;
  }
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& /*out*/,
               CoreIntent& intent, uint32_t /*now_ms*/) {
  if(edge != ButtonEdge::ShortTap) return;
  if(btn == Button::A) { intent.push    = true; intent.target = ScreenId::SystemMenu; }
  else if(btn == Button::Y) { intent.replace = true; intent.target = ScreenId::Home; }
}

void exit(uint32_t /*now_ms*/) {
  // ui_core will restore face base position on next screen enter; nothing to do.
}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::dock_clock
