// buddy_mode.cpp — reactive eye-centric Claude Buddy surface.
#include "buddy_mode.h"

#include <cstdio>
#include <cstring>

#include "../chrome.h"
#include "../companion.h"
#include "../face.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"
#include "../risk_tier.h"

namespace ui_v2::screens::buddy_mode {

namespace {

bool     g_show_details = false;
uint32_t g_joy_until_ms = 0;
uint32_t g_proud_until_ms = 0;
uint32_t g_prev_running = 0;
uint32_t g_prev_waiting = 0;
bool     g_prev_prompt = false;
bool     g_prev_transfer = false;
PersonaState g_prev_persona = PersonaState::Sleep;

enum class RateBand : uint8_t {
  Quiet,
  Working,
  Busy,
  Sprint,
};

RateBand rate_band(uint16_t per_min) {
  if(per_min < 30) return RateBand::Quiet;
  if(per_min < 150) return RateBand::Working;
  if(per_min < 600) return RateBand::Busy;
  return RateBand::Sprint;
}

Rgb24 state_color(const BuddyInputs& in) {
  if(in.prompt_active && !in.prompt_sent) return kDangerRed;
  if(in.xfer_active) return kAccentClay;
  if(in.persona == PersonaState::Sleep || in.heartbeat_age_ms > 45000) return kTextDim;
  switch(rate_band(in.token_rate_per_min)) {
    case RateBand::Quiet:   return companion::accent_current();
    case RateBand::Working: return kBusyBlue;
    case RateBand::Busy:    return kWarnAmber;
    case RateBand::Sprint:  return kDangerRed;
  }
  return companion::accent_current();
}

ExpressionId prompt_expression(const BuddyInputs& in) {
  switch(risk::classify(in.prompt_tool)) {
    case RiskTier::Safe:    return ExpressionId::Focused;
    case RiskTier::Neutral: return ExpressionId::GlyphCircle;
    case RiskTier::Caution: return ExpressionId::Skeptical;
    case RiskTier::Danger:  return ExpressionId::GlyphCaretIn;
  }
  return ExpressionId::Alert;
}

ExpressionId active_expression(const BuddyInputs& in, uint32_t now_ms) {
  if(in.prompt_active && !in.prompt_sent) return prompt_expression(in);
  if(in.xfer_active) return ExpressionId::Scan;
  if(now_ms < g_joy_until_ms) return ExpressionId::Joy;
  if(now_ms < g_proud_until_ms) return ExpressionId::Proud;
  if(in.persona == PersonaState::Sleep || in.heartbeat_age_ms > 45000) {
    return ExpressionId::Asleep;
  }
  if(in.waiting > 0 && in.running == 0) return ExpressionId::Attentive;
  if(in.running > 0) {
    switch(rate_band(in.token_rate_per_min)) {
      case RateBand::Quiet:   return ExpressionId::Focused;
      case RateBand::Working: return ExpressionId::Focused;
      case RateBand::Busy:    return ExpressionId::Alert;
      case RateBand::Sprint:  return ExpressionId::GlyphCaretIn;
    }
  }
  return ExpressionId::Neutral;
}

void state_headline(const BuddyInputs& in, uint32_t now_ms, char* out, size_t n) {
  if(n == 0) return;
  if(in.prompt_active && !in.prompt_sent) {
    std::snprintf(out, n, "Approval waiting");
  } else if(in.xfer_active) {
    std::snprintf(out, n, "Receiving character");
  } else if(in.persona == PersonaState::Sleep || in.heartbeat_age_ms > 45000) {
    std::snprintf(out, n, "Sleeping");
  } else if(now_ms < g_joy_until_ms) {
    std::snprintf(out, n, "Task complete");
  } else if(now_ms < g_proud_until_ms) {
    std::snprintf(out, n, "Holding steady");
  } else if(in.running > 0) {
    if(rate_band(in.token_rate_per_min) == RateBand::Sprint) {
      std::snprintf(out, n, "Locked in");
    } else {
      std::snprintf(out, n, "Claude working");
    }
  } else if(in.waiting > 0) {
    std::snprintf(out, n, "Waiting");
  } else {
    std::snprintf(out, n, "Claude buddy");
  }
}

void state_detail(const BuddyInputs& in, char* out, size_t n) {
  if(n == 0) return;
  if(in.prompt_active && !in.prompt_sent) {
    if(in.prompt_tool[0]) std::snprintf(out, n, "%s needs your answer", in.prompt_tool);
    else                  std::snprintf(out, n, "Review the pending request");
  } else if(in.xfer_active) {
    std::snprintf(out, n, "%lu / %lu bytes", static_cast<unsigned long>(in.xfer_written),
                  static_cast<unsigned long>(in.xfer_total));
  } else if(in.running > 0 || in.waiting > 0) {
    std::snprintf(out, n, "%u running  %u waiting  %u tok/min",
                  static_cast<unsigned>(in.running),
                  static_cast<unsigned>(in.waiting),
                  static_cast<unsigned>(in.token_rate_per_min));
  } else if(in.msg[0]) {
    std::snprintf(out, n, "%s", in.msg);
  } else {
    std::snprintf(out, n, "%u tok/min today", static_cast<unsigned>(in.token_rate_per_min));
  }
}

void draw_metric_chip(int x, int y, int w, const char* label, const char* value,
                      Rgb24 value_color) {
  gfx::rounded_rect(x, y, w, 24, 5, kChipPanel);
  gfx::text(label, x + 6, y + 4, kTextDim, 1.0f);
  gfx::text_aligned(value, x, y + 4, w - 6, gfx::Align::Right, value_color, 1.0f);
}

void enter(uint32_t /*now_ms*/) {
  g_show_details = false;
  g_joy_until_ms = 0;
  g_proud_until_ms = 0;
  g_prev_running = 0;
  g_prev_waiting = 0;
  g_prev_prompt = false;
  g_prev_transfer = false;
  g_prev_persona = PersonaState::Sleep;
}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& face,
          CoreIntent& /*intent*/, uint32_t now_ms) {
  const bool prompt_now = in.prompt_active && !in.prompt_sent;
  const bool transfer_now = in.xfer_active;
  const bool was_busy = (g_prev_running > 0 || g_prev_waiting > 0 || g_prev_prompt || g_prev_transfer);
  const bool now_busy = (in.running > 0 || in.waiting > 0 || prompt_now || transfer_now);
  if((was_busy && !now_busy) ||
     (in.persona == PersonaState::Celebrate && g_prev_persona != PersonaState::Celebrate)) {
    g_joy_until_ms = now_ms + 3000u;
    g_proud_until_ms = now_ms + 8000u;
  }
  g_prev_running = in.running;
  g_prev_waiting = in.waiting;
  g_prev_prompt = prompt_now;
  g_prev_transfer = transfer_now;
  g_prev_persona = in.persona;

  const Rgb24 accent = state_color(in);
  const Rgb24 eye_field = lerp(kBgInkDeep, accent, 22);
  const Rgb24 metric_divider = lerp(kChipPanelHi, accent, 40);

  gfx::clear(kBgInk);
  gfx::rounded_rect(20, 40, 74, 84, 10, eye_field);
  gfx::rounded_rect(226, 40, 74, 84, 10, eye_field);
  gfx::fill_rect(20, 194, 280, 1, metric_divider);

  face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kBuddyEyeCy);
  face::set_ink(face, kTextPrimary);
  face::set_ambient(face, !prompt_now && !transfer_now && in.motion_profile == MotionProfile::Full,
                    !prompt_now && !transfer_now && in.motion_profile == MotionProfile::Full);
  const ExpressionId want = active_expression(in, now_ms);
  if(face.target != want) {
    face::begin_transition(face, want, now_ms, 160);
  }
  face::tick_and_render(face, now_ms);

  char headline[48];
  char detail[96];
  state_headline(in, now_ms, headline, sizeof(headline));
  state_detail(in, detail, sizeof(detail));
  gfx::text_aligned(headline, 0, 148, geom::kPanelW, gfx::Align::Center, accent, 1.6f);
  char detail_fit[96];
  gfx::fit_text(detail_fit, sizeof(detail_fit), detail, 276, 1.0f);
  gfx::text_aligned(detail_fit, 0, 167, geom::kPanelW, gfx::Align::Center, kTextWarm, 1.0f);

  if(g_show_details) {
    char rate[16];
    char today[16];
    char run[16];
    char wait[16];
    gfx::format_compact(today, sizeof(today), in.tokens_today);
    std::snprintf(rate, sizeof(rate), "%u", static_cast<unsigned>(in.token_rate_per_min));
    std::snprintf(run, sizeof(run), "%u", static_cast<unsigned>(in.running));
    std::snprintf(wait, sizeof(wait), "%u", static_cast<unsigned>(in.waiting));
    draw_metric_chip(16, 198, 70, "rate", rate, accent);
    draw_metric_chip(92, 198, 70, "run", run, kBusyBlue);
    draw_metric_chip(168, 198, 70, "wait", wait, kWarnAmber);
    draw_metric_chip(244, 198, 60, "today", today, kTextPrimary);
  } else {
    char today[16], approve[16], deny[16];
    gfx::format_compact(today, sizeof(today), in.tokens_today);
    gfx::format_thousands(approve, sizeof(approve), in.approvals);
    gfx::format_thousands(deny, sizeof(deny), in.denials);
    draw_metric_chip(28, 198, 82, "today", today, accent);
    draw_metric_chip(118, 198, 82, "approve", approve, kOkGreen);
    draw_metric_chip(208, 198, 82, "deny", deny, kDangerRed);
  }

  chrome::draw_status_rail(in, "Menu", "Pet", now_ms);
  chrome::draw_footer(g_show_details ? "Hide" : "Details", "Home");
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& /*out*/,
               CoreIntent& intent, uint32_t /*now_ms*/) {
  if(edge != ButtonEdge::ShortTap) return;
  if(btn == Button::A) {
    intent.push = true;
    intent.target = ScreenId::SystemMenu;
  } else if(btn == Button::B) {
    g_show_details = !g_show_details;
  } else if(btn == Button::X) {
    intent.push = true;
    intent.target = ScreenId::Pet;
  } else if(btn == Button::Y) {
    intent.replace = true;
    intent.target = ScreenId::Home;
  }
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::buddy_mode
