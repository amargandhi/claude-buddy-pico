// transfer.cpp — character-pack transfer progress overlay.
#include "transfer.h"

#include <cstdio>

#include "../chrome.h"
#include "../face.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"

namespace ui_v2::screens::transfer {

namespace {

// Linger on the end-state (done or disconnected) before popping so the
// user sees the result of the action they triggered. Matches the 800 ms
// sync→home handoff cadence for visual consistency.
constexpr uint32_t kLingerMs = 800;

uint32_t g_enter_ms        = 0;
uint32_t g_ended_ms        = 0;  // 0 while xfer_active; set on falling edge
bool     g_ended_linked    = false;
uint32_t g_final_written   = 0;
uint32_t g_final_total     = 0;

void enter(uint32_t now_ms) {
  g_enter_ms      = now_ms;
  g_ended_ms      = 0;
  g_ended_linked  = false;
  g_final_written = 0;
  g_final_total   = 0;
}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& face,
          CoreIntent& intent, uint32_t now_ms) {
  // Detect end-of-transfer. We latch `ended` once so the screen's final
  // snapshot doesn't change if V1 keeps tweaking xfer fields during the
  // linger. The "Saved." vs "Disconnected." decision is based on link
  // state at the moment the transfer stopped — if the Mac vanished
  // mid-flight, that's a disconnect; otherwise we assume success.
  if(g_ended_ms == 0 && !in.xfer_active) {
    g_ended_ms      = now_ms;
    g_ended_linked  = (in.link == LinkState::LinkedSecure ||
                       in.link == LinkState::LinkedInsecure);
    g_final_written = in.xfer_written;
    g_final_total   = in.xfer_total;
  }

  // Face: small, top-right, attentive during transfer, switches to Happy
  // on success or Sleepy on disconnect. Mirrors approval's face grammar
  // so the visual vocabulary of "the Mac is doing something to me right
  // now" is consistent.
  face::set_base(face, 180, 210, 36);
  face::set_ambient(face, false, false);

  ExpressionId want;
  if(g_ended_ms == 0) {
    want = ExpressionId::Attentive;
  } else if(g_ended_linked) {
    want = ExpressionId::Happy;
  } else {
    want = ExpressionId::Sleepy;
  }
  if(face.target != want) {
    face::begin_transition(face, want, now_ms, 150);
  }

  gfx::clear(kBgInk);
  face::tick_and_render(face, now_ms);

  // Title.
  const char* title;
  Rgb24       title_color;
  if(g_ended_ms == 0) {
    title       = "Receiving character";
    title_color = kTextPrimary;
  } else if(g_ended_linked) {
    title       = "Saved.";
    title_color = kOkGreen;
  } else {
    title       = "Disconnected.";
    title_color = kDangerRed;
  }
  gfx::text_aligned(title, 0, 72, geom::kPanelW,
                    gfx::Align::Center, title_color, 2.2f);

  // Subtitle — bytes received. We render from the snapshot during the
  // linger so the numbers don't flicker back to zero if V1 clears
  // xfer_written after completion.
  const uint32_t written = g_ended_ms ? g_final_written : in.xfer_written;
  const uint32_t total   = g_ended_ms ? g_final_total   : in.xfer_total;

  char sub[48];
  if(total > 0) {
    std::snprintf(sub, sizeof(sub), "%lu / %lu bytes",
                  static_cast<unsigned long>(written),
                  static_cast<unsigned long>(total));
  } else {
    std::snprintf(sub, sizeof(sub), "%lu bytes",
                  static_cast<unsigned long>(written));
  }
  gfx::text_aligned(sub, 0, 108, geom::kPanelW,
                    gfx::Align::Center, kTextDim, 1.3f);

  // Progress bar. Tall enough to read at desk distance. If total is
  // unknown (0), we animate an indeterminate sweep rather than show a
  // zero-width bar — which would lie about stalled-ness.
  const int bar_x = geom::kPadOuter;
  const int bar_y = 140;
  const int bar_w = geom::kPanelW - geom::kPadOuter * 2;
  const int bar_h = 12;

  if(total > 0) {
    gfx::progress_bar(bar_x, bar_y, bar_w, bar_h,
                      written, total, kAccentClay, kChipPanel);
  } else {
    // Indeterminate: fill the track and draw a moving 40 px highlight.
    gfx::fill_rect(bar_x, bar_y, bar_w, bar_h, kChipPanel);
    const int hi_w = 40;
    const int range = bar_w - hi_w;
    const uint32_t period = 1600;
    const uint32_t t = (now_ms - g_enter_ms) % period;
    // Triangle wave 0..range..0 so the highlight bounces rather than
    // wrapping — a wrap reads as "loading forever" on mobile and we've
    // been trained to distrust it.
    uint32_t phase = (t * range) / (period / 2);
    int hi_x;
    if(phase <= static_cast<uint32_t>(range)) {
      hi_x = bar_x + static_cast<int>(phase);
    } else {
      hi_x = bar_x + range - static_cast<int>(phase - range);
    }
    gfx::fill_rect(hi_x, bar_y, hi_w, bar_h, kAccentClay);
  }

  // Chrome — LNK and BAT still visible. No button hints, no footer
  // affordances; the user is a passenger here. See rationale in .h.
  chrome::draw_status_rail(in, nullptr, nullptr, now_ms);
  chrome::draw_footer(nullptr, nullptr);

  // Linger complete → pop back to whatever was under us.
  if(g_ended_ms != 0 && (now_ms - g_ended_ms) >= kLingerMs) {
    intent.pop = true;
  }
}

void on_button(Button /*btn*/, ButtonEdge /*edge*/, BuddyOutputs& /*out*/,
               CoreIntent& /*intent*/, uint32_t /*now_ms*/) {
  // Non-cancellable from the Pico side. V1 drives the transfer; buttons
  // here are no-ops on purpose. If we later grow a "request_cancel_xfer"
  // output we can bind B here and label the footer — but that has to be
  // a real output, not a label without a handler.
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::transfer
