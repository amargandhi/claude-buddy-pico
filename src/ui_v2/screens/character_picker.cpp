#include "character_picker.h"

#include <cstdio>

#include "../chrome.h"
#include "../companion.h"
#include "../gfx.h"
#include "../palette.h"

namespace ui_v2::screens::character_picker {

namespace {

uint8_t g_slot = 0;

void enter(uint32_t /*now_ms*/) {
  g_slot = companion::selected_slot();
}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& /*face*/,
          CoreIntent& /*intent*/, uint32_t now_ms) {
  gfx::clear(kBgInk);

  companion::draw_preview(92, 58, g_slot, PersonaState::Celebrate, false);

  char name[32];
  companion::label_for_slot(g_slot, name, sizeof(name));
  const Rgb24 accent = companion::accent_for_slot(g_slot);
  char meta[24];
  const unsigned count = companion::slot_count();
  std::snprintf(meta, sizeof(meta), "%u / %u", static_cast<unsigned>(g_slot) + 1, count);

  const int meta_w = gfx::text_width(meta, 1.0f);
  char name_fit[32];
  gfx::fit_text(name_fit, sizeof(name_fit), name, 304 - meta_w - 18, 1.4f);
  gfx::text(name_fit, 8, 28, accent, 1.4f);
  gfx::text_aligned(meta, 0, 28, 312, gfx::Align::Right, kTextWarm, 1.0f);
  gfx::fill_rect(8, 46, 304, 1, kChipPanel);
  gfx::text_aligned("desk companion", 0, 184, 320, gfx::Align::Center, kTextDim, 1.0f);

  chrome::draw_status_rail(in, "Prev", "Next", now_ms);
  chrome::draw_footer("Select", "Back");
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& out,
               CoreIntent& intent, uint32_t now_ms) {
  if(edge != ButtonEdge::ShortTap) return;
  const uint8_t count = companion::slot_count();
  if(count == 0) {
    if(btn == Button::Y) intent.pop = true;
    return;
  }
  if(btn == Button::A) {
    g_slot = (g_slot == 0) ? static_cast<uint8_t>(count - 1) : static_cast<uint8_t>(g_slot - 1);
  } else if(btn == Button::X) {
    g_slot = static_cast<uint8_t>((g_slot + 1) % count);
  } else if(btn == Button::B) {
    out.set_character_index = true;
    out.character_index = companion::species_index_for_slot(g_slot);
    out.set_persona = true;
    out.persona_override = PersonaState::Heart;
    out.persona_until_ms = now_ms + 1200u;
  } else if(btn == Button::Y) {
    intent.pop = true;
  }
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::character_picker
