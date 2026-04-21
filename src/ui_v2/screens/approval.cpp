// approval.cpp — approval overlay implementation.
#include "approval.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../acting.h"
#include "../chrome.h"
#include "../face.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"
#include "../permissions_log.h"
#include "../risk_tier.h"

namespace ui_v2::screens::approval {

namespace {

enum class Phase : uint8_t {
  Arrival,
  Live,
  ApproveBeat,
  DenyBeat,
  DismissBeat,
  TimeoutBeat,
};

Phase    g_phase = Phase::Arrival;
uint32_t g_phase_started_ms = 0;
bool     g_emit_decision = false;
bool     g_phase_beat_started = false;

char     g_pending_tool[48] = "";
uint32_t g_pending_arrived_ms = 0;
bool     g_decision_recorded = false;
acting::ApprovalWaitBand g_last_wait_band = acting::ApprovalWaitBand::None;
uint32_t g_last_wait_pulse_ms = 0;
PromptResultBand g_result_band = PromptResultBand::None;

Rgb24 tier_color(RiskTier t) {
  switch(t) {
    case RiskTier::Safe:    return kOkGreen;
    case RiskTier::Neutral: return kBusyBlue;
    case RiskTier::Caution: return kWarnAmber;
    case RiskTier::Danger:  return kDangerRed;
  }
  return kDangerRed;
}

ExpressionId tier_expression(RiskTier t) {
  switch(t) {
    case RiskTier::Safe:    return ExpressionId::Focused;
    case RiskTier::Neutral: return ExpressionId::Attentive;
    case RiskTier::Caution: return ExpressionId::Skeptical;
    case RiskTier::Danger:  return ExpressionId::Alert;
  }
  return ExpressionId::Alert;
}

ExpressionId arrival_expression(const BuddyInputs& in, RiskTier t) {
  if(in.face_style == FaceStyle::Classic) return tier_expression(t);
  if(t == RiskTier::Danger) return ExpressionId::GlyphWideCircle;
  if(t == RiskTier::Neutral) return ExpressionId::GlyphCircle;
  if(t == RiskTier::Safe) return ExpressionId::Focused;
  return ExpressionId::Alert;
}

ExpressionId result_expression(const BuddyInputs& in, bool approve, uint32_t phase_age) {
  if(in.face_style == FaceStyle::Classic) {
    if(approve) return ExpressionId::Happy;
    return phase_age < 120u ? ExpressionId::Squint : ExpressionId::GlyphCaretIn;
  }
  if(!approve) return phase_age < 120u ? ExpressionId::Squint
                                       : ExpressionId::GlyphCaretIn;
  switch(g_result_band) {
    case PromptResultBand::Quick:  return phase_age < 900u ? ExpressionId::Heart
                                                           : ExpressionId::Proud;
    case PromptResultBand::Normal: return phase_age < 900u ? ExpressionId::Relief
                                                           : ExpressionId::Proud;
    case PromptResultBand::Slow:   return phase_age < 900u ? ExpressionId::Happy
                                                           : ExpressionId::Proud;
    case PromptResultBand::None:   return ExpressionId::Proud;
  }
  return ExpressionId::Proud;
}

void start_phase(Phase phase, uint32_t now_ms) {
  g_phase = phase;
  g_phase_started_ms = now_ms;
  g_phase_beat_started = false;
}

uint8_t wait_panel_strength(acting::ApprovalWaitBand band) {
  switch(band) {
    case acting::ApprovalWaitBand::PromptNew:       return 28;
    case acting::ApprovalWaitBand::PromptWaiting:   return 42;
    case acting::ApprovalWaitBand::PromptOverdue:   return 58;
    case acting::ApprovalWaitBand::PromptEscalated: return 76;
    case acting::ApprovalWaitBand::None:            return 24;
  }
  return 24;
}

const char* wait_tag(acting::ApprovalWaitBand band) {
  switch(band) {
    case acting::ApprovalWaitBand::PromptNew:       return "NEW";
    case acting::ApprovalWaitBand::PromptWaiting:   return "WAIT";
    case acting::ApprovalWaitBand::PromptOverdue:   return "OVER";
    case acting::ApprovalWaitBand::PromptEscalated: return "NOW";
    case acting::ApprovalWaitBand::None:            return "LIVE";
  }
  return "LIVE";
}

void draw_focus_shell(Rgb24 accent, uint8_t strength) {
  gfx::clear(kBgInk);
  gfx::fill_rect(0, 0, geom::kPanelW, 3, accent);
  gfx::rounded_rect(24, 18, 272, 96, 14, lerp(kBgInkDeep, accent, strength));
  gfx::rounded_rect(12, 140, 296, 62, 8, lerp(kChipPanel, accent, static_cast<uint8_t>(strength / 2)));
  const Rgb24 shell_line = lerp(kChipPanelHi, accent, 84);
  gfx::fill_rect(28, 118, 78, 1, shell_line);
  gfx::fill_rect(214, 118, 78, 1, shell_line);
  gfx::fill_rect(12, 202, 296, 1, kChipPanelHi);
}

void enter(uint32_t now_ms) {
  start_phase(Phase::Arrival, now_ms);
  g_emit_decision = false;
  g_pending_tool[0] = '\0';
  g_pending_arrived_ms = now_ms;
  g_decision_recorded = false;
  g_last_wait_band = acting::ApprovalWaitBand::None;
  g_last_wait_pulse_ms = 0;
  g_result_band = PromptResultBand::None;
}

int draw_hint_wrapped(const char* hint, int x, int y, int w, int max_lines) {
  if(!hint || !hint[0] || max_lines <= 0) return 0;
  constexpr float kHintScale = 1.0f;
  char wrapped[3][96] = {};
  int line = 0;
  int cursor = 0;
  const int max_w = w - 16;

  const char* p = hint;
  while(*p && line < max_lines) {
    char word[40] = {};
    int wlen = 0;
    while(*p == ' ') ++p;
    while(*p && *p != ' ' && *p != '\n' && wlen + 1 < static_cast<int>(sizeof(word))) {
      word[wlen++] = *p++;
    }
    word[wlen] = '\0';

    if(wlen == 0 && *p == '\n') {
      ++p;
      ++line;
      cursor = 0;
      continue;
    }
    if(wlen == 0) break;

    char candidate[96];
    if(cursor == 0) std::snprintf(candidate, sizeof(candidate), "%s", word);
    else            std::snprintf(candidate, sizeof(candidate), "%s %s", wrapped[line], word);

    if(cursor != 0 && gfx::text_width(candidate, kHintScale) > max_w) {
      ++line;
      if(line >= max_lines) break;
      std::snprintf(wrapped[line], sizeof(wrapped[line]), "%s", word);
      cursor = wlen;
    } else {
      std::snprintf(wrapped[line], sizeof(wrapped[line]), "%s", candidate);
      cursor = static_cast<int>(std::strlen(wrapped[line]));
    }

    if(*p == '\n') {
      ++p;
      ++line;
      cursor = 0;
    }
  }

  int drawn = 0;
  for(int i = 0; i < max_lines; ++i) {
    if(!wrapped[i][0]) continue;
    gfx::text(wrapped[i], x + 8, y + 10 + i * 13, kTextWarm, 1.0f);
    ++drawn;
  }
  return drawn;
}

void tick(const BuddyInputs& in, BuddyOutputs& out, FaceState& face,
          CoreIntent& intent, uint32_t now_ms) {
  if(in.prompt_active && !in.prompt_sent) {
    std::snprintf(g_pending_tool, sizeof(g_pending_tool), "%s", in.prompt_tool);
    g_pending_arrived_ms = in.prompt_arrived_ms;
  }

  const RiskTier tier = risk::classify(g_pending_tool);
  const Rgb24 accent = tier_color(tier);
  const uint32_t phase_age = now_ms - g_phase_started_ms;

  if(g_phase == Phase::DismissBeat) {
    draw_focus_shell(kTextWarm, 40);
    face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kApprovalEyeCy);
    face::set_ambient(face, false, false);
    face::set_ink(face, kTextPrimary);
    if(!g_phase_beat_started) {
      face::play_beat(face, ActingBeat::Dismiss, now_ms);
      g_phase_beat_started = true;
    }
    if(face.target != ExpressionId::Sad) {
      face::begin_transition(face, ExpressionId::Sad, now_ms, 140);
    }
    face::tick_and_render(face, now_ms);
    gfx::text_aligned("LATER", 0, 148, geom::kPanelW,
                      gfx::Align::Center, kTextWarm, 1.5f);
    if(phase_age >= 220u) intent.pop = true;
    return;
  }

  if(!in.prompt_active || in.prompt_sent) {
    if(!g_decision_recorded && !in.prompt_sent && g_phase != Phase::TimeoutBeat) {
      const uint32_t wait_ms = (now_ms >= g_pending_arrived_ms)
          ? (now_ms - g_pending_arrived_ms) : 0;
      perms::record(g_pending_tool, perms::Decision::Timeout, now_ms, wait_ms);
      g_decision_recorded = true;
      start_phase(Phase::TimeoutBeat, now_ms);
    }
    if(g_phase == Phase::TimeoutBeat) {
      draw_focus_shell(kTextDim, 34);
      face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kApprovalEyeCy);
      face::set_ambient(face, false, false);
      face::set_ink(face, kTextPrimary);
      if(!g_phase_beat_started) {
        face::play_beat(face, ActingBeat::Timeout, now_ms);
        g_phase_beat_started = true;
      }
      const ExpressionId want = (in.persona == PersonaState::Sleep || in.heartbeat_age_ms > 45000u)
          ? ExpressionId::Sleepy : ExpressionId::Sad;
      if(face.target != want) {
        face::begin_transition(face, want, now_ms, 160);
      }
      face::tick_and_render(face, now_ms);
      gfx::text_aligned("REQUEST EXPIRED", 0, 148, geom::kPanelW,
                        gfx::Align::Center, kTextDim, 1.3f);
      if(phase_age >= 220u) intent.pop = true;
      return;
    }
    intent.pop = true;
    return;
  }

  if(g_phase == Phase::Arrival && phase_age >= 320u) {
    start_phase(Phase::Live, now_ms);
  }

  if(g_phase == Phase::Arrival) {
    draw_focus_shell(accent, 58);
    face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kApprovalEyeCy);
    face::set_ambient(face, false, false);
    face::set_ink(face, kTextPrimary);
    if(!g_phase_beat_started) {
      face::play_beat(face, ActingBeat::Arrival, now_ms);
      g_phase_beat_started = true;
    }
    const ExpressionId want = arrival_expression(in, tier);
    if(face.target != want) {
      face::begin_transition(face, want, now_ms, 180);
    }
    face::tick_and_render(face, now_ms);
    gfx::text_aligned("CLAUDE NEEDS PERMISSION", 0, 148, geom::kPanelW,
                      gfx::Align::Center, accent, 1.2f);
    gfx::text_aligned("Review before deciding", 0, 168, geom::kPanelW,
                      gfx::Align::Center, kTextWarm, 1.0f);
    return;
  }

  if(g_phase == Phase::ApproveBeat || g_phase == Phase::DenyBeat) {
    const bool approve = (g_phase == Phase::ApproveBeat);
    const Rgb24 beat_accent = approve
        ? (g_result_band == PromptResultBand::Quick ? kAccentClay : kOkGreen)
        : kDangerRed;
    draw_focus_shell(beat_accent, approve ? 62 : 52);
    face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kApprovalEyeCy);
    face::set_ambient(face, false, false);
    face::set_ink(face, kTextPrimary);
    if(!g_phase_beat_started) {
      face::play_beat(face,
                      approve
                          ? (g_result_band == PromptResultBand::Quick ? ActingBeat::ApproveQuick
                             : g_result_band == PromptResultBand::Normal ? ActingBeat::ApproveNormal
                                                                          : ActingBeat::ApproveSlow)
                          : ActingBeat::Deny,
                      now_ms);
      g_phase_beat_started = true;
    }
    const ExpressionId want = result_expression(in, approve, phase_age);
    if(face.target != want) {
      face::begin_transition(face, want, now_ms, approve ? 180 : 200);
    }
    face::tick_and_render(face, now_ms);
    gfx::text_aligned(approve ? "APPROVED" : "DENIED", 0, 150, geom::kPanelW,
                      gfx::Align::Center, beat_accent, 1.8f);
    if(approve) {
      const char* subtitle = (g_result_band == PromptResultBand::Quick)
          ? "Quick answer, warm settle"
          : (g_result_band == PromptResultBand::Normal)
                ? "Decision sent cleanly"
                : "Decision sent after a wait";
      gfx::text_aligned(subtitle, 0, 170, geom::kPanelW,
                        gfx::Align::Center, kTextWarm, 1.0f);
    } else {
      gfx::text_aligned("Claude will stay blocked", 0, 170, geom::kPanelW,
                        gfx::Align::Center, kTextWarm, 1.0f);
    }
    if(!g_emit_decision && phase_age >= 200) {
      if(approve) out.send_approve_once = true;
      else        out.send_deny = true;
      g_emit_decision = true;
    }
    return;
  }

  const acting::ApprovalWaitBand wait_band = acting::approval_wait_band(in, now_ms);
  draw_focus_shell(accent, wait_panel_strength(wait_band));

  face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kApprovalEyeCy);
  face::set_ambient(face, false, false);
  face::set_ink(face, kTextPrimary);
  const ExpressionId want = acting::approval_expression(in, now_ms);
  if(face.target != want) {
    face::begin_transition(face, want, now_ms, 180);
  }
  if(wait_band != g_last_wait_band) {
    g_last_wait_band = wait_band;
    g_last_wait_pulse_ms = now_ms;
    if(wait_band == acting::ApprovalWaitBand::PromptNew) {
      face::play_beat(face, ActingBeat::Arrival, now_ms, 180);
    } else if(wait_band >= acting::ApprovalWaitBand::PromptOverdue) {
      face::play_beat(face, ActingBeat::WaitTighten, now_ms);
    }
  } else if(wait_band >= acting::ApprovalWaitBand::PromptOverdue) {
    const uint32_t pulse_period = (wait_band == acting::ApprovalWaitBand::PromptEscalated) ? 1100u : 1600u;
    if(now_ms - g_last_wait_pulse_ms >= pulse_period) {
      face::play_beat(face, ActingBeat::WaitTighten, now_ms);
      g_last_wait_pulse_ms = now_ms;
    }
  }
  face::tick_and_render(face, now_ms);

  char label[48];
  risk::label(in.prompt_tool, label, sizeof(label));
  gfx::text_aligned(label, 0, 104, geom::kPanelW, gfx::Align::Center, accent, 2.2f);

  char waited[16];
  gfx::format_mmss(waited, sizeof(waited),
                   (now_ms >= in.prompt_arrived_ms) ? ((now_ms - in.prompt_arrived_ms) / 1000u) : 0u);
  char tool_meta[64];
  std::snprintf(tool_meta, sizeof(tool_meta), "%s  %s",
                risk::tier_name(tier), g_pending_tool[0] ? g_pending_tool : "unknown");
  gfx::text(tool_meta, 12, 126, kTextDim, 1.0f);
  char waited_label[24];
  std::snprintf(waited_label, sizeof(waited_label), "%s %s", wait_tag(wait_band), waited);
  gfx::text_aligned(waited_label, 0, 126, 308, gfx::Align::Right, accent, 1.0f);

  gfx::text("HINT", 20, 148, kTextDim, 1.0f);
  draw_hint_wrapped(in.prompt_hint, 12, 156, 296, 3);

  const uint32_t waited_ms = (now_ms >= in.prompt_arrived_ms)
      ? (now_ms - in.prompt_arrived_ms) : 0;
  gfx::progress_bar(12, 204, 296, 6, waited_ms, 30000, accent, kChipPanel);

  chrome::draw_status_rail(in, "Approve", nullptr, now_ms, /*show_alerts=*/false);
  chrome::draw_footer("Deny", "Later");
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& out,
               CoreIntent& intent, uint32_t now_ms) {
  if(edge != ButtonEdge::ShortTap) return;
  if(g_phase != Phase::Live) return;

  auto record = [now_ms](perms::Decision d) {
    if(g_decision_recorded) return;
    const uint32_t wait_ms = (now_ms >= g_pending_arrived_ms)
        ? (now_ms - g_pending_arrived_ms) : 0;
    perms::record(g_pending_tool, d, now_ms, wait_ms);
    g_decision_recorded = true;
  };

  if(btn == Button::A) {
    record(perms::Decision::Approved);
    const uint32_t wait_s = (now_ms >= g_pending_arrived_ms)
        ? ((now_ms - g_pending_arrived_ms) / 1000u) : 0u;
    g_result_band = (wait_s < 5u) ? PromptResultBand::Quick
                  : (wait_s <= 20u) ? PromptResultBand::Normal
                                    : PromptResultBand::Slow;
    start_phase(Phase::ApproveBeat, now_ms);
    g_emit_decision = false;
  } else if(btn == Button::B) {
    record(perms::Decision::Denied);
    g_result_band = PromptResultBand::None;
    start_phase(Phase::DenyBeat, now_ms);
    g_emit_decision = false;
  } else if(btn == Button::Y) {
    record(perms::Decision::Dismissed);
    start_phase(Phase::DismissBeat, now_ms);
  }
  (void)out;
  (void)intent;
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::approval
