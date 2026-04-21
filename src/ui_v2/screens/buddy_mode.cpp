// buddy_mode.cpp — reactive eye-centric Claude Buddy surface.
#include "buddy_mode.h"

#include <cstdio>
#include <cstring>

#include "../acting.h"
#include "../chrome.h"
#include "../companion.h"
#include "../face.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"

namespace ui_v2::screens::buddy_mode {

namespace {

bool     g_show_details = false;
uint32_t g_joy_until_ms = 0;
uint32_t g_proud_until_ms = 0;
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

Rgb24 state_color(const BuddyInputs& in, uint32_t now_ms) {
  if(acting::approval_pending(in)) return acting::prompt_accent(in);
  if(acting::prompt_result_active(in, now_ms)) {
    switch(in.prompt_result) {
      case PromptResult::Approved:
        return (in.prompt_result_band == PromptResultBand::Quick)
            ? kAccentClay : kOkGreen;
      case PromptResult::Denied:
        return kDangerRed;
      case PromptResult::TimedOut:
        return kTextDim;
      case PromptResult::None:
        break;
    }
  }
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

ExpressionId active_expression(const BuddyInputs& in, uint32_t now_ms) {
  if(acting::prompt_result_active(in, now_ms)) {
    return acting::prompt_result_expression(in, now_ms);
  }
  if(acting::approval_pending(in)) return acting::approval_expression(in, now_ms);
  if(in.xfer_active) return ExpressionId::Scan;
  if(now_ms < g_joy_until_ms) return ExpressionId::Joy;
  if(now_ms < g_proud_until_ms) return ExpressionId::Proud;
  if(in.persona == PersonaState::Sleep || in.heartbeat_age_ms > 45000) {
    return ExpressionId::Asleep;
  }
  if(in.waiting > 0 && in.running == 0) return ExpressionId::Attentive;
  if(in.running > 0) {
    return ((now_ms / 2200u) % 4u == 1u) ? ExpressionId::Scan
                                         : ExpressionId::Focused;
  }
  return ExpressionId::Neutral;
}

void state_headline(const BuddyInputs& in, uint32_t now_ms, char* out, size_t n) {
  if(n == 0) return;
  if(acting::prompt_result_active(in, now_ms)) {
    switch(in.prompt_result) {
      case PromptResult::Approved:
        if(in.prompt_result_band == PromptResultBand::Quick) {
          std::snprintf(out, n, "Quick approval");
        } else {
          std::snprintf(out, n, "Approval sent");
        }
        break;
      case PromptResult::Denied:
        std::snprintf(out, n, "Approval denied");
        break;
      case PromptResult::TimedOut:
        std::snprintf(out, n, "Request expired");
        break;
      case PromptResult::None:
        std::snprintf(out, n, "Claude buddy");
        break;
    }
  } else if(acting::approval_pending(in)) {
    switch(acting::approval_wait_band(in, now_ms)) {
      case acting::ApprovalWaitBand::PromptNew:
        std::snprintf(out, n, "New approval");
        break;
      case acting::ApprovalWaitBand::PromptWaiting:
        std::snprintf(out, n, "Approval waiting");
        break;
      case acting::ApprovalWaitBand::PromptOverdue:
        std::snprintf(out, n, "Needs answer");
        break;
      case acting::ApprovalWaitBand::PromptEscalated:
        std::snprintf(out, n, "Answer now");
        break;
      case acting::ApprovalWaitBand::None:
        std::snprintf(out, n, "Approval waiting");
        break;
    }
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
  if(acting::prompt_result_active(in, in.now_ms)) {
    switch(in.prompt_result) {
      case PromptResult::Approved:
        if(in.prompt_result_band == PromptResultBand::Quick) {
          std::snprintf(out, n, "Warm settle after a fast yes");
        } else if(in.prompt_result_band == PromptResultBand::Normal) {
          std::snprintf(out, n, "Decision sent with a calm settle");
        } else {
          std::snprintf(out, n, "Approved after a longer wait");
        }
        break;
      case PromptResult::Denied:
        std::snprintf(out, n, "Claude stays blocked on %s",
                      in.prompt_tool[0] ? in.prompt_tool : "that request");
        break;
      case PromptResult::TimedOut:
        std::snprintf(out, n, "The approval vanished before a reply");
        break;
      case PromptResult::None:
        std::snprintf(out, n, "%u tok/min today", static_cast<unsigned>(in.token_rate_per_min));
        break;
    }
  } else if(acting::approval_pending(in)) {
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
  g_prev_persona = PersonaState::Sleep;
}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& face,
          CoreIntent& /*intent*/, uint32_t now_ms) {
  const bool prompt_now = acting::approval_pending(in);
  if(in.persona == PersonaState::Celebrate && g_prev_persona != PersonaState::Celebrate) {
    g_joy_until_ms = now_ms + 3000u;
    g_proud_until_ms = now_ms + 8000u;
  }
  g_prev_persona = in.persona;

  const Rgb24 accent = state_color(in, now_ms);
  const Rgb24 eye_field = lerp(kBgInkDeep, accent, 22);
  const Rgb24 metric_divider = lerp(kChipPanelHi, accent, 40);

  gfx::clear(kBgInk);
  gfx::rounded_rect(20, 40, 74, 84, 10, eye_field);
  gfx::rounded_rect(226, 40, 74, 84, 10, eye_field);
  gfx::fill_rect(20, 194, 280, 1, metric_divider);

  face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kBuddyEyeCy);
  face::set_ink(face, kTextPrimary);
  face::set_ambient(face, !prompt_now && !in.xfer_active && in.motion_profile == MotionProfile::Full,
                    !prompt_now && !in.xfer_active && in.motion_profile == MotionProfile::Full);
  const ExpressionId want = active_expression(in, now_ms);
  if(face.target != want) {
    face::begin_transition(face, want, now_ms, 160);
  }
  face::tick_and_render(face, now_ms);

  char headline[48];
  char detail[96];
  state_headline(in, now_ms, headline, sizeof(headline));
  BuddyInputs detail_in = in;
  detail_in.now_ms = now_ms;
  state_detail(detail_in, detail, sizeof(detail));
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
