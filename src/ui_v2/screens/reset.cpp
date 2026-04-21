// reset.cpp — two-stage destructive confirmation.
#include "reset.h"

#include <cstdio>

#include "../chrome.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"

namespace ui_v2::screens::reset {

namespace {

bool     g_armed   = false;
uint32_t g_arm_ms  = 0;
Mode     g_mode    = Mode::FactoryReset;
constexpr uint32_t kArmWindowMs = 3000;

void enter(uint32_t /*now_ms*/) { g_armed = false; g_arm_ms = 0; }

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& /*face*/,
          CoreIntent& intent, uint32_t now_ms) {
  gfx::clear(kBgInk);

  // Disarm if arm window elapsed.
  if(g_armed && (now_ms - g_arm_ms) >= kArmWindowMs) {
    g_armed = false;
  }

  const Rgb24 warn = g_armed ? kDangerRed : kWarnAmber;
  gfx::fill_rect(0, 20, geom::kPanelW, 4, warn);

  const char* title = (g_mode == Mode::ClearStats) ? "Clear Stats"
                                                   : "Factory Reset";
  gfx::text_aligned(title, 0, 46, geom::kPanelW, gfx::Align::Center,
                    warn, 3.0f);

  const char* sub = nullptr;
  if(g_armed) {
    sub = (g_mode == Mode::ClearStats)
        ? "Hold A again to wipe approvals, nap, tokens, and level"
        : "Hold A again to wipe stats, settings, pack, and unpair";
  } else {
    sub = "Hold A to arm. Any other button cancels.";
  }
  gfx::text_aligned(sub, 0, 96, geom::kPanelW, gfx::Align::Center,
                    kTextWarm, 1.0f);

  // Countdown bar while armed.
  if(g_armed) {
    const uint32_t left = (now_ms - g_arm_ms);
    gfx::progress_bar(geom::kPadOuter + 20, 140,
                      geom::kPanelW - (geom::kPadOuter + 20) * 2, 8,
                      kArmWindowMs - left, kArmWindowMs,
                      kDangerRed, kChipPanel);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%u ms left", kArmWindowMs - left);
    gfx::text_aligned(buf, 0, 156, geom::kPanelW, gfx::Align::Center,
                      kTextDim, 1.0f);
  }

  // The ARMED/RESET state is already conveyed by the colored strip at y=20
  // and the sub-copy line, so the rail shows only the action affordances.
  // Any short-tap on B/X/Y cancels; A needs a long-hold to act.
  chrome::draw_status_rail(in, g_armed ? "Hold: Wipe" : "Hold: Arm",
                           "Cancel", now_ms);
  chrome::draw_footer("Cancel", "Cancel");
  (void)intent;
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& out,
               CoreIntent& intent, uint32_t now_ms) {
  if(btn == Button::A && edge == ButtonEdge::HoldFired) {
    if(g_armed) {
      // Second hold — fire.
      if(g_mode == Mode::ClearStats) {
        out.request_clear_stats = true;
      } else {
        out.request_factory_reset = true;
      }
      g_armed = false;
      intent.replace = true;
      intent.target  = ScreenId::Home;
      return;
    }
    g_armed  = true;
    g_arm_ms = now_ms;
    return;
  }
  if(edge == ButtonEdge::ShortTap) {
    g_armed = false;
    intent.pop = true;
  }
}

void exit(uint32_t /*now_ms*/) { g_armed = false; }

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

void prepare(Mode mode) { g_mode = mode; }

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::reset
