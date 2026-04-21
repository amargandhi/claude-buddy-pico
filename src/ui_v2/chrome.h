// chrome.h — shared status rail and footer widgets.
//
// The four physical buttons sit at the corners of the case, but the case
// hides the A/B/X/Y letter moldings — the user never sees which corner is
// which letter. Putting an "A" badge on the screen would teach nothing. So
// the affordance here is purely *positional*: each button's action label
// lives in the screen corner closest to the physical button.
//
//   +----------------------------------+
//   | A-hint  [LNK] [BAT] [APPR*]  X-h |   <- status rail (top 20 px)
//   |                                  |
//   |          (screen content)        |
//   |                                  |
//   | B-hint                    Y-hint |   <- footer (bottom 28 px)
//   +----------------------------------+
//
//   *APPR chip only appears when a prompt is pending. Pulses red to catch
//    the eye; suppress on the Approval screen itself via show_alerts=false.
//
// Chips are centered in the rail between whatever horizontal space the
// A and X hints have claimed. Labels are rendered in Clay Peach with a
// 1-px underline — the same visual grammar as a link on anthropic.com —
// so they read instantly as "tappable things" rather than decorative
// text. Rail hints use scale 1.5 (9 px body) and footer hints use scale
// 2.0 (12 px body) — sized for ~40–50 cm viewing distance on this 2.8"
// ~143 PPI panel.
//
// Any hint may be nullptr to leave its corner blank.
#pragma once

#include <cstdint>

#include "palette.h"
#include "ui_state.h"

namespace ui_v2::chrome {

// Draw the top status rail. Renders the A-hint label flush-left (top-left
// corner, aligned under physical button A), LNK and BAT chips centered in
// the remaining space, and the X-hint label flush-right (top-right
// corner, under X). The inline lifetime-TOTAL counter that used to sit
// after the chips was removed — it competed with the chip motif and
// broke centering; lifetime heartbeats live on the Stats screen instead.
//
// show_alerts (default true) controls the transient alert chips (today:
// APPR when a prompt is pending). Screens that ARE the alert — like
// Approval itself — should pass false to avoid the "you have a pending
// approval" chip painted on top of the pending-approval screen.
void draw_status_rail(const BuddyInputs& in, const char* a_hint,
                      const char* x_hint, uint32_t now_ms,
                      bool show_alerts = true);

// Draw the bottom footer row. B-hint label flush-left (under B), Y-hint
// label flush-right (under Y). Pass nullptr to hide either label.
void draw_footer(const char* b_hint, const char* y_hint);

// Helper: compute chip color for the link state.
struct ChipColors {
  Rgb24 bg;
  Rgb24 fg;
};
ChipColors link_colors(LinkState s);
ChipColors power_colors(PowerState s, uint8_t battery_pct);

}  // namespace ui_v2::chrome
