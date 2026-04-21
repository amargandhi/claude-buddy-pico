// pet.cpp — character-state dashboard for the desk buddy.
#include "pet.h"

#include <cstdio>

#include "../acting.h"
#include "../chrome.h"
#include "../companion.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"
#include "../permissions_log.h"

namespace ui_v2::screens::pet {

namespace {

enum class ActionFlash : uint8_t {
  None,
  Feed,
  Play,
};

ActionFlash g_flash = ActionFlash::None;
uint32_t    g_flash_until_ms = 0;

uint32_t approval_streak(const BuddyInputs& in) {
  const uint32_t n = perms::count();
  if(n == 0) return in.denials == 0 ? in.approvals : 0;

  uint32_t streak = 0;
  for(uint32_t i = 0; i < n; ++i) {
    const perms::Entry e = perms::get(i);
    if(e.decision != static_cast<uint8_t>(perms::Decision::Approved)) break;
    ++streak;
  }
  return streak;
}

const char* base_mood_name(const BuddyInputs& in, uint32_t streak) {
  if(in.pet_energy_pct <= 20) return "sleepy";
  if(in.pet_fed_pct <= 25) return "hungry";
  if(streak >= 8 && in.pet_fed_pct >= 65 && in.pet_energy_pct >= 55) return "proud";
  if(in.pet_energy_pct >= 75 && in.pet_fed_pct >= 70) return "happy";
  if(in.pet_energy_pct >= 55) return "playful";
  return "calm";
}

Rgb24 base_mood_color(const BuddyInputs& in, uint32_t streak) {
  if(in.pet_energy_pct <= 20) return kTextDim;
  if(in.pet_fed_pct <= 25) return kWarnAmber;
  if(streak >= 8 && in.pet_fed_pct >= 65 && in.pet_energy_pct >= 55) return kAccentClay;
  if(in.pet_energy_pct >= 75 && in.pet_fed_pct >= 70) return kOkGreen;
  if(in.pet_energy_pct >= 55) return kBusyBlue;
  return kTextWarm;
}

PersonaState mood_persona(const BuddyInputs& in, uint32_t streak, uint32_t now_ms) {
  if(now_ms < g_flash_until_ms) {
    return g_flash == ActionFlash::Feed ? PersonaState::Heart
                                        : PersonaState::Celebrate;
  }
  if(in.pet_energy_pct <= 20) return PersonaState::Sleep;
  if(in.pet_fed_pct <= 25) return PersonaState::Attention;
  if(streak >= 8 && in.pet_fed_pct >= 65 && in.pet_energy_pct >= 55) return PersonaState::Heart;
  if(in.pet_energy_pct >= 75 && in.pet_fed_pct >= 70) return PersonaState::Celebrate;
  if(in.pet_energy_pct >= 55) return PersonaState::Busy;
  return PersonaState::Idle;
}

const char* display_mood_name(const BuddyInputs& in, uint32_t streak, uint32_t now_ms) {
  if(acting::approval_pending(in)) {
    switch(acting::approval_wait_band(in, now_ms)) {
      case acting::ApprovalWaitBand::PromptNew:       return "approval";
      case acting::ApprovalWaitBand::PromptWaiting:   return "waiting";
      case acting::ApprovalWaitBand::PromptOverdue:   return "overdue";
      case acting::ApprovalWaitBand::PromptEscalated: return "urgent";
      case acting::ApprovalWaitBand::None:            return "approval";
    }
  }
  if(acting::prompt_result_active(in, now_ms)) {
    switch(in.prompt_result) {
      case PromptResult::Approved:
        if(in.prompt_result_band == PromptResultBand::Quick) return "loved";
        if(in.prompt_result_band == PromptResultBand::Normal) return "relief";
        return "earned";
      case PromptResult::Denied:
        return "denied";
      case PromptResult::TimedOut:
        return base_mood_name(in, streak);
      case PromptResult::None:
        break;
    }
  }
  return base_mood_name(in, streak);
}

Rgb24 display_mood_color(const BuddyInputs& in, uint32_t streak, uint32_t now_ms) {
  if(acting::approval_pending(in)) return acting::prompt_accent(in);
  if(acting::prompt_result_active(in, now_ms)) {
    switch(in.prompt_result) {
      case PromptResult::Approved:
        return (in.prompt_result_band == PromptResultBand::Quick)
            ? kAccentClay : kOkGreen;
      case PromptResult::Denied:
        return kDangerRed;
      case PromptResult::TimedOut:
        return base_mood_color(in, streak);
      case PromptResult::None:
        break;
    }
  }
  return base_mood_color(in, streak);
}

PersonaState display_persona(const BuddyInputs& in, uint32_t streak, uint32_t now_ms) {
  PersonaState override_persona = PersonaState::Idle;
  if(acting::pet_persona_override(in, now_ms, override_persona)) {
    return override_persona;
  }
  return mood_persona(in, streak, now_ms);
}

void format_age(char* out, size_t out_sz, uint32_t age_minutes) {
  const uint32_t days = age_minutes / (60u * 24u);
  const uint32_t hours = (age_minutes / 60u) % 24u;
  if(days > 0) std::snprintf(out, out_sz, "%ud %uh", days, hours);
  else         std::snprintf(out, out_sz, "%uh %um", hours, age_minutes % 60u);
}

void draw_meter(int x, int y, int bar_w, const char* label,
                uint8_t pct, Rgb24 accent) {
  char pct_buf[8];
  std::snprintf(pct_buf, sizeof(pct_buf), "%u%%", static_cast<unsigned>(pct));
  gfx::text(label, x, y, kTextDim, 1.0f);
  gfx::progress_bar(x + 42, y + 2, bar_w, 6, pct, 100, accent, kChipPanel);
  gfx::text_aligned(pct_buf, x + 42 + bar_w + 6, y - 2, 44, gfx::Align::Left,
                    kTextPrimary, 1.0f);
}

void enter(uint32_t /*now_ms*/) {
  g_flash = ActionFlash::None;
  g_flash_until_ms = 0;
}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& face,
          CoreIntent& /*intent*/, uint32_t now_ms) {
  (void)face;
  gfx::clear(kBgInk);

  const uint32_t streak = approval_streak(in);
  const Rgb24 accent = display_mood_color(in, streak, now_ms);
  const PersonaState persona = display_persona(in, streak, now_ms);

  char species[32];
  companion::label_current(species, sizeof(species));
  const char* mood = display_mood_name(in, streak, now_ms);
  const int mood_w = gfx::text_width(mood, 1.0f);
  char species_fit[32];
  gfx::fit_text(species_fit, sizeof(species_fit), species, 304 - mood_w - 18, 1.4f);
  gfx::text(species_fit, 8, 28, accent, 1.4f);
  gfx::text_aligned(mood, 0, 28, 312, gfx::Align::Right, accent, 1.0f);
  gfx::fill_rect(8, 46, 304, 1, kChipPanel);

  gfx::rounded_rect(16, 56, 172, 114, 8, lerp(kChipPanel, accent, 24));
  gfx::fill_rect(28, 72, 148, 1, lerp(kChipPanelHi, accent, 80));
  companion::draw_current(70, 78, persona, false);

  char age[16];
  format_age(age, sizeof(age), in.pet_age_minutes);
  char streak_buf[16];
  std::snprintf(streak_buf, sizeof(streak_buf), "%u", static_cast<unsigned>(streak));

  gfx::rounded_rect(200, 56, 104, 114, 8, kChipPanel);
  gfx::text("AGE", 210, 68, kTextDim, 1.0f);
  gfx::text(age, 210, 82, kTextPrimary, 1.1f);
  gfx::text("STREAK", 210, 106, kTextDim, 1.0f);
  gfx::text(streak_buf, 210, 120, accent, 1.2f);

  char feeds[16];
  char plays[16];
  std::snprintf(feeds, sizeof(feeds), "%u", static_cast<unsigned>(in.pet_feeds));
  std::snprintf(plays, sizeof(plays), "%u", static_cast<unsigned>(in.pet_plays));
  gfx::text("FEEDS", 210, 142, kTextDim, 1.0f);
  gfx::text(feeds, 254, 142, kTextPrimary, 1.0f);
  gfx::text("PLAYS", 210, 156, kTextDim, 1.0f);
  gfx::text(plays, 254, 156, kTextPrimary, 1.0f);

  draw_meter(16, 184, 198, "fed", in.pet_fed_pct, kWarnAmber);
  draw_meter(16, 198, 198, "energy", in.pet_energy_pct, kBusyBlue);

  chrome::draw_status_rail(in, "Stats", "Play", now_ms);
  chrome::draw_footer("Feed", "Home");
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& out,
               CoreIntent& intent, uint32_t now_ms) {
  if(edge == ButtonEdge::ShortTap) {
    if(btn == Button::A) {
      intent.replace = true;
      intent.target = ScreenId::Stats;
    } else if(btn == Button::B) {
      out.request_pet_feed = true;
      g_flash = ActionFlash::Feed;
      g_flash_until_ms = now_ms + 1400u;
    } else if(btn == Button::X) {
      out.request_pet_play = true;
      g_flash = ActionFlash::Play;
      g_flash_until_ms = now_ms + 1400u;
    } else if(btn == Button::Y) {
      intent.replace = true;
      intent.target = ScreenId::Home;
    }
  } else if(edge == ButtonEdge::HoldFired && btn == Button::A) {
    intent.push = true;
    intent.target = ScreenId::SystemMenu;
  }
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::pet
