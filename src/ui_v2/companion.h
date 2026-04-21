#pragma once

#include <cstddef>
#include <cstdint>

#include "palette.h"
#include "ui_state.h"

namespace ui_v2::companion {

uint8_t slot_count();
uint8_t selected_slot();
uint8_t species_index_for_slot(uint8_t slot);
void label_for_slot(uint8_t slot, char* out, size_t out_size);
Rgb24 accent_for_slot(uint8_t slot);
void label_current(char* out, size_t out_size);
Rgb24 accent_current();

void draw_preview(int origin_x, int origin_y, uint8_t slot,
                  PersonaState persona, bool compact);
void draw_current(int origin_x, int origin_y, PersonaState persona, bool compact);

}  // namespace ui_v2::companion
