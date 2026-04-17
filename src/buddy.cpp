#include "buddy.h"
#include "buddy_common.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "libraries/pico_graphics/pico_graphics.hpp"
#include "pico/time.h"

using namespace pimoroni;

namespace {

enum { B_SLEEP, B_IDLE, B_BUSY, B_ATTENTION, B_CELEBRATE, B_DIZZY, B_HEART };

PicoGraphics* buddy_graphics = nullptr;
int origin_x = 0;
int origin_y = 0;
int render_offset_x = 0;
int render_offset_y = 0;
uint8_t scale = 1;
uint32_t tick_count = 0;
uint32_t next_tick_at = 0;
uint16_t current_color = 0xffff;
Point cursor{};

constexpr uint32_t kTickMs = 200;
constexpr uint8_t kStateCount = 7;
constexpr int kClearPad = 6;

int scaled_y_base() {
  return BUDDY_Y_BASE * scale - (scale - 1) * 14;
}

void set_pen565(const uint16_t color) {
  if(buddy_graphics == nullptr) {
    return;
  }
  const uint8_t r5 = static_cast<uint8_t>((color >> 11) & 0x1f);
  const uint8_t g6 = static_cast<uint8_t>((color >> 5) & 0x3f);
  const uint8_t b5 = static_cast<uint8_t>(color & 0x1f);
  const uint8_t r = static_cast<uint8_t>((r5 * 255) / 31);
  const uint8_t g = static_cast<uint8_t>((g6 * 255) / 63);
  const uint8_t b = static_cast<uint8_t>((b5 * 255) / 31);
  buddy_graphics->set_pen(r, g, b);
}

int measure_fixed_width(const char* text) {
  if(buddy_graphics == nullptr || text == nullptr) {
    return 0;
  }
  return buddy_graphics->measure_text(text, static_cast<float>(scale), 0, true);
}

void draw_text(const char* text, const Point point) {
  if(buddy_graphics == nullptr || text == nullptr || text[0] == '\0') {
    return;
  }
  buddy_graphics->set_font("bitmap6");
  buddy_graphics->text(text, point, 2048, static_cast<float>(scale), 0.0f, 0, true);
}

int phase_lookup(const int phase, const int a, const int b, const int c, const int d) {
  switch(phase & 0x3) {
    case 0:
      return a;
    case 1:
      return b;
    case 2:
      return c;
    default:
      return d;
  }
}

void update_render_offsets(const uint8_t persona_state, const uint32_t now) {
  const int slow = static_cast<int>((now / 140) & 0x3);
  const int medium = static_cast<int>((now / 100) & 0x3);
  const int fast = static_cast<int>((now / 70) & 0x3);

  render_offset_x = 0;
  render_offset_y = 0;

  switch(persona_state) {
    case B_SLEEP:
      render_offset_y = phase_lookup(slow, 0, 1, 2, 1);
      break;
    case B_IDLE:
      render_offset_y = phase_lookup(slow, 0, 1, 0, -1);
      break;
    case B_BUSY:
      render_offset_x = phase_lookup(fast, 0, 1, 0, -1);
      render_offset_y = phase_lookup(medium, 0, -1, 0, 1);
      break;
    case B_ATTENTION:
      render_offset_x = phase_lookup(fast, -1, 1, -1, 1);
      render_offset_y = phase_lookup(medium, 0, -1, 0, 1);
      break;
    case B_CELEBRATE:
      render_offset_x = phase_lookup(fast, 0, 1, 0, -1);
      render_offset_y = phase_lookup(fast, 0, -2, -1, 0);
      break;
    case B_DIZZY:
      render_offset_x = phase_lookup(fast, -2, 0, 2, 0);
      render_offset_y = phase_lookup(medium, 0, 1, 0, -1);
      break;
    case B_HEART:
      render_offset_x = phase_lookup(medium, 0, 1, 0, -1);
      render_offset_y = phase_lookup(slow, 0, -1, -2, -1);
      break;
    default:
      break;
  }
}

}  // namespace

const int BUDDY_X_CENTER = 67;
const int BUDDY_CANVAS_W = 135;
const int BUDDY_Y_BASE = 30;
const int BUDDY_Y_OVERLAY = 6;
const int BUDDY_CHAR_W = 6;
const int BUDDY_CHAR_H = 8;

const uint16_t BUDDY_BG = 0x0000;
const uint16_t BUDDY_HEART = 0xF810;
const uint16_t BUDDY_DIM = 0x8410;
const uint16_t BUDDY_YEL = 0xFFE0;
const uint16_t BUDDY_WHITE = 0xFFFF;
const uint16_t BUDDY_CYAN = 0x07FF;
const uint16_t BUDDY_GREEN = 0x07E0;
const uint16_t BUDDY_PURPLE = 0xA01F;
const uint16_t BUDDY_RED = 0xF800;
const uint16_t BUDDY_BLUE = 0x041F;

extern const Species CAPYBARA_SPECIES;
extern const Species DUCK_SPECIES;
extern const Species GOOSE_SPECIES;
extern const Species BLOB_SPECIES;
extern const Species CAT_SPECIES;
extern const Species DRAGON_SPECIES;
extern const Species OCTOPUS_SPECIES;
extern const Species OWL_SPECIES;
extern const Species PENGUIN_SPECIES;
extern const Species TURTLE_SPECIES;
extern const Species SNAIL_SPECIES;
extern const Species GHOST_SPECIES;
extern const Species AXOLOTL_SPECIES;
extern const Species CACTUS_SPECIES;
extern const Species ROBOT_SPECIES;
extern const Species RABBIT_SPECIES;
extern const Species MUSHROOM_SPECIES;
extern const Species CHONK_SPECIES;

static const Species* const SPECIES_TABLE[] = {
    &CAPYBARA_SPECIES, &DUCK_SPECIES,    &GOOSE_SPECIES,   &BLOB_SPECIES,   &CAT_SPECIES,     &DRAGON_SPECIES,
    &OCTOPUS_SPECIES,  &OWL_SPECIES,     &PENGUIN_SPECIES, &TURTLE_SPECIES, &SNAIL_SPECIES,   &GHOST_SPECIES,
    &AXOLOTL_SPECIES,  &CACTUS_SPECIES,  &ROBOT_SPECIES,   &RABBIT_SPECIES, &MUSHROOM_SPECIES, &CHONK_SPECIES,
};
static constexpr uint8_t N_SPECIES = sizeof(SPECIES_TABLE) / sizeof(SPECIES_TABLE[0]);
static uint8_t current_species_idx = 0;
static uint8_t last_drawn_state = 0xff;
static uint8_t last_drawn_species = 0xff;
static PackDefinition custom_pack{};
static bool custom_selected = false;
static uint8_t custom_frame_idx = 0;
static uint8_t custom_state_idx = 0xff;
static uint32_t custom_next_frame_at = 0;

void buddyPrintLine(const char* line, int y_px, uint16_t color, int x_off);

void render_custom_species(const uint8_t persona_state, const uint32_t now) {
  if(!custom_pack.valid) {
    return;
  }

  if(custom_state_idx != persona_state) {
    custom_state_idx = persona_state;
    custom_frame_idx = 0;
    custom_next_frame_at = 0;
  }

  const PackState& pack_state = custom_pack.states[persona_state];
  const char* frame = custom_pack.name;
  if(pack_state.nFrames > 0) {
    if(custom_next_frame_at == 0 || static_cast<int32_t>(now - custom_next_frame_at) >= 0) {
      if(custom_next_frame_at != 0 && pack_state.nFrames > 1) {
        custom_frame_idx = static_cast<uint8_t>((custom_frame_idx + 1) % pack_state.nFrames);
      }
      custom_next_frame_at = now + std::max<uint16_t>(pack_state.delayMs, 80);
    }
    frame = pack_state.frames[custom_frame_idx];
  }

  buddyPrintLine(frame, scaled_y_base() + (18 * scale), custom_pack.bodyColor, 0);
  buddyPrintLine(custom_pack.name, scaled_y_base() + (36 * scale), custom_pack.textDimColor, 0);
}

void buddyInit() {
  tick_count = 0;
  next_tick_at = 0;
  current_species_idx = 0;
  last_drawn_state = 0xff;
  last_drawn_species = 0xff;
  custom_selected = false;
  custom_pack = {};
  custom_frame_idx = 0;
  custom_state_idx = 0xff;
  custom_next_frame_at = 0;
}

void buddySetTarget(PicoGraphics* graphics, const int x, const int y) {
  buddy_graphics = graphics;
  origin_x = x;
  origin_y = y;
}

void buddyPrintLine(const char* line, const int y_px, const uint16_t color, const int x_off) {
  if(buddy_graphics == nullptr || line == nullptr) {
    return;
  }

  const char* render = line;
  size_t length = std::strlen(render);
  if(scale > 1) {
    while(length > 0 && render[length - 1] == ' ') {
      --length;
    }
    while(length > 0 && *render == ' ') {
      ++render;
      --length;
    }
  }

  std::string trimmed(render, length);
  const int width = measure_fixed_width(trimmed.c_str());
  const int x = origin_x + render_offset_x + BUDDY_X_CENTER - (width / 2) + (x_off * scale);
  const int y = origin_y + render_offset_y + y_px;
  set_pen565(color);
  draw_text(trimmed.c_str(), Point(x, y));
}

void buddyPrintSprite(const char* const* lines, const uint8_t n_lines, const int y_offset, const uint16_t color, const int x_off) {
  if(lines == nullptr) {
    return;
  }
  const int y_base = origin_y + scaled_y_base();
  for(uint8_t i = 0; i < n_lines; ++i) {
    buddyPrintLine(lines[i], y_base - origin_y + ((y_offset + (i * BUDDY_CHAR_H)) * scale), color, x_off);
  }
}

void buddySetCursor(const int x, const int y) {
  cursor.x = origin_x + render_offset_x + BUDDY_X_CENTER + ((x - BUDDY_X_CENTER) * scale);
  cursor.y = origin_y + render_offset_y + (y * scale);
}

void buddySetColor(const uint16_t fg) {
  current_color = fg;
}

void buddyPrint(const char* s) {
  if(buddy_graphics == nullptr || s == nullptr || s[0] == '\0') {
    return;
  }
  set_pen565(current_color);
  draw_text(s, cursor);
  cursor.x += measure_fixed_width(s);
}

void buddySetSpeciesIdx(const uint8_t idx) {
  if(idx == 0xff && custom_pack.valid) {
    custom_selected = true;
    buddyInvalidate();
    return;
  }
  if(idx < N_SPECIES) {
    custom_selected = false;
    current_species_idx = idx;
    buddyInvalidate();
  }
}

void buddySetSpecies(const char* name) {
  if(name == nullptr) {
    return;
  }
  for(uint8_t i = 0; i < N_SPECIES; ++i) {
    if(std::strcmp(SPECIES_TABLE[i]->name, name) == 0) {
      current_species_idx = i;
      buddyInvalidate();
      return;
    }
  }
}

const char* buddySpeciesName() {
  if(custom_selected && custom_pack.valid) {
    return custom_pack.name;
  }
  return SPECIES_TABLE[current_species_idx]->name;
}

uint16_t buddySpeciesColor() {
  if(custom_selected && custom_pack.valid) {
    return custom_pack.bodyColor;
  }
  return SPECIES_TABLE[current_species_idx]->bodyColor;
}

uint8_t buddySpeciesCount() {
  return custom_pack.valid ? static_cast<uint8_t>(N_SPECIES + 1) : N_SPECIES;
}

uint8_t buddySpeciesIdx() {
  if(custom_selected && custom_pack.valid) {
    return 0xff;
  }
  return current_species_idx;
}

void buddyNextSpecies() {
  if(custom_pack.valid) {
    if(custom_selected) {
      custom_selected = false;
      current_species_idx = 0;
    } else if(current_species_idx + 1 >= N_SPECIES) {
      custom_selected = true;
    } else {
      current_species_idx += 1;
    }
  } else {
    current_species_idx = (current_species_idx + 1) % N_SPECIES;
  }
  buddyInvalidate();
}

void buddyInvalidate() {
  last_drawn_state = 0xff;
  custom_frame_idx = 0;
  custom_state_idx = 0xff;
  custom_next_frame_at = 0;
}

void buddySetPeek(const bool peek) {
  const uint8_t new_scale = peek ? 1 : 2;
  if(scale == new_scale) {
    return;
  }
  scale = new_scale;
  buddyInvalidate();
}

void buddyTick(uint8_t persona_state) {
  if(buddy_graphics == nullptr) {
    return;
  }

  const uint32_t now = to_ms_since_boot(get_absolute_time());
  if(static_cast<int32_t>(now - next_tick_at) >= 0) {
    next_tick_at = now + kTickMs;
    tick_count++;
  }

  if(persona_state >= kStateCount) {
    persona_state = B_IDLE;
  }

  update_render_offsets(persona_state, now);

  last_drawn_state = persona_state;
  last_drawn_species = current_species_idx;

  set_pen565(BUDDY_BG);
  buddy_graphics->rectangle(
      Rect(origin_x - kClearPad,
           origin_y - kClearPad,
           BUDDY_CANVAS_W + (kClearPad * 2),
           ((BUDDY_Y_BASE + (5 * BUDDY_CHAR_H) + 12) * scale) + (kClearPad * 2)));

  buddy_graphics->set_font("bitmap6");
  if(custom_selected && custom_pack.valid) {
    render_custom_species(persona_state, now);
    return;
  }

  const Species* species = SPECIES_TABLE[current_species_idx];
  if(species->states[persona_state] != nullptr) {
    species->states[persona_state](tick_count);
  }
}

void buddySetCustomSpecies(const PackDefinition* pack) {
  if(pack == nullptr || !pack->valid) {
    return;
  }
  custom_pack = *pack;
  custom_selected = true;
  buddyInvalidate();
}

void buddyClearCustomSpecies() {
  custom_pack = {};
  custom_selected = false;
  buddyInvalidate();
}

bool buddyHasCustomSpecies() {
  return custom_pack.valid;
}

bool buddyCustomSpeciesSelected() {
  return custom_selected && custom_pack.valid;
}
