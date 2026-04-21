#include "companion.h"

#include <algorithm>
#include <cstdio>

#include "buddy.h"
#include "gfx.h"

namespace ui_v2::companion {

namespace {

Rgb24 rgb565_to_rgb24(uint16_t c) {
  const uint8_t r5 = static_cast<uint8_t>((c >> 11) & 0x1f);
  const uint8_t g6 = static_cast<uint8_t>((c >> 5) & 0x3f);
  const uint8_t b5 = static_cast<uint8_t>(c & 0x1f);
  return Rgb24{
      static_cast<uint8_t>((r5 * 255) / 31),
      static_cast<uint8_t>((g6 * 255) / 63),
      static_cast<uint8_t>((b5 * 255) / 31),
  };
}

uint8_t clamp_slot(uint8_t slot) {
  const uint8_t count = slot_count();
  if(count == 0) return 0;
  return std::min<uint8_t>(slot, count - 1);
}

}  // namespace

uint8_t slot_count() {
  return buddySpeciesCount();
}

uint8_t selected_slot() {
  const uint8_t count = slot_count();
  if(count == 0) return 0;
  if(buddyCustomSpeciesSelected()) return static_cast<uint8_t>(count - 1);
  return std::min<uint8_t>(buddySpeciesIdx(), count - 1);
}

uint8_t species_index_for_slot(uint8_t slot) {
  const uint8_t count = slot_count();
  if(count == 0) return 0;
  const uint8_t clamped = clamp_slot(slot);
  if(buddyHasCustomSpecies() && clamped == static_cast<uint8_t>(count - 1)) {
    return 0xff;
  }
  return clamped;
}

void label_for_slot(uint8_t slot, char* out, size_t out_size) {
  if(out_size == 0) return;
  out[0] = '\0';

  const uint8_t saved = buddySpeciesIdx();
  buddySetSpeciesIdx(species_index_for_slot(slot));
  std::snprintf(out, out_size, "%s", buddySpeciesName());
  buddySetSpeciesIdx(saved);
}

Rgb24 accent_for_slot(uint8_t slot) {
  const uint8_t saved = buddySpeciesIdx();
  buddySetSpeciesIdx(species_index_for_slot(slot));
  const uint16_t color = buddySpeciesColor();
  buddySetSpeciesIdx(saved);
  return rgb565_to_rgb24(color);
}

void label_current(char* out, size_t out_size) {
  if(out_size == 0) return;
  out[0] = '\0';
  std::snprintf(out, out_size, "%s", buddySpeciesName());
}

Rgb24 accent_current() {
  return rgb565_to_rgb24(buddySpeciesColor());
}

void draw_preview(int origin_x, int origin_y, uint8_t slot,
                  PersonaState persona, bool compact) {
  auto* g = gfx::native_graphics();
  if(g == nullptr) return;

  const uint8_t saved = buddySpeciesIdx();
  buddySetSpeciesIdx(species_index_for_slot(slot));
  buddySetTarget(g, origin_x, origin_y);
  buddySetPeek(compact);
  buddyTick(static_cast<uint8_t>(persona));
  buddySetSpeciesIdx(saved);
  buddySetPeek(false);
}

void draw_current(int origin_x, int origin_y, PersonaState persona, bool compact) {
  auto* g = gfx::native_graphics();
  if(g == nullptr) return;

  buddySetTarget(g, origin_x, origin_y);
  buddySetPeek(compact);
  buddyTick(static_cast<uint8_t>(persona));
  buddySetPeek(false);
}

}  // namespace ui_v2::companion
