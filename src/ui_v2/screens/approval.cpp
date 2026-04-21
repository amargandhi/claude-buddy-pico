// approval.cpp — approval overlay implementation.
#include "approval.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

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
};

Phase    g_phase = Phase::Arrival;
uint32_t g_phase_started_ms = 0;
bool     g_emit_decision = false;

char     g_pending_tool[48] = "";
uint32_t g_pending_arrived_ms = 0;
bool     g_decision_recorded = false;

bool ascii_eq(const char* a, const char* b) {
  if(!a || !b) return a == b;
  return std::strcmp(a, b) == 0;
}

bool ascii_starts(const char* s, const char* prefix) {
  if(!s || !prefix) return false;
  while(*prefix) {
    if(*s++ != *prefix++) return false;
  }
  return true;
}

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

ExpressionId live_expression(const BuddyInputs& in, RiskTier t) {
  if(in.face_style == FaceStyle::Classic) return tier_expression(t);

  const char* tool = in.prompt_tool;
  if(ascii_eq(tool, "Read") || ascii_eq(tool, "Glob") || ascii_eq(tool, "Grep")) {
    return ExpressionId::Focused;
  }
  if(ascii_eq(tool, "WebSearch") || ascii_eq(tool, "TaskList") || ascii_eq(tool, "TaskGet")) {
    return ExpressionId::Scan;
  }
  if(ascii_eq(tool, "WebFetch") || ascii_eq(tool, "TodoWrite") ||
     ascii_eq(tool, "Skill") || ascii_eq(tool, "TaskStop")) {
    return ExpressionId::GlyphCircle;
  }
  if(ascii_eq(tool, "Edit") || ascii_eq(tool, "Write") ||
     ascii_eq(tool, "NotebookEdit") || ascii_eq(tool, "TaskCreate") ||
     ascii_eq(tool, "TaskUpdate")) {
    return ExpressionId::Skeptical;
  }
  if(ascii_eq(tool, "Bash")) {
    return ExpressionId::GlyphCaretIn;
  }
  if(ascii_eq(tool, "Agent") || ascii_starts(tool, "mcp__")) {
    return ExpressionId::Alert;
  }
  return tier_expression(t);
}

ExpressionId arrival_expression(const BuddyInputs& in, RiskTier t) {
  if(in.face_style == FaceStyle::Classic) return tier_expression(t);
  if(t == RiskTier::Danger) return ExpressionId::GlyphWideCircle;
  if(t == RiskTier::Neutral) return ExpressionId::GlyphCircle;
  if(t == RiskTier::Safe) return ExpressionId::Focused;
  return ExpressionId::Alert;
}

ExpressionId result_expression(const BuddyInputs& in, bool approve) {
  if(in.face_style == FaceStyle::Classic) {
    return approve ? ExpressionId::Happy : ExpressionId::Squint;
  }
  return approve ? ExpressionId::GlyphArcUp : ExpressionId::GlyphCaretIn;
}

Rgb24 tier_panel(RiskTier t, uint8_t strength) {
  return lerp(kBgInkDeep, tier_color(t), strength);
}

void enter(uint32_t now_ms) {
  g_phase = Phase::Arrival;
  g_phase_started_ms = now_ms;
  g_emit_decision = false;
  g_pending_tool[0] = '\0';
  g_pending_arrived_ms = now_ms;
  g_decision_recorded = false;
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
  std::snprintf(g_pending_tool, sizeof(g_pending_tool), "%s", in.prompt_tool);
  g_pending_arrived_ms = in.prompt_arrived_ms;

  if(!in.prompt_active || in.prompt_sent) {
    if(!g_decision_recorded && !in.prompt_sent) {
      const uint32_t wait_ms = (now_ms >= g_pending_arrived_ms)
          ? (now_ms - g_pending_arrived_ms) : 0;
      perms::record(g_pending_tool, perms::Decision::Timeout, now_ms, wait_ms);
      g_decision_recorded = true;
    }
    intent.pop = true;
    return;
  }

  const RiskTier tier = risk::classify(in.prompt_tool);
  const Rgb24 accent = tier_color(tier);
  const uint32_t phase_age = now_ms - g_phase_started_ms;
  if(g_phase == Phase::Arrival && phase_age >= 260) {
    g_phase = Phase::Live;
    g_phase_started_ms = now_ms;
  }

  face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kApprovalEyeCy);
  face::set_ambient(face, false, false);

  if(g_phase == Phase::Arrival) {
    gfx::fill_rect(0, 0, geom::kPanelW, geom::kPanelH, kCaseOrange);
    gfx::fill_rect(0, 0, geom::kPanelW, 3, accent);
    face::set_ink(face, kBgInkDeep);
    const ExpressionId want = arrival_expression(in, tier);
    if(face.target != want) {
      face::begin_transition(face, want, now_ms, 120);
    }
    face::tick_and_render(face, now_ms);
    gfx::text_aligned("CLAUDE NEEDS PERMISSION", 0, 148, geom::kPanelW,
                      gfx::Align::Center, kBgInkDeep, 1.3f);
    return;
  }

  if(g_phase == Phase::ApproveBeat || g_phase == Phase::DenyBeat) {
    const bool approve = (g_phase == Phase::ApproveBeat);
    const Rgb24 beat_bg = approve
        ? lerp(kCaseOrange, kOkGreen, 96)
        : lerp(kCaseOrange, kDangerRed, 96);
    gfx::fill_rect(0, 0, geom::kPanelW, geom::kPanelH, beat_bg);
    gfx::fill_rect(0, 0, geom::kPanelW, 3, approve ? kOkGreen : kDangerRed);
    face::set_ink(face, kBgInkDeep);
    const ExpressionId want = result_expression(in, approve);
    if(face.target != want) {
      face::begin_transition(face, want, now_ms, 120);
    }
    face::tick_and_render(face, now_ms);
    gfx::text_aligned(approve ? "APPROVED" : "DENIED", 0, 150, geom::kPanelW,
                      gfx::Align::Center, kBgInkDeep, 1.8f);
    if(!g_emit_decision && phase_age >= 200) {
      if(approve) out.send_approve_once = true;
      else        out.send_deny = true;
      g_emit_decision = true;
    }
    return;
  }

  gfx::clear(kBgInk);
  gfx::fill_rect(0, 0, geom::kPanelW, 3, accent);
  gfx::rounded_rect(12, 28, 296, 72, 8, tier_panel(tier, 36));
  gfx::rounded_rect(12, 144, 296, 56, 6, tier_panel(tier, 28));
  gfx::fill_rect(12, 138, 296, 1, kChipPanelHi);
  gfx::fill_rect(12, 202, 296, 1, kChipPanelHi);

  face::set_ink(face, kTextPrimary);
  const ExpressionId want = live_expression(in, tier);
  if(face.target != want) {
    face::begin_transition(face, want, now_ms, 140);
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
                risk::tier_name(tier), in.prompt_tool[0] ? in.prompt_tool : "unknown");
  gfx::text(tool_meta, 12, 126, kTextDim, 1.0f);
  char waited_label[24];
  std::snprintf(waited_label, sizeof(waited_label), "WAIT %s", waited);
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
    g_phase = Phase::ApproveBeat;
    g_phase_started_ms = now_ms;
    g_emit_decision = false;
  } else if(btn == Button::B) {
    record(perms::Decision::Denied);
    g_phase = Phase::DenyBeat;
    g_phase_started_ms = now_ms;
    g_emit_decision = false;
  } else if(btn == Button::Y) {
    record(perms::Decision::Dismissed);
    intent.pop = true;
  }
  (void)out;
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::approval
