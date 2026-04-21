// chrome.cpp — status rail and footer implementations.
//
// The four physical buttons sit at the case corners, but the case hides the
// A/B/X/Y letter moldings — users never learn which corner is which letter.
// So instead of teaching "press the A button", we anchor each action label
// in the screen corner closest to its physical button. The mapping becomes
// self-evident: "the label I want is the one near the button I'm about to
// press". Labels are styled in Clay Peach with a 1-px underline so they
// read as "tappable" — the same visual grammar as a link on anthropic.com.
//
// Typography (2026-04-20 revision):
// The first pass made the corner hints too loud; on real hardware they
// competed with the actual character/content instead of quietly supporting
// it. This revision keeps the corner-position affordance model but scales the
// hint text back down so the chrome reads like a caption, not a title.
#include "chrome.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "geometry.h"
#include "gfx.h"
#include "palette.h"

namespace ui_v2::chrome {

namespace {

constexpr float kRailChipTextScale = 1.2f;   // status rail chip labels
constexpr float kRailHintScale     = 1.0f;   // A/X-hint corner labels
constexpr float kFooterHintScale   = 1.2f;   // B/Y-hint corner labels
constexpr int   kEdgePad           = 4;      // inset from bezel
constexpr int   kChipInnerPadX     = 6;      // left/right padding inside chip
constexpr int   kChipGap           = 6;      // horizontal gap between chips

const char* link_short(LinkState s) {
  switch(s) {
    case LinkState::NotAdvertising: return "OFF";
    case LinkState::Advertising:    return "ADV";
    case LinkState::Connecting:     return "CON";
    case LinkState::PairingPasskey: return "PIN";
    case LinkState::LinkedInsecure: return "LNK?";
    case LinkState::LinkedSecure:   return "LNK";
  }
  return "?";
}

// Draw a rounded chip with a text label at the given scale. Returns width.
int draw_chip_scaled(int x, int y, int h, const char* label, Rgb24 bg,
                     Rgb24 fg, float scale) {
  if(!label) return 0;
  const int tw = gfx::text_width(label, scale);
  const int w  = tw + kChipInnerPadX * 2;
  const int r  = h / 2;
  gfx::rounded_rect(x, y, w, h, r, bg);
  const int glyph_h = static_cast<int>(6 * scale);
  const int ty = y + (h - glyph_h) / 2;
  gfx::text(label, x + kChipInnerPadX, ty, fg, scale);
  return w;
}

// Draw a corner-anchored action label. `flush_right=true` places the label
// against the right bezel; otherwise it hugs the left bezel. Returns the
// width consumed (including the edge pad), which is what callers need to
// know how much rail space is reserved for this corner.
int draw_corner_hint(const char* hint, int y_band_top, int y_band_h,
                     float scale, bool flush_right) {
  if(!hint || !hint[0]) return 0;
  const int tw      = gfx::text_width(hint, scale);
  const int glyph_h = static_cast<int>(6 * scale);
  const int ty      = y_band_top + (y_band_h - glyph_h) / 2;
  const int tx      = flush_right ? (geom::kPanelW - kEdgePad - tw)
                                  : kEdgePad;
  gfx::text(hint, tx, ty, kAccentClay, scale);
  // 1-px clay underline — the "link" tell.
  const int underline_y = std::min(y_band_top + y_band_h - 2, ty + glyph_h + 1);
  gfx::fill_rect(tx, underline_y, tw, 1, kAccentClayDim);
  return tw + kEdgePad;  // text + the outer pad
}

// Measured, ready-to-paint chip descriptor. We compute widths first, then
// center the band in the available zone between the A and X hint claims.
struct ChipSpec {
  const char* label;
  Rgb24       bg;
  Rgb24       fg;
  int         w;  // chip width including inner padding
};

}  // namespace

ChipColors link_colors(LinkState s) {
  switch(s) {
    case LinkState::LinkedSecure:   return {kOkGreen,       kBgInk};
    case LinkState::LinkedInsecure: return {kWarnAmber,     kBgInk};
    case LinkState::PairingPasskey: return {kBusyBlue,      kTextPrimary};
    case LinkState::Connecting:     return {kBusyBlue,      kTextPrimary};
    case LinkState::Advertising:    return {kAccentClayDim, kTextPrimary};
    case LinkState::NotAdvertising: return {kChipPanel,     kTextDim};
  }
  return {kChipPanel, kTextDim};
}

ChipColors power_colors(PowerState s, uint8_t battery_pct) {
  if(s == PowerState::UsbCharging) return {kOkGreen,    kBgInk};
  if(s == PowerState::UsbPowered)  return {kChipPanel,  kTextPrimary};
  if(battery_pct < 15)             return {kDangerRed,  kTextPrimary};
  if(battery_pct < 30)             return {kWarnAmber,  kBgInk};
  return {kChipPanel, kTextPrimary};
}

// When battery is genuinely critical, the chip breathes. A static red can be
// dismissed as ambient color; a breathing red can't — the retina catches
// movement even when the eye isn't on the chip. The amplitude is small on
// purpose: Jobs-era iPod low-battery pulse, not a smoke-alarm strobe. We
// breathe between kDangerRed and kAccentClayDim (a warmer, darker red)
// rather than fading to black — full fade would let the chip disappear
// and hide the warning at trough, which is backwards.
//
// Critical threshold is the OR of two signals:
//   (a) V1 set battery_low — authoritative, comes from mV-calibrated logic
//       that can fire even if battery_pct is lagging.
//   (b) battery_pct <= 10 — a hard floor so we still pulse in the rare
//       case V1's calibration is off and only percent is accurate.
bool is_battery_critical(const BuddyInputs& in) {
  if(in.power != PowerState::Battery) return false;
  return in.battery_low || in.battery_pct <= 10;
}

Rgb24 battery_pulse_bg(uint32_t now_ms) {
  // 1200 ms cycle — roughly 50 bpm, slower than normal heart rate so it
  // reads "laboring" without feeling frantic.
  constexpr float kPeriodMs = 1200.0f;
  const float t = (static_cast<float>(now_ms % static_cast<uint32_t>(kPeriodMs))) *
                  (6.2831853f / kPeriodMs);
  const float phase = 0.5f + 0.5f * std::sin(t);   // 0..1
  const uint8_t mix = static_cast<uint8_t>(phase * 255.0f);
  return lerp(kAccentClayDim, kDangerRed, mix);
}

void draw_status_rail(const BuddyInputs& in, const char* a_hint,
                      const char* x_hint, uint32_t now_ms,
                      bool show_alerts) {
  // Background band.
  gfx::fill_rect(0, 0, geom::kPanelW, geom::kStatusRailH, kBgInk);

  // Corner hints first — they claim their bezel-side zones so the chip
  // band knows how much space it has to work with.
  const int a_w = draw_corner_hint(a_hint, 0, geom::kStatusRailH,
                                   kRailHintScale, /*flush_right=*/false);
  const int x_w = draw_corner_hint(x_hint, 0, geom::kStatusRailH,
                                   kRailHintScale, /*flush_right=*/true);

  // Build chip specs and measure. We intentionally drop the old inline
  // TOTAL counter here — Stats screen surfaces lifetime heartbeats in a
  // readable form, and keeping it in the rail forced a mixed chip/plain
  // text layout that broke center alignment. LNK and BAT are the two
  // permanent at-a-glance indicators; a third slot is reserved for
  // transient alerts (today: pending approval; reachable from home.cpp's
  // "Open" affordance but needs to be SEEN from other screens too, so it
  // rides the rail where any screen using chrome will show it).
  ChipSpec chips[3];
  int n = 0;

  // LNK chip.
  {
    const ChipColors c = link_colors(in.link);
    const char* lbl = link_short(in.link);
    const int tw = gfx::text_width(lbl, kRailChipTextScale);
    chips[n++] = {lbl, c.bg, c.fg, tw + kChipInnerPadX * 2};
  }

  // BAT chip — buffer has to outlive the measure pass, so declare it here.
  char bat_buf[12];
  {
    if(in.power == PowerState::UsbCharging) {
      std::snprintf(bat_buf, sizeof(bat_buf), "%u%% +", in.battery_pct);
    } else if(in.power == PowerState::UsbPowered) {
      std::snprintf(bat_buf, sizeof(bat_buf), "USB");
    } else {
      std::snprintf(bat_buf, sizeof(bat_buf), "%u%%", in.battery_pct);
    }
    ChipColors c = power_colors(in.power, in.battery_pct);
    // Critical override: swap chip bg for the pulsing red so the chip
    // breathes. Text stays kTextPrimary for max contrast regardless of
    // which side of the pulse we're on (both ends are dark enough that
    // cream text reads cleanly — checked mathematically, not by eye).
    if(is_battery_critical(in)) {
      c.bg = battery_pulse_bg(now_ms);
      c.fg = kTextPrimary;
    }
    const int tw = gfx::text_width(bat_buf, kRailChipTextScale);
    chips[n++] = {bat_buf, c.bg, c.fg, tw + kChipInnerPadX * 2};
  }

  // APPR chip — conditional. Only shown when a prompt is pending and has
  // not yet been answered, AND the caller asked for alerts (Approval
  // itself doesn't, since the chip would restate what the whole screen
  // already says). Pulses the same way the battery-critical chip does
  // so the rail has one consistent "urgent" visual grammar. Placed
  // after BAT so it reads left→right as "connection, power, alerts".
  char appr_buf[8];
  if(show_alerts && in.prompt_active && !in.prompt_sent) {
    std::snprintf(appr_buf, sizeof(appr_buf), "APPR");
    Rgb24 bg = battery_pulse_bg(now_ms);
    Rgb24 fg = kTextPrimary;
    const int tw = gfx::text_width(appr_buf, kRailChipTextScale);
    chips[n++] = {appr_buf, bg, fg, tw + kChipInnerPadX * 2};
  }

  // Total band width (chips + gaps).
  int band_w = 0;
  for(int i = 0; i < n; ++i) {
    band_w += chips[i].w;
    if(i < n - 1) band_w += kChipGap;
  }

  // Horizontal zone available between hint claims.
  const int zone_left  = (a_w > 0) ? (kEdgePad + a_w + 8) : kEdgePad;
  const int zone_right = (x_w > 0) ? (geom::kPanelW - (kEdgePad + x_w) - 8)
                                   : (geom::kPanelW - kEdgePad);
  const int zone_w = zone_right - zone_left;

  int chip_x = (band_w <= zone_w)
                 ? (zone_left + (zone_w - band_w) / 2)
                 : zone_left;  // overflow: left-flush, last chip may clip

  // Chip vertical geometry — centered in the rail.
  const int chip_h = geom::kStatusRailH - 4;               // 16 px tall
  const int y      = (geom::kStatusRailH - chip_h) / 2;    // 2 px top

  for(int i = 0; i < n; ++i) {
    if(chip_x + chips[i].w > zone_right) break;
    draw_chip_scaled(chip_x, y, chip_h, chips[i].label,
                     chips[i].bg, chips[i].fg, kRailChipTextScale);
    chip_x += chips[i].w + kChipGap;
  }
}

void draw_footer(const char* b_hint, const char* y_hint) {
  // Slightly deeper ink so the footer reads as its own control band — akin
  // to the home indicator band on Anthropic's product surfaces.
  gfx::fill_rect(0, geom::kFooterY, geom::kPanelW, geom::kFooterH,
                 kBgInkDeep);
  // Hairline separator between content and footer.
  gfx::fill_rect(0, geom::kFooterY, geom::kPanelW, 1, kChipPanel);

  draw_corner_hint(b_hint, geom::kFooterY, geom::kFooterH,
                   kFooterHintScale, /*flush_right=*/false);
  draw_corner_hint(y_hint, geom::kFooterY, geom::kFooterH,
                   kFooterHintScale, /*flush_right=*/true);
}

}  // namespace ui_v2::chrome
