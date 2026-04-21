// home.cpp — home screen implementation.
#include "home.h"

#include <cstdio>
#include <cstring>

#include "../chrome.h"
#include "../companion.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"
#include "reset.h"

namespace ui_v2::screens::home {

namespace {

// Latched in tick() each frame, read by on_button. on_button does not get
// BuddyInputs, but the A-button behavior needs to know whether there is a
// pending approval (to route to Approval instead of SystemMenu). The only
// window where this races is the ~1 frame (40 ms) between a prompt
// arriving and the user pressing — well below human reaction, and ui_core
// will auto-push Approval on that rising edge anyway.
bool g_pending_prompt = false;

void enter(uint32_t /*now_ms*/) {}

PersonaState companion_persona(const BuddyInputs& in) {
  if(in.prompt_active && !in.prompt_sent) return PersonaState::Attention;
  return in.persona;
}

Rgb24 status_color(const BuddyInputs& in) {
  if(in.prompt_active && !in.prompt_sent) return kDangerRed;
  if(in.persona == PersonaState::Sleep) return kTextDim;
  if(in.running > 0) return kBusyBlue;
  if(in.waiting > 0) return kWarnAmber;
  return kTextPrimary;
}

const char* hero_title(const BuddyInputs& in) {
  if(in.prompt_active && !in.prompt_sent) return "Approval Needed";
  if(in.persona == PersonaState::Sleep) return "Sleeping";
  if(in.running > 0) return "Claude Working";
  if(in.waiting > 0) return "Waiting";
  return "Ready";
}

void hero_subtitle(const BuddyInputs& in, char* out, size_t n) {
  if(n == 0) return;
  out[0] = '\0';
  if(in.prompt_active && !in.prompt_sent) {
    if(in.prompt_tool[0]) {
      std::snprintf(out, n, "Press Open to review %s", in.prompt_tool);
    } else {
      std::snprintf(out, n, "Press Open to review the pending request");
    }
    return;
  }
  if(in.msg[0]) {
    std::snprintf(out, n, "%s", in.msg);
    return;
  }
  if(in.running > 0 || in.waiting > 0) {
    std::snprintf(out, n, "%u running, %u waiting",
                  static_cast<unsigned>(in.running),
                  static_cast<unsigned>(in.waiting));
    return;
  }
  if(in.persona == PersonaState::Sleep) {
    std::snprintf(out, n, "Press Nap again to wake");
    return;
  }
  std::snprintf(out, n, "Connected and ready for the next thing");
}

void draw_header(const BuddyInputs& in, Rgb24 accent) {
  char name[32];
  companion::label_current(name, sizeof(name));
  char today[24];
  gfx::format_compact(today, sizeof(today), in.tokens_today);

  char today_label[32];
  std::snprintf(today_label, sizeof(today_label), "%s today", today);
  const int today_w = gfx::text_width(today_label, 1.0f);
  char name_fit[32];
  gfx::fit_text(name_fit, sizeof(name_fit), name, 304 - today_w - 18, 1.4f);
  gfx::text(name_fit, 8, 28, accent, 1.4f);
  gfx::text_aligned(today_label, 0, 28, 312, gfx::Align::Right, kTextDim, 1.0f);
  gfx::fill_rect(8, 46, 304, 1, kChipPanel);
}

void draw_meta_row(const BuddyInputs& in) {
  char ap[16], dn[16], lv[16];
  gfx::format_thousands(ap, sizeof(ap), in.approvals);
  gfx::format_thousands(dn, sizeof(dn), in.denials);
  std::snprintf(lv, sizeof(lv), "L%u", static_cast<unsigned>(in.level));

  gfx::text("approve", 16, 198, kTextDim, 1.0f);
  gfx::text(ap, 64, 198, kOkGreen, 1.0f);
  gfx::text("deny", 128, 198, kTextDim, 1.0f);
  gfx::text(dn, 160, 198, kDangerRed, 1.0f);
  gfx::text("level", 224, 198, kTextDim, 1.0f);
  gfx::text(lv, 260, 198, kAccentClay, 1.0f);
}

void tick(const BuddyInputs& in, BuddyOutputs& out, FaceState& face,
          CoreIntent& intent, uint32_t now_ms) {
  // Modal auto-push for prompts now lives in ui_core (see tick_40ms
  // rising-edge block) so it works from any screen, not just Home.
  // Home's tick no longer needs to raise navigation intents — all of
  // Home's navigation is button-driven and lives in on_button.
  (void)intent;

  (void)out;
  (void)face;
  gfx::clear(kBgInk);

  const Rgb24 accent = companion::accent_current();
  draw_header(in, accent);
  companion::draw_current(92, 48, companion_persona(in), false);

  char line[128];
  if(in.prompt_active && !in.prompt_sent) {
    if(in.prompt_tool[0]) std::snprintf(line, sizeof(line), "Open %s permission", in.prompt_tool);
    else                  std::snprintf(line, sizeof(line), "Open pending approval");
  } else if(in.msg[0]) {
    std::snprintf(line, sizeof(line), "%s", in.msg);
  } else if(in.running > 0 || in.waiting > 0) {
    std::snprintf(line, sizeof(line), "%s", hero_title(in));
  } else {
    hero_subtitle(in, line, sizeof(line));
  }
  char fit[128];
  gfx::fit_text(fit, sizeof(fit), line, 292, 1.2f);
  gfx::text_aligned(fit, 0, 174, geom::kPanelW,
                    gfx::Align::Center, status_color(in), 1.2f);
  draw_meta_row(in);

  // Corner hints: A=top-left, X=top-right, B=bottom-left, Y=bottom-right.
  // When an approval is pending (user pressed "Later" on the modal, or we
  // re-entered Home while a prompt was still active without an auto-push
  // edge), relabel A from "Menu" to "Open" so there is a visible path back
  // to the pending prompt. Without this, Y=Later on Approval is a trap —
  // the red banner tells the user something is pending but no button
  // admits it. See approval.cpp on_button Button::Y rationale.
  g_pending_prompt = in.prompt_active && !in.prompt_sent;
  chrome::draw_status_rail(in, g_pending_prompt ? "Open" : "Menu", "Stats", now_ms);
  chrome::draw_footer("Buddy", "Nap");
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& out,
               CoreIntent& intent, uint32_t /*now_ms*/) {
  if(edge == ButtonEdge::ShortTap) {
    if(btn == Button::A) {
      // Rail label is "Open" when there is a pending approval and "Menu"
      // otherwise; route the press to match the label. See tick() comment
      // for the rationale — this closes the Later-button dead-end.
      intent.push   = true;
      intent.target = g_pending_prompt ? ScreenId::Approval : ScreenId::SystemMenu;
    }
    else if(btn == Button::B) { intent.push = true; intent.target = ScreenId::Buddy; }
    else if(btn == Button::X) { intent.push = true; intent.target = ScreenId::Stats; }
    else if(btn == Button::Y) { out.request_nap_toggle = true; }
  } else if(edge == ButtonEdge::HoldFired) {
    // When a prompt is pending, A-short has become "Open"; don't also
    // interpret A-hold as "factory reset" since a slightly-too-long press
    // by a user trying to open the pending approval would land them in a
    // destructive confirmation flow. Swallow A-hold in that mode.
    if(btn == Button::A && !g_pending_prompt) {
      reset::prepare(reset::Mode::FactoryReset);
      intent.push = true; intent.target = ScreenId::Reset;
    }
    else if(btn == Button::Y) { out.request_screen_off = true; }
  }
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::home
