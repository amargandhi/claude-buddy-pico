// claude_buddy_pico.cpp
//
// Pico Buddy main firmware. This file is intentionally a single translation
// unit for v1.
//
// Why: bring-up required rapid iteration across BLE, persistence, UI state,
// and input handling at the same time, with real hardware as the only way to
// verify behavior. A premature split would have meant breaking working code
// that could not easily be re-verified in bulk. The tradeoff was landing
// working firmware faster at the cost of a large file.
//
// V2 plans to split this along BLE, UI, state, buddy, and persistence
// boundaries once the protocol surface stops moving. Until then, navigate
// this file by section: constants, state, storage helpers, BLE event
// handlers, UI rendering, main loop.
//
// See docs/project-plan.md for the overall v1/v2 structure.

#include <algorithm>
#include <cinttypes>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "btstack.h"
#include "btstack_base64_decoder.h"
#include "ble/gatt-service/nordic_spp_service_server.h"

#include "buddy.h"
#include "button.hpp"
#include "drivers/st7789/st7789.hpp"
#include "hardware/adc.h"
#include "hardware/flash.h"
#include "libraries/pico_graphics/pico_graphics.hpp"
#include "pico/btstack_flash_bank.h"
#include "pico/cyw43_arch.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico_display_28.hpp"
#include "rgbled.hpp"

#include "claude_buddy_pico.h"

using namespace pimoroni;

namespace {

constexpr uint32_t kUiTickMs = 40;
constexpr uint32_t kBatterySampleMs = 2000;
constexpr uint32_t kPromptKeepAliveMs = 30000;
constexpr uint32_t kDimAfterMs = 20000;
constexpr uint32_t kScreenOffAfterMs = 30000;
constexpr uint32_t kHoldThresholdMs = 800;
constexpr float kAdcReferenceVolts = 3.3f;
constexpr float kVoltageDividerScale = 3.0f;
constexpr uint32_t kDefaultPasskey = 123456;
constexpr uint32_t kTokensPerLevel = 50000;
constexpr uint32_t kDemoStepMs = 8000;
constexpr uint32_t kStorageOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
constexpr uint32_t kPersistDebounceMs = 2000;
constexpr uint32_t kStorageMagic = 0x43425032;
constexpr uint16_t kStorageVersion = 3;
constexpr uint8_t kCustomSpeciesSentinel = 0xff;
constexpr uint8_t kInfoPageCount = 7;
constexpr uint8_t kPetPageCount = 2;
constexpr uint8_t kMenuCount = 6;
constexpr uint8_t kSettingsCount = 8;
constexpr uint8_t kResetCount = 3;

constexpr int kLeftX = 8;
constexpr int kLeftY = 12;
constexpr int kLeftW = 140;
constexpr int kLeftH = 212;
constexpr int kRightX = 156;
constexpr int kRightY = 12;
constexpr int kRightW = 156;
constexpr int kRightH = 212;
constexpr int kFooterY = 226;

constexpr uint16_t C_BG = 0x0000;
constexpr uint16_t C_PANEL = 0x18C3;
constexpr uint16_t C_PANEL_ALT = 0x2104;
constexpr uint16_t C_TEXT = 0xFFFF;
constexpr uint16_t C_DIM = 0x8410;
constexpr uint16_t C_HOT = 0xFA20;
constexpr uint16_t C_GREEN = 0x07E0;
constexpr uint16_t C_BLUE = 0x041F;
constexpr uint16_t C_GOLD = 0xFFE0;
constexpr uint16_t C_WHITE = 0xFFFF;
constexpr uint8_t kLedMax = 72;
constexpr char kDefaultDeviceName[] = "Claude Pico";

constexpr uint8_t kBrightnessLevels[] = {48, 88, 128, 176, 220};

static_assert(kStorageOffset + FLASH_SECTOR_SIZE <= PICO_FLASH_SIZE_BYTES, "Persistent storage must fit in flash");
static_assert(kStorageOffset >= PICO_FLASH_BANK_STORAGE_OFFSET + PICO_FLASH_BANK_TOTAL_SIZE,
              "Persistent storage must not overlap BTstack flash bank");

enum PersonaState : uint8_t {
  P_SLEEP,
  P_IDLE,
  P_BUSY,
  P_ATTENTION,
  P_CELEBRATE,
  P_DIZZY,
  P_HEART,
};

enum class DisplayMode : uint8_t {
  Normal,
  Pet,
  Info,
};

const char* state_names[] = {"sleep", "idle", "busy", "attention", "celebrate", "dizzy", "heart"};

struct DemoScenario {
  const char* name;
  uint32_t total;
  uint32_t running;
  uint32_t waiting;
  bool completed;
  uint32_t tokens_today;
  const char* msg;
  const char* entries[4];
  uint8_t entry_count;
};

const DemoScenario demo_scenarios[] = {
    {"asleep", 0, 0, 0, false, 0, "Demo: waiting for Claude", {"dock mode idle"}, 1},
    {"idle", 1, 0, 0, false, 12000, "Demo: all clear", {"workspace calm", "ready to help"}, 2},
    {"busy", 4, 3, 0, false, 89000, "Demo: Claude is working", {"reading files", "running tools", "writing code"}, 3},
    {"attention", 2, 1, 1, false, 45000, "Demo: approval pending", {"tool wants access", "A approve, B deny"}, 2},
    {"celebrate", 5, 0, 0, true, 142000, "Demo: task wrapped up", {"tests passed", "tokens milestone"}, 2},
};

struct PersistentBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint8_t brightness_level;
  uint8_t led_enabled;
  uint8_t transcript_enabled;
  uint8_t demo_mode;
  uint8_t clock_enabled;
  uint8_t ascii_species_idx;
  uint8_t custom_selected;
  uint8_t reserved0;
  char device_name[32];
  char owner[32];
  uint32_t approvals;
  uint32_t denials;
  uint32_t nap_seconds;
  uint32_t tokens_total;
  uint32_t level;
  uint16_t velocity[8];
  uint8_t vel_idx;
  uint8_t vel_count;
  uint8_t custom_valid;
  uint8_t reserved1;
  PackDefinition custom_pack;
  uint32_t crc;
};

ST7789 st7789(PicoDisplay28::WIDTH, PicoDisplay28::HEIGHT, ROTATE_0, false, get_spi_pins(BG_SPI_FRONT));
PicoGraphics_PenRGB332 graphics(st7789.width, st7789.height, nullptr);
RGBLED led(PicoDisplay28::LED_R, PicoDisplay28::LED_G, PicoDisplay28::LED_B);

Button button_a(PicoDisplay28::A);
Button button_b(PicoDisplay28::B);
Button button_x(PicoDisplay28::X);
Button button_y(PicoDisplay28::Y);

const uint8_t adv_data[] = {
    2, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    12, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'C', 'l', 'a', 'u', 'd', 'e', ' ', 'P', 'i', 'c', 'o',
};
const uint8_t adv_data_len = sizeof(adv_data);

const uint8_t scan_response_data[] = {
    17, BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS,
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e,
};
const uint8_t scan_response_data_len = sizeof(scan_response_data);

struct BuddyState {
  hci_con_handle_t connection_handle = HCI_CON_HANDLE_INVALID;
  bd_addr_t peer_addr{};
  bd_addr_type_t peer_addr_type = BD_ADDR_TYPE_LE_PUBLIC;

  bool advertising = true;
  bool link_connected = false;
  bool secure = false;
  bool paired = false;
  bool nus_linked = false;
  bool prompt_active = false;
  bool response_sent = false;
  bool prompt_details = false;
  bool transcript_enabled = true;
  bool led_enabled = true;
  bool screen_off = false;
  bool napping = false;
  bool battery_low = false;
  bool usb_present = false;
  bool recently_completed = false;

  bool last_a = false;
  bool last_b = false;
  bool last_x = false;
  bool last_y = false;
  bool a_hold_fired = false;
  bool x_hold_fired = false;
  bool y_hold_fired = false;
  bool demo_mode = false;
  bool clock_enabled = true;
  bool rtc_valid = false;
  bool xfer_active = false;
  bool xfer_file_open = false;
  bool xfer_capture_manifest = false;

  DisplayMode display_mode = DisplayMode::Normal;
  PersonaState base_state = P_SLEEP;
  PersonaState active_state = P_SLEEP;

  uint8_t security_level = 0;
  uint8_t brightness_level = 4;
  uint8_t battery_percent = 0;
  uint8_t info_page = 0;
  uint8_t pet_page = 0;
  uint8_t msg_scroll = 0;
  uint8_t line_count = 0;
  uint8_t vel_idx = 0;
  uint8_t vel_count = 0;
  uint8_t energy_at_nap = 3;
  uint8_t reset_confirm_idx = 0xff;

  uint16_t battery_mv = 0;
  uint16_t velocity[8]{};
  uint16_t line_gen = 0;
  uint16_t last_line_gen = 0;

  uint32_t reconnects = 0;
  uint32_t approvals = 0;
  uint32_t denials = 0;
  uint32_t nap_seconds = 0;
  uint32_t total = 0;
  uint32_t running = 0;
  uint32_t waiting = 0;
  uint32_t tokens_total = 0;
  uint32_t tokens_today = 0;
  uint32_t level = 0;
  uint32_t prompt_arrived_ms = 0;
  uint32_t one_shot_until_ms = 0;
  uint32_t reset_confirm_until = 0;
  uint32_t last_bridge_tokens = 0;
  uint32_t last_nap_end_ms = 0;
  uint32_t nap_start_ms = 0;
  uint32_t rtc_local_epoch = 0;
  uint32_t rtc_synced_ms = 0;
  uint32_t xfer_expected = 0;
  uint32_t xfer_written = 0;
  uint32_t xfer_total = 0;
  uint32_t xfer_total_written = 0;
  uint32_t persist_due_ms = 0;

  absolute_time_t connected_since = nil_time;
  absolute_time_t last_heartbeat = nil_time;
  absolute_time_t last_input = nil_time;
  absolute_time_t last_battery_sample = nil_time;
  absolute_time_t a_down_since = nil_time;
  absolute_time_t x_down_since = nil_time;
  absolute_time_t y_down_since = nil_time;

  char status_line[64] = "Advertising";
  char banner[96] = "Waiting for Claude Desktop";
  char msg[96] = "No Claude connected";
  char last_rx[256] = "No RX yet";
  char last_ack[96] = "No acks yet";
  char device_name[32] = "Claude Pico";
  char owner[32] = "";
  char prompt_id[64] = "";
  char prompt_tool[32] = "";
  char prompt_hint[96] = "";
  char passkey[16] = "123456";
  char tx_line[768] = "";
  char xfer_name[24] = "";
  char xfer_file_path[48] = "";
  char lines[8][96]{};
  char manifest_buffer[4096]{};

  bool tx_pending = false;
  bool menu_open = false;
  bool settings_open = false;
  bool reset_open = false;
  bool tokens_synced = false;
  bool persist_dirty = false;
  size_t manifest_len = 0;

  uint8_t menu_sel = 0;
  uint8_t settings_sel = 0;
  uint8_t reset_sel = 0;

  btstack_context_callback_registration_t send_request{};
  btstack_packet_callback_registration_t hci_event_callback_registration{};
  btstack_packet_callback_registration_t sm_event_callback_registration{};
  btstack_timer_source ui_timer{};
} state;

PackDefinition installed_pack{};

uint32_t now_ms() {
  return to_ms_since_boot(get_absolute_time());
}

void copy_string(char* dest, const size_t dest_size, const char* src) {
  if(dest_size == 0) {
    return;
  }
  if(src == nullptr) {
    dest[0] = '\0';
    return;
  }
  std::snprintf(dest, dest_size, "%s", src);
}

void safe_name_copy(char* dest, const size_t dest_size, const char* src) {
  if(dest_size == 0) {
    return;
  }
  size_t j = 0;
  if(src != nullptr) {
    for(size_t i = 0; src[i] != '\0' && j < dest_size - 1; ++i) {
      const char c = src[i];
      if(c != '"' && c != '\\' && static_cast<unsigned char>(c) >= 0x20) {
        dest[j++] = c;
      }
    }
  }
  dest[j] = '\0';
}

void copy_summary(char* dest, const size_t dest_size, const char* src) {
  if(dest_size == 0) {
    return;
  }
  if(src == nullptr) {
    dest[0] = '\0';
    return;
  }
  std::snprintf(dest, dest_size, "%.250s", src);
}

uint32_t fnv1a32(const void* data, const size_t size) {
  const auto* bytes = static_cast<const uint8_t*>(data);
  uint32_t hash = 2166136261u;
  for(size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 16777619u;
  }
  return hash;
}

uint16_t parse_hex_color(const char* text, const uint16_t fallback) {
  if(text == nullptr || text[0] == '\0') {
    return fallback;
  }
  const char* cursor = text[0] == '#' ? text + 1 : text;
  char* end = nullptr;
  const uint32_t value = std::strtoul(cursor, &end, 16);
  if(end == cursor) {
    return fallback;
  }
  return static_cast<uint16_t>((((value >> 19) & 0x1f) << 11) | (((value >> 10) & 0x3f) << 5) | ((value >> 3) & 0x1f));
}

const char* skip_ws(const char* cursor) {
  while(cursor != nullptr && (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' || *cursor == '\t')) {
    ++cursor;
  }
  return cursor;
}

const char* find_key(const char* scope, const char* key) {
  if(scope == nullptr || key == nullptr) {
    return nullptr;
  }
  char needle[48];
  std::snprintf(needle, sizeof(needle), "\"%s\"", key);
  const char* found = std::strstr(scope, needle);
  if(found == nullptr) {
    return nullptr;
  }
  const char* colon = std::strchr(found + std::strlen(needle), ':');
  if(colon == nullptr) {
    return nullptr;
  }
  return skip_ws(colon + 1);
}

const char* find_matching(const char* start, const char open_char, const char close_char) {
  if(start == nullptr || *start != open_char) {
    return nullptr;
  }
  int depth = 0;
  bool in_string = false;
  bool escaped = false;
  for(const char* cursor = start; *cursor != '\0'; ++cursor) {
    const char ch = *cursor;
    if(in_string) {
      if(escaped) {
        escaped = false;
      } else if(ch == '\\') {
        escaped = true;
      } else if(ch == '"') {
        in_string = false;
      }
      continue;
    }
    if(ch == '"') {
      in_string = true;
      continue;
    }
    if(ch == open_char) {
      ++depth;
    } else if(ch == close_char) {
      --depth;
      if(depth == 0) {
        return cursor;
      }
    }
  }
  return nullptr;
}

bool extract_string_value(const char* scope, const char* key, char* out, const size_t out_size) {
  const char* value = find_key(scope, key);
  if(value == nullptr || *value != '"' || out == nullptr || out_size == 0) {
    return false;
  }
  ++value;
  size_t length = 0;
  bool escaped = false;
  while(*value != '\0' && length + 1 < out_size) {
    if(escaped) {
      out[length++] = *value;
      escaped = false;
    } else if(*value == '\\') {
      escaped = true;
    } else if(*value == '"') {
      break;
    } else {
      out[length++] = *value;
    }
    ++value;
  }
  out[length] = '\0';
  return true;
}

int extract_int_value(const char* scope, const char* key, const int fallback) {
  const char* value = find_key(scope, key);
  if(value == nullptr) {
    return fallback;
  }
  return std::atoi(value);
}

bool parse_text_state(const char* manifest, const char* state_key, PackState* out_state) {
  if(manifest == nullptr || state_key == nullptr || out_state == nullptr) {
    return false;
  }

  const char* states_value = find_key(manifest, "states");
  if(states_value == nullptr || *states_value != '{') {
    return false;
  }

  const char* state_value = find_key(states_value, state_key);
  if(state_value == nullptr || *state_value != '{') {
    return false;
  }

  const char* state_end = find_matching(state_value, '{', '}');
  if(state_end == nullptr) {
    return false;
  }

  PackState parsed{};
  parsed.delayMs = static_cast<uint16_t>(std::max(80, extract_int_value(state_value, "delay", 200)));

  const char* frames_value = find_key(state_value, "frames");
  if(frames_value == nullptr || *frames_value != '[') {
    return false;
  }
  const char* frames_end = find_matching(frames_value, '[', ']');
  if(frames_end == nullptr || frames_end > state_end) {
    return false;
  }

  const char* cursor = frames_value + 1;
  while(cursor < frames_end && parsed.nFrames < 8) {
    cursor = std::strchr(cursor, '"');
    if(cursor == nullptr || cursor >= frames_end) {
      break;
    }
    ++cursor;
    size_t length = 0;
    while(cursor < frames_end && *cursor != '\0' && *cursor != '"' && length + 1 < sizeof(parsed.frames[0])) {
      parsed.frames[parsed.nFrames][length++] = *cursor++;
    }
    parsed.frames[parsed.nFrames][length] = '\0';
    if(cursor < frames_end && *cursor == '"') {
      ++cursor;
    }
    parsed.nFrames += 1;
  }

  if(parsed.nFrames == 0) {
    return false;
  }

  *out_state = parsed;
  return true;
}

bool parse_text_pack_manifest(const char* manifest, PackDefinition* out_pack) {
  if(manifest == nullptr || out_pack == nullptr) {
    return false;
  }

  PackDefinition pack{};
  pack.bodyColor = 0x6B4D;
  pack.bgColor = C_BG;
  pack.textColor = C_WHITE;
  pack.textDimColor = C_DIM;
  pack.inkColor = C_BG;

  char mode[16] = "";
  if(extract_string_value(manifest, "mode", mode, sizeof(mode)) && std::strcmp(mode, "text") != 0) {
    return false;
  }

  if(!extract_string_value(manifest, "name", pack.name, sizeof(pack.name)) || pack.name[0] == '\0') {
    copy_string(pack.name, sizeof(pack.name), "custom");
  }

  const char* colors = find_key(manifest, "colors");
  if(colors != nullptr && *colors == '{') {
    char color[16] = "";
    if(extract_string_value(colors, "body", color, sizeof(color))) pack.bodyColor = parse_hex_color(color, pack.bodyColor);
    if(extract_string_value(colors, "bg", color, sizeof(color))) pack.bgColor = parse_hex_color(color, pack.bgColor);
    if(extract_string_value(colors, "text", color, sizeof(color))) pack.textColor = parse_hex_color(color, pack.textColor);
    if(extract_string_value(colors, "textDim", color, sizeof(color))) pack.textDimColor = parse_hex_color(color, pack.textDimColor);
    if(extract_string_value(colors, "ink", color, sizeof(color))) pack.inkColor = parse_hex_color(color, pack.inkColor);
  }

  bool any_frames = false;
  for(uint8_t i = 0; i < 7; ++i) {
    if(parse_text_state(manifest, state_names[i], &pack.states[i])) {
      any_frames = true;
    }
  }

  if(!any_frames) {
    return false;
  }

  pack.valid = true;
  *out_pack = pack;
  return true;
}

void set_pen565(const uint16_t color) {
  const uint8_t r5 = static_cast<uint8_t>((color >> 11) & 0x1f);
  const uint8_t g6 = static_cast<uint8_t>((color >> 5) & 0x3f);
  const uint8_t b5 = static_cast<uint8_t>(color & 0x1f);
  const uint8_t r = static_cast<uint8_t>((r5 * 255) / 31);
  const uint8_t g = static_cast<uint8_t>((g6 * 255) / 63);
  const uint8_t b = static_cast<uint8_t>((b5 * 255) / 31);
  graphics.set_pen(r, g, b);
}

int text_width(const char* text, const float scale = 1.0f, const bool fixed = true) {
  graphics.set_font("bitmap6");
  return graphics.measure_text(text == nullptr ? "" : text, scale, 0, fixed);
}

void draw_text(const char* text, const int x, const int y, const float scale = 1.0f, const uint16_t color = C_TEXT,
               const bool fixed = true) {
  if(text == nullptr || text[0] == '\0') {
    return;
  }
  set_pen565(color);
  graphics.set_font("bitmap6");
  graphics.text(text, Point(x, y), 4096, scale, 0.0f, 0, fixed);
}

void draw_centered(const char* text, const int center_x, const int y, const float scale = 1.0f, const uint16_t color = C_TEXT) {
  draw_text(text, center_x - (text_width(text, scale) / 2), y, scale, color);
}

void fit_text(char* out, const size_t out_size, const char* text, const int max_width, const float scale = 1.0f) {
  if(out == nullptr || out_size == 0) {
    return;
  }
  out[0] = '\0';
  if(text == nullptr) {
    return;
  }
  copy_string(out, out_size, text);
  if(text_width(out, scale) <= max_width) {
    return;
  }

  const char* ellipsis = "...";
  const int ellipsis_width = text_width(ellipsis, scale);
  if(ellipsis_width > max_width) {
    return;
  }

  size_t length = std::strlen(out);
  while(length > 0) {
    out[length - 1] = '\0';
    --length;

    char candidate[96];
    std::snprintf(candidate, sizeof(candidate), "%s%s", out, ellipsis);
    if(text_width(candidate, scale) <= max_width) {
      copy_string(out, out_size, candidate);
      return;
    }
  }

  copy_string(out, out_size, ellipsis);
}

void draw_centered_fit(const char* text, const int center_x, const int y, const int max_width, const float scale = 1.0f,
                       const uint16_t color = C_TEXT) {
  char fitted[96];
  fit_text(fitted, sizeof(fitted), text, max_width, scale);
  draw_centered(fitted, center_x, y, scale, color);
}

bool contains_token(const char* haystack, const char* needle) {
  return haystack != nullptr && needle != nullptr && std::strstr(haystack, needle) != nullptr;
}

int extract_int(const char* line, const char* key, const int fallback) {
  char needle[48];
  std::snprintf(needle, sizeof(needle), "\"%s\":", key);
  const char* start = std::strstr(line, needle);
  if(start == nullptr) {
    return fallback;
  }
  start += std::strlen(needle);
  while(*start == ' ') {
    ++start;
  }
  return std::atoi(start);
}

bool extract_bool(const char* line, const char* key, const bool fallback) {
  char needle[48];
  std::snprintf(needle, sizeof(needle), "\"%s\":", key);
  const char* start = std::strstr(line, needle);
  if(start == nullptr) {
    return fallback;
  }
  start += std::strlen(needle);
  while(*start == ' ') {
    ++start;
  }
  if(std::strncmp(start, "true", 4) == 0) {
    return true;
  }
  if(std::strncmp(start, "false", 5) == 0) {
    return false;
  }
  return fallback;
}

void extract_string_after(const char* line, const char* anchor, const char* key, char* out, const size_t out_size) {
  const char* scope = line;
  if(anchor != nullptr) {
    scope = std::strstr(line, anchor);
    if(scope == nullptr) {
      return;
    }
  }

  char needle[48];
  std::snprintf(needle, sizeof(needle), "\"%s\":", key);
  const char* start = std::strstr(scope, needle);
  if(start == nullptr) {
    return;
  }

  start += std::strlen(needle);
  while(*start == ' ') {
    ++start;
  }
  if(*start != '"') {
    return;
  }
  ++start;

  size_t len = 0;
  while(start[len] != '\0' && start[len] != '"' && len < out_size - 1) {
    out[len] = start[len];
    ++len;
  }
  out[len] = '\0';
}

void extract_entries(const char* line) {
  const char* start = std::strstr(line, "\"entries\":[");
  if(start == nullptr) {
    return;
  }
  start = std::strchr(start, '[');
  if(start == nullptr) {
    return;
  }
  ++start;

  char next_lines[8][96]{};
  uint8_t next_count = 0;

  while(*start != '\0' && *start != ']' && next_count < 8) {
    while(*start != '\0' && *start != '"' && *start != ']') {
      ++start;
    }
    if(*start != '"') {
      break;
    }
    ++start;

    size_t len = 0;
    while(start[len] != '\0' && start[len] != '"' && len < sizeof(next_lines[0]) - 1) {
      next_lines[next_count][len] = start[len];
      ++len;
    }
    next_lines[next_count][len] = '\0';
    ++next_count;

    const char* end = std::strchr(start, '"');
    if(end == nullptr) {
      break;
    }
    start = end + 1;
  }

  bool changed = next_count != state.line_count;
  if(!changed) {
    for(uint8_t i = 0; i < next_count; ++i) {
      if(std::strcmp(next_lines[i], state.lines[i]) != 0) {
        changed = true;
        break;
      }
    }
  }

  for(uint8_t i = 0; i < next_count; ++i) {
    copy_string(state.lines[i], sizeof(state.lines[i]), next_lines[i]);
  }
  state.line_count = next_count;
  if(changed) {
    state.line_gen += 1;
    state.msg_scroll = 0;
  }
}

void set_status(const char* text) {
  copy_string(state.status_line, sizeof(state.status_line), text);
}

void set_banner(const char* text) {
  copy_string(state.banner, sizeof(state.banner), text);
}

void set_ack(const char* text) {
  copy_string(state.last_ack, sizeof(state.last_ack), text);
}

bool desktop_active() {
  if(is_nil_time(state.last_heartbeat)) {
    return false;
  }
  return absolute_time_diff_us(state.last_heartbeat, get_absolute_time()) <=
         static_cast<int64_t>(kPromptKeepAliveMs) * 1000;
}

void clear_prompt() {
  state.prompt_active = false;
  state.response_sent = false;
  state.prompt_details = false;
  state.prompt_id[0] = '\0';
  state.prompt_tool[0] = '\0';
  state.prompt_hint[0] = '\0';
}

uint16_t read_vsys_mv() {
  uint32_t total = 0;
  for(uint8_t i = 0; i < 8; ++i) {
    adc_select_input(3);
    sleep_us(5);
    total += adc_read();
  }
  const float raw = static_cast<float>(total) / 8.0f;
  const float volts = (raw * kAdcReferenceVolts * kVoltageDividerScale) / 4095.0f;
  return static_cast<uint16_t>(volts * 1000.0f);
}

uint8_t estimate_percent(const uint16_t millivolts) {
  if(millivolts >= 4200) return 100;
  if(millivolts <= 3300) return 0;
  if(millivolts >= 4000) return static_cast<uint8_t>(80 + ((millivolts - 4000) * 20) / 200);
  if(millivolts >= 3700) return static_cast<uint8_t>(20 + ((millivolts - 3700) * 60) / 300);
  return static_cast<uint8_t>((millivolts - 3300) * 20 / 400);
}

void update_battery_state() {
  const absolute_time_t now = get_absolute_time();
  if(!is_nil_time(state.last_battery_sample) &&
     absolute_time_diff_us(state.last_battery_sample, now) < static_cast<int64_t>(kBatterySampleMs) * 1000) {
    return;
  }

  state.usb_present = cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN) != 0;
  uint16_t millivolts = read_vsys_mv();
  if(state.usb_present && millivolts < 1000) {
    millivolts = state.battery_mv >= 1000 ? state.battery_mv : 5000;
  }

  state.battery_mv = millivolts;
  state.battery_percent = state.usb_present && millivolts > 4500 ? 100 : estimate_percent(millivolts);
  state.battery_low = !state.usb_present && millivolts > 0 && millivolts < 3500;
  state.last_battery_sample = now;
}

void persist_state();
void schedule_persist(uint32_t delay_ms = kPersistDebounceMs);
void flush_scheduled_persist();
void clear_stats();

void trigger_one_shot(const PersonaState persona_state, const uint32_t duration_ms) {
  state.active_state = persona_state;
  state.one_shot_until_ms = now_ms() + duration_ms;
}

void stats_on_bridge_tokens(const uint32_t bridge_total) {
  if(!state.tokens_synced) {
    state.last_bridge_tokens = bridge_total;
    state.tokens_synced = true;
    return;
  }
  if(bridge_total < state.last_bridge_tokens) {
    state.last_bridge_tokens = bridge_total;
    return;
  }

  const uint32_t delta = bridge_total - state.last_bridge_tokens;
  state.last_bridge_tokens = bridge_total;
  if(delta == 0) {
    return;
  }

  const uint32_t level_before = state.level;
  state.tokens_total += delta;
  state.level = state.tokens_total / kTokensPerLevel;
  if(state.level > level_before) {
    trigger_one_shot(P_CELEBRATE, 3000);
    schedule_persist();
  }
}

void stats_on_approval(const uint32_t seconds_to_respond) {
  state.approvals += 1;
  state.velocity[state.vel_idx] = static_cast<uint16_t>(std::min<uint32_t>(seconds_to_respond, 65535u));
  state.vel_idx = (state.vel_idx + 1) % 8;
  if(state.vel_count < 8) {
    state.vel_count += 1;
  }
  schedule_persist();
}

void stats_on_denial() {
  state.denials += 1;
  schedule_persist();
}

uint16_t median_velocity() {
  if(state.vel_count == 0) {
    return 0;
  }
  uint16_t values[8];
  std::memcpy(values, state.velocity, sizeof(values));
  for(uint8_t i = 1; i < state.vel_count; ++i) {
    const uint16_t key = values[i];
    int8_t j = static_cast<int8_t>(i) - 1;
    while(j >= 0 && values[j] > key) {
      values[j + 1] = values[j];
      --j;
    }
    values[j + 1] = key;
  }
  return values[state.vel_count / 2];
}

uint8_t mood_tier() {
  const uint16_t velocity = median_velocity();
  int8_t tier = 2;
  if(velocity != 0) {
    if(velocity < 15) tier = 4;
    else if(velocity < 30) tier = 3;
    else if(velocity < 60) tier = 2;
    else if(velocity < 120) tier = 1;
    else tier = 0;
  }

  if(state.approvals + state.denials >= 3) {
    if(state.denials > state.approvals) tier -= 2;
    else if(state.denials * 2 > state.approvals) tier -= 1;
  }
  if(tier < 0) tier = 0;
  return static_cast<uint8_t>(tier);
}

uint8_t fed_progress() {
  return static_cast<uint8_t>((state.tokens_total % kTokensPerLevel) / (kTokensPerLevel / 10));
}

uint8_t energy_tier() {
  const uint32_t hours_since = (now_ms() - state.last_nap_end_ms) / 3600000;
  int8_t energy = static_cast<int8_t>(state.energy_at_nap) - static_cast<int8_t>(hours_since / 2);
  if(energy < 0) energy = 0;
  if(energy > 5) energy = 5;
  return static_cast<uint8_t>(energy);
}

void update_snapshot_banner() {
  if(state.xfer_active) {
    set_banner("Installing character pack");
    return;
  }
  if(state.demo_mode) {
    set_banner("Demo mode");
    return;
  }
  if(state.prompt_active && !state.response_sent) {
    set_banner("Approval pending");
    return;
  }
  if(!desktop_active()) {
    set_banner(state.link_connected ? "Connected, waiting for Claude" : "Waiting for Claude Desktop");
    return;
  }
  if(state.waiting > 0) {
    set_banner("Claude needs attention");
    return;
  }
  if(state.running > 0) {
    set_banner("Claude is working");
    return;
  }
  set_banner(state.secure ? "Buddy connected" : "Connected, pairing in progress");
}

PersonaState derive_state() {
  if(state.screen_off || state.napping) {
    return P_SLEEP;
  }
  if(!desktop_active()) {
    return P_SLEEP;
  }
  if(state.waiting > 0 && !state.response_sent) {
    return P_ATTENTION;
  }
  if(state.recently_completed) {
    return P_CELEBRATE;
  }
  if(state.running > 0) {
    return P_BUSY;
  }
  return P_IDLE;
}

void request_can_send();

void queue_tx_line(const char* line_without_newline) {
  if(state.connection_handle == HCI_CON_HANDLE_INVALID || line_without_newline == nullptr) {
    return;
  }
  std::snprintf(state.tx_line, sizeof(state.tx_line), "%s\n", line_without_newline);
  state.tx_pending = true;
  request_can_send();
}

void request_can_send() {
  if(!state.tx_pending || state.connection_handle == HCI_CON_HANDLE_INVALID) {
    return;
  }
  state.send_request.callback = [](void* context) {
    UNUSED(context);
    if(!state.tx_pending || state.connection_handle == HCI_CON_HANDLE_INVALID) {
      return;
    }
    nordic_spp_service_server_send(state.connection_handle,
                                   reinterpret_cast<const uint8_t*>(state.tx_line),
                                   static_cast<uint16_t>(std::strlen(state.tx_line)));
    state.tx_pending = false;
  };
  nordic_spp_service_server_request_can_send_now(&state.send_request, state.connection_handle);
}

void send_status_reply() {
  char line[640];
  char owner_fragment[64] = "";
  if(state.owner[0] != '\0') {
    std::snprintf(owner_fragment, sizeof(owner_fragment), ",\"owner\":\"%s\"", state.owner);
  }

  std::snprintf(
      line,
      sizeof(line),
      "{\"ack\":\"status\",\"ok\":true,\"data\":{\"name\":\"%s\"%s,\"sec\":%s,\"bat\":{\"pct\":%u,\"mV\":%u,\"mA\":0,\"usb\":%s},\"sys\":{\"up\":%lu,\"heap\":0},\"stats\":{\"appr\":%lu,\"deny\":%lu,\"vel\":%u,\"nap\":%lu,\"lvl\":%lu}}}",
      state.device_name,
      owner_fragment,
      state.secure ? "true" : "false",
      static_cast<unsigned int>(state.battery_percent),
      static_cast<unsigned int>(state.battery_mv),
      state.usb_present ? "true" : "false",
      static_cast<unsigned long>(now_ms() / 1000),
      static_cast<unsigned long>(state.approvals),
      static_cast<unsigned long>(state.denials),
      static_cast<unsigned int>(median_velocity()),
      static_cast<unsigned long>(state.nap_seconds),
      static_cast<unsigned long>(state.level));

  queue_tx_line(line);
  set_ack("Acked status");
}

void ack_command(const char* command) {
  char line[128];
  std::snprintf(line, sizeof(line), "{\"ack\":\"%s\",\"ok\":true,\"n\":0}", command);
  queue_tx_line(line);

  char ack_line[96];
  std::snprintf(ack_line, sizeof(ack_line), "Acked %s", command);
  set_ack(ack_line);
}

void ack_command_result(const char* command, const bool ok, const uint32_t n = 0, const char* error = nullptr) {
  char line[192];
  if(error != nullptr && error[0] != '\0') {
    std::snprintf(line, sizeof(line), "{\"ack\":\"%s\",\"ok\":%s,\"n\":%lu,\"error\":\"%s\"}",
                  command, ok ? "true" : "false", static_cast<unsigned long>(n), error);
  } else {
    std::snprintf(line, sizeof(line), "{\"ack\":\"%s\",\"ok\":%s,\"n\":%lu}",
                  command, ok ? "true" : "false", static_cast<unsigned long>(n));
  }
  queue_tx_line(line);

  char ack_line[96];
  std::snprintf(ack_line, sizeof(ack_line), "%s %s", ok ? "Acked" : "Failed", command);
  set_ack(ack_line);
}

void clear_transfer_state() {
  state.xfer_active = false;
  state.xfer_file_open = false;
  state.xfer_capture_manifest = false;
  state.xfer_expected = 0;
  state.xfer_written = 0;
  state.xfer_total = 0;
  state.xfer_total_written = 0;
  state.manifest_len = 0;
  state.xfer_name[0] = '\0';
  state.xfer_file_path[0] = '\0';
  state.manifest_buffer[0] = '\0';
}

void delete_custom_pack() {
  installed_pack = {};
  buddyClearCustomSpecies();
  persist_state();
}

void factory_reset_state() {
  state.brightness_level = 4;
  state.led_enabled = true;
  state.transcript_enabled = true;
  state.demo_mode = false;
  state.clock_enabled = true;
  clear_stats();
  state.owner[0] = '\0';
  copy_string(state.device_name, sizeof(state.device_name), kDefaultDeviceName);
  delete_custom_pack();
  buddySetSpeciesIdx(0);
  persist_state();
}

void clear_stats() {
  state.approvals = 0;
  state.denials = 0;
  state.nap_seconds = 0;
  state.tokens_total = 0;
  state.level = 0;
  state.last_bridge_tokens = state.tokens_today;
  state.tokens_synced = false;
  std::memset(state.velocity, 0, sizeof(state.velocity));
  state.vel_idx = 0;
  state.vel_count = 0;
}

PersistentBlob build_persistent_blob() {
  PersistentBlob blob{};
  blob.magic = kStorageMagic;
  blob.version = kStorageVersion;
  blob.size = sizeof(PersistentBlob);
  blob.brightness_level = state.brightness_level;
  blob.led_enabled = state.led_enabled ? 1 : 0;
  blob.transcript_enabled = state.transcript_enabled ? 1 : 0;
  blob.demo_mode = state.demo_mode ? 1 : 0;
  blob.clock_enabled = state.clock_enabled ? 1 : 0;
  blob.ascii_species_idx = buddyCustomSpeciesSelected() ? 0 : buddySpeciesIdx();
  blob.custom_selected = buddyCustomSpeciesSelected() ? 1 : 0;
  copy_string(blob.device_name, sizeof(blob.device_name), state.device_name);
  copy_string(blob.owner, sizeof(blob.owner), state.owner);
  blob.approvals = state.approvals;
  blob.denials = state.denials;
  blob.nap_seconds = state.nap_seconds;
  blob.tokens_total = state.tokens_total;
  blob.level = state.level;
  std::memcpy(blob.velocity, state.velocity, sizeof(blob.velocity));
  blob.vel_idx = state.vel_idx;
  blob.vel_count = state.vel_count;
  blob.custom_valid = installed_pack.valid ? 1 : 0;
  blob.custom_pack = installed_pack;
  blob.crc = fnv1a32(&blob, sizeof(blob) - sizeof(blob.crc));
  return blob;
}

struct FlashCommit {
  uint8_t bytes[FLASH_SECTOR_SIZE];
};

void flash_commit_sector(void* context) {
  auto* commit = static_cast<FlashCommit*>(context);
  flash_range_erase(kStorageOffset, FLASH_SECTOR_SIZE);
  for(uint32_t offset = 0; offset < FLASH_SECTOR_SIZE; offset += FLASH_PAGE_SIZE) {
    flash_range_program(kStorageOffset + offset, commit->bytes + offset, FLASH_PAGE_SIZE);
  }
}

void persist_state() {
  state.persist_dirty = false;
  state.persist_due_ms = 0;
  FlashCommit commit{};
  std::memset(commit.bytes, 0xff, sizeof(commit.bytes));
  const PersistentBlob blob = build_persistent_blob();
  static_assert(sizeof(blob) < FLASH_SECTOR_SIZE, "Persistent blob must fit in one flash sector");
  std::memcpy(commit.bytes, &blob, sizeof(blob));
  flash_safe_execute(flash_commit_sector, &commit, UINT32_MAX);
}

void schedule_persist(const uint32_t delay_ms) {
  state.persist_dirty = true;
  state.persist_due_ms = now_ms() + delay_ms;
}

void flush_scheduled_persist() {
  if(!state.persist_dirty) {
    return;
  }
  if(static_cast<int32_t>(now_ms() - state.persist_due_ms) < 0) {
    return;
  }
  persist_state();
}

void apply_persistent_blob(const PersistentBlob& blob) {
  state.brightness_level = std::min<uint8_t>(blob.brightness_level, 4);
  state.led_enabled = blob.led_enabled != 0;
  state.transcript_enabled = blob.transcript_enabled != 0;
  state.demo_mode = blob.demo_mode != 0;
  state.clock_enabled = blob.clock_enabled != 0;
  copy_string(state.device_name, sizeof(state.device_name), blob.device_name);
  copy_string(state.owner, sizeof(state.owner), blob.owner);
  state.approvals = blob.approvals;
  state.denials = blob.denials;
  state.nap_seconds = blob.nap_seconds;
  state.tokens_total = blob.tokens_total;
  state.level = blob.level;
  std::memcpy(state.velocity, blob.velocity, sizeof(state.velocity));
  state.vel_idx = blob.vel_idx;
  state.vel_count = blob.vel_count;

  installed_pack = blob.custom_valid ? blob.custom_pack : PackDefinition{};
  if(installed_pack.valid) {
    buddySetCustomSpecies(&installed_pack);
    if(!blob.custom_selected) {
      buddySetSpeciesIdx(std::min<uint8_t>(blob.ascii_species_idx, buddySpeciesCount() - 1));
    }
  } else {
    buddyClearCustomSpecies();
    buddySetSpeciesIdx(std::min<uint8_t>(blob.ascii_species_idx, buddySpeciesCount() - 1));
  }
}

bool load_persistent_state() {
  const auto* blob = reinterpret_cast<const PersistentBlob*>(XIP_BASE + kStorageOffset);
  if(blob->magic != kStorageMagic || blob->version != kStorageVersion || blob->size != sizeof(PersistentBlob)) {
    return false;
  }
  const uint32_t crc = fnv1a32(blob, sizeof(PersistentBlob) - sizeof(blob->crc));
  if(crc != blob->crc) {
    return false;
  }
  apply_persistent_blob(*blob);
  return true;
}

bool extract_time_sync(const char* line, uint32_t* local_epoch_out) {
  if(line == nullptr || local_epoch_out == nullptr) {
    return false;
  }
  const char* start = std::strstr(line, "\"time\":[");
  if(start == nullptr) {
    return false;
  }
  start = std::strchr(start, '[');
  if(start == nullptr) {
    return false;
  }
  ++start;
  const uint32_t epoch = static_cast<uint32_t>(std::strtoul(start, nullptr, 10));
  const char* comma = std::strchr(start, ',');
  if(comma == nullptr) {
    return false;
  }
  const int32_t offset = static_cast<int32_t>(std::strtol(comma + 1, nullptr, 10));
  *local_epoch_out = epoch + offset;
  return true;
}

uint32_t current_local_epoch() {
  if(!state.rtc_valid) {
    return 0;
  }
  return state.rtc_local_epoch + ((now_ms() - state.rtc_synced_ms) / 1000);
}

bool should_show_clock() {
  return state.display_mode == DisplayMode::Normal && state.clock_enabled && state.usb_present && state.rtc_valid &&
         !state.prompt_active && !state.menu_open && !state.settings_open && !state.reset_open && !state.demo_mode &&
         state.running == 0 && state.waiting == 0;
}

void apply_demo_snapshot() {
  if(!state.demo_mode) {
    return;
  }
  const size_t scenario_count = sizeof(demo_scenarios) / sizeof(demo_scenarios[0]);
  const uint8_t idx = static_cast<uint8_t>((now_ms() / kDemoStepMs) % scenario_count);
  const DemoScenario& scenario = demo_scenarios[idx];
  state.total = scenario.total;
  state.running = scenario.running;
  state.waiting = scenario.waiting;
  state.recently_completed = scenario.completed;
  state.tokens_today = scenario.tokens_today;
  copy_string(state.msg, sizeof(state.msg), scenario.msg);
  state.line_count = scenario.entry_count;
  for(uint8_t i = 0; i < scenario.entry_count; ++i) {
    copy_string(state.lines[i], sizeof(state.lines[i]), scenario.entries[i]);
  }
  state.prompt_active = false;
  state.response_sent = false;
  copy_string(state.banner, sizeof(state.banner), "Demo mode");
}

void handle_protocol_line(const char* line) {
  copy_summary(state.last_rx, sizeof(state.last_rx), line);

  uint32_t local_epoch = 0;
  if(extract_time_sync(line, &local_epoch)) {
    state.rtc_local_epoch = local_epoch;
    state.rtc_synced_ms = now_ms();
    state.rtc_valid = true;
  }

  if(contains_token(line, "\"cmd\":\"status\"")) {
    send_status_reply();
    return;
  }

  if(contains_token(line, "\"cmd\":\"name\"")) {
    char updated[32] = "";
    extract_string_after(line, nullptr, "name", updated, sizeof(updated));
    if(updated[0] != '\0') {
      safe_name_copy(state.device_name, sizeof(state.device_name), updated);
      persist_state();
    }
    ack_command("name");
    return;
  }

  if(contains_token(line, "\"cmd\":\"owner\"")) {
    char updated[32] = "";
    extract_string_after(line, nullptr, "name", updated, sizeof(updated));
    if(updated[0] != '\0') {
      safe_name_copy(state.owner, sizeof(state.owner), updated);
      persist_state();
    }
    ack_command("owner");
    return;
  }

  if(contains_token(line, "\"cmd\":\"species\"")) {
    const int idx = extract_int(line, "idx", buddyCustomSpeciesSelected() ? kCustomSpeciesSentinel : buddySpeciesIdx());
    if(idx == kCustomSpeciesSentinel && installed_pack.valid) {
      buddySetSpeciesIdx(kCustomSpeciesSentinel);
      persist_state();
    } else if(idx >= 0 && idx < buddySpeciesCount()) {
      buddySetSpeciesIdx(static_cast<uint8_t>(idx));
      persist_state();
    }
    ack_command("species");
    return;
  }

  if(contains_token(line, "\"cmd\":\"unpair\"")) {
    if(state.connection_handle != HCI_CON_HANDLE_INVALID) {
      gap_delete_bonding(state.peer_addr_type, state.peer_addr);
    }
    state.paired = false;
    state.secure = false;
    state.security_level = 0;
    ack_command("unpair");
    return;
  }

  if(contains_token(line, "\"cmd\":\"char_begin\"")) {
    clear_transfer_state();
    state.xfer_active = true;
    state.xfer_total = static_cast<uint32_t>(extract_int(line, "total", 0));
    extract_string_after(line, nullptr, "name", state.xfer_name, sizeof(state.xfer_name));
    if(state.xfer_name[0] == '\0') {
      copy_string(state.xfer_name, sizeof(state.xfer_name), "custom");
    }
    set_banner("Receiving character pack");
    ack_command_result("char_begin", true);
    return;
  }

  if(contains_token(line, "\"cmd\":\"file\"")) {
    if(!state.xfer_active) {
      ack_command_result("file", false);
      return;
    }
    state.xfer_file_open = true;
    state.xfer_capture_manifest = false;
    state.xfer_expected = static_cast<uint32_t>(extract_int(line, "size", 0));
    state.xfer_written = 0;
    extract_string_after(line, nullptr, "path", state.xfer_file_path, sizeof(state.xfer_file_path));
    state.xfer_capture_manifest = std::strcmp(state.xfer_file_path, "manifest.json") == 0;
    if(state.xfer_capture_manifest) {
      state.manifest_len = 0;
      state.manifest_buffer[0] = '\0';
    }
    ack_command_result("file", state.xfer_file_path[0] != '\0');
    return;
  }

  if(contains_token(line, "\"cmd\":\"chunk\"")) {
    if(!state.xfer_active || !state.xfer_file_open) {
      ack_command_result("chunk", false);
      return;
    }

    char encoded[520] = "";
    extract_string_after(line, nullptr, "d", encoded, sizeof(encoded));
    if(encoded[0] == '\0') {
      ack_command_result("chunk", false, state.xfer_written);
      return;
    }

    uint8_t decoded[384];
    const int written = btstack_base64_decoder_process_block(reinterpret_cast<const uint8_t*>(encoded),
                                                             static_cast<uint32_t>(std::strlen(encoded)),
                                                             decoded,
                                                             sizeof(decoded));
    if(written < 0) {
      ack_command_result("chunk", false, state.xfer_written);
      return;
    }

    if(state.xfer_capture_manifest) {
      if(state.manifest_len + static_cast<size_t>(written) >= sizeof(state.manifest_buffer)) {
        ack_command_result("chunk", false, state.xfer_written, "manifest too large");
        return;
      }
      std::memcpy(state.manifest_buffer + state.manifest_len, decoded, static_cast<size_t>(written));
      state.manifest_len += static_cast<size_t>(written);
      state.manifest_buffer[state.manifest_len] = '\0';
    }

    state.xfer_written += static_cast<uint32_t>(written);
    state.xfer_total_written += static_cast<uint32_t>(written);
    ack_command_result("chunk", true, state.xfer_written);
    return;
  }

  if(contains_token(line, "\"cmd\":\"file_end\"")) {
    if(!state.xfer_active || !state.xfer_file_open) {
      ack_command_result("file_end", false);
      return;
    }
    const bool expected_ok = state.xfer_expected == 0 || state.xfer_written == state.xfer_expected;
    const bool manifest_ok = !state.xfer_capture_manifest || state.manifest_len > 0;
    const bool ok = expected_ok && manifest_ok;
    state.xfer_file_open = false;
    state.xfer_capture_manifest = false;
    state.xfer_expected = 0;
    state.xfer_written = 0;
    ack_command_result("file_end", ok, state.xfer_total_written);
    return;
  }

  if(contains_token(line, "\"cmd\":\"char_end\"")) {
    PackDefinition parsed_pack{};
    const bool ok = parse_text_pack_manifest(state.manifest_buffer, &parsed_pack);
    if(ok) {
      installed_pack = parsed_pack;
      buddySetCustomSpecies(&installed_pack);
      persist_state();
      state.display_mode = DisplayMode::Pet;
      set_banner("Character pack installed");
    }
    ack_command_result("char_end", ok, state.xfer_total_written, ok ? nullptr : "text manifest required");
    clear_transfer_state();
    return;
  }

  if(contains_token(line, "\"evt\":\"turn\"")) {
    set_banner("Turn event received");
    return;
  }

  if(!contains_token(line, "\"total\"")) {
    return;
  }

  if(state.demo_mode) {
    state.last_heartbeat = get_absolute_time();
    update_snapshot_banner();
    return;
  }

  state.total = static_cast<uint32_t>(extract_int(line, "total", state.total));
  state.running = static_cast<uint32_t>(extract_int(line, "running", state.running));
  state.waiting = static_cast<uint32_t>(extract_int(line, "waiting", state.waiting));
  state.tokens_today = static_cast<uint32_t>(extract_int(line, "tokens_today", state.tokens_today));
  state.recently_completed = extract_bool(line, "completed", false);

  const uint32_t bridge_tokens = static_cast<uint32_t>(extract_int(line, "tokens", state.last_bridge_tokens));
  stats_on_bridge_tokens(bridge_tokens);

  char previous_prompt[64];
  std::snprintf(previous_prompt, sizeof(previous_prompt), "%s", state.prompt_id);

  extract_string_after(line, nullptr, "msg", state.msg, sizeof(state.msg));
  extract_entries(line);

  if(contains_token(line, "\"prompt\"")) {
    state.prompt_active = true;
    extract_string_after(line, "\"prompt\"", "id", state.prompt_id, sizeof(state.prompt_id));
    extract_string_after(line, "\"prompt\"", "tool", state.prompt_tool, sizeof(state.prompt_tool));
    extract_string_after(line, "\"prompt\"", "hint", state.prompt_hint, sizeof(state.prompt_hint));
    if(std::strcmp(previous_prompt, state.prompt_id) != 0) {
      state.response_sent = false;
      state.prompt_arrived_ms = now_ms();
      state.prompt_details = false;
      state.display_mode = DisplayMode::Normal;
      trigger_one_shot(P_ATTENTION, 2000);
    }
  } else {
    clear_prompt();
  }

  state.last_heartbeat = get_absolute_time();
  update_snapshot_banner();
}

void handle_prompt_decision(const char* decision) {
  if(!state.prompt_active || state.prompt_id[0] == '\0' || state.response_sent) {
    return;
  }
  char line[192];
  std::snprintf(line, sizeof(line), "{\"cmd\":\"permission\",\"id\":\"%s\",\"decision\":\"%s\"}", state.prompt_id, decision);
  queue_tx_line(line);
  state.response_sent = true;

  if(std::strcmp(decision, "once") == 0) {
    const uint32_t took_seconds = (now_ms() - state.prompt_arrived_ms) / 1000;
    stats_on_approval(took_seconds);
    set_ack("Sent approval");
    if(took_seconds < 5) {
      trigger_one_shot(P_HEART, 2000);
    }
  } else {
    stats_on_denial();
    set_ack("Sent denial");
    trigger_one_shot(P_DIZZY, 1500);
  }
  update_snapshot_banner();
}

void toggle_nap() {
  if(!state.napping) {
    state.napping = true;
    state.nap_start_ms = now_ms();
    set_banner("Buddy is napping");
    return;
  }

  state.napping = false;
  if(state.nap_start_ms != 0) {
    state.nap_seconds += (now_ms() - state.nap_start_ms) / 1000;
  }
  state.last_nap_end_ms = now_ms();
  state.energy_at_nap = 5;
  state.last_input = get_absolute_time();
  update_snapshot_banner();
  schedule_persist();
}

uint8_t current_backlight() {
  if(state.screen_off) {
    return 0;
  }
  if(state.napping) {
    return 12;
  }
  const uint8_t base = kBrightnessLevels[state.brightness_level];
  if(state.prompt_active && !state.response_sent) {
    return base;
  }
  if(!is_nil_time(state.last_input) && now_ms() - to_ms_since_boot(state.last_input) > kDimAfterMs) {
    return std::max<uint8_t>(24, base / 3);
  }
  return base;
}

void draw_chip(const Rect rect, const uint16_t bg, const char* text, const uint16_t fg = C_TEXT) {
  set_pen565(bg);
  graphics.rectangle(rect);
  draw_centered(text, rect.x + (rect.w / 2), rect.y + 4, 1.0f, fg);
}

uint8_t wrap_into(const char* in, char out[][28], const uint8_t max_rows, const uint8_t width) {
  if(in == nullptr) {
    return 0;
  }

  uint8_t row = 0;
  uint8_t col = 0;
  const char* p = in;
  while(*p != '\0' && row < max_rows) {
    while(*p == ' ') p++;
    const char* word = p;
    while(*p != '\0' && *p != ' ') p++;
    uint8_t word_len = static_cast<uint8_t>(p - word);
    if(word_len == 0) {
      break;
    }

    const uint8_t needed = (col > 0 ? 1 : 0) + word_len;
    if(col + needed > width) {
      out[row][col] = '\0';
      if(++row >= max_rows) {
        return row;
      }
      out[row][0] = ' ';
      col = 1;
    }

    if(col > 1 || (col == 1 && out[row][0] != ' ')) {
      out[row][col++] = ' ';
    }

    while(word_len > width - col) {
      const uint8_t take = width - col;
      std::memcpy(&out[row][col], word, take);
      col += take;
      word += take;
      word_len -= take;
      out[row][col] = '\0';
      if(++row >= max_rows) {
        return row;
      }
      out[row][0] = ' ';
      col = 1;
    }

    std::memcpy(&out[row][col], word, word_len);
    col += word_len;
  }

  if(col > 0 && row < max_rows) {
    out[row][col] = '\0';
    row++;
  }
  return row;
}

uint8_t build_wrapped_transcript(char wrapped[][28], uint8_t source[], const uint8_t max_rows, const uint8_t width) {
  uint8_t wrapped_count = 0;
  for(uint8_t i = 0; i < state.line_count && wrapped_count < max_rows; ++i) {
    const uint8_t got = wrap_into(state.lines[i], &wrapped[wrapped_count], max_rows - wrapped_count, width);
    for(uint8_t j = 0; j < got && wrapped_count + j < max_rows; ++j) {
      source[wrapped_count + j] = i;
    }
    wrapped_count += got;
  }
  return wrapped_count;
}

void panel_line(int& y, const char* text, const uint16_t color = C_DIM) {
  draw_text(text, kRightX + 8, y, 1.0f, color);
  y += 10;
}

void panel_header(const char* title, const uint8_t page = 0, const uint8_t total_pages = 0) {
  char header[48];
  if(total_pages > 0) {
    std::snprintf(header, sizeof(header), "%s %u/%u", title, static_cast<unsigned int>(page + 1), static_cast<unsigned int>(total_pages));
  } else {
    std::snprintf(header, sizeof(header), "%s", title);
  }
  draw_text(header, kRightX + 8, kRightY + 8, 1.5f, C_WHITE);
}

void draw_left_column() {
  const uint16_t accent = buddySpeciesColor();
  const int center_x = kLeftX + (kLeftW / 2);
  const int text_max_width = kLeftW - 12;

  set_pen565(C_PANEL);
  graphics.rectangle(Rect(kLeftX, kLeftY, kLeftW, kLeftH));
  set_pen565(accent);
  graphics.rectangle(Rect(kLeftX, kLeftY, kLeftW, 6));

  draw_centered_fit(state.device_name, center_x, kLeftY + 16, kLeftW - 18, 2.0f, C_WHITE);
  if(state.owner[0] != '\0') {
    char owner_line[48];
    std::snprintf(owner_line, sizeof(owner_line), "owner: %s", state.owner);
    draw_centered_fit(owner_line, center_x, kLeftY + 30, text_max_width, 1.0f, C_DIM);
  } else {
    draw_centered_fit("Claude Hardware Buddy", center_x, kLeftY + 30, text_max_width, 1.0f, C_DIM);
  }

  graphics.set_clip(Rect(kLeftX + 2, kLeftY + 44, kLeftW - 4, 118));
  buddySetTarget(&graphics, kLeftX + 2, kLeftY + 40);
  buddySetPeek(false);
  buddyTick(state.active_state);
  graphics.remove_clip();

  char state_line[40];
  std::snprintf(state_line, sizeof(state_line), "%s  %s", state_names[state.active_state], buddySpeciesName());
  draw_centered_fit(state_line, center_x, kLeftY + 176, text_max_width, 1.0f, accent);
  if(!should_show_clock()) {
    draw_centered_fit(state.banner, center_x, kLeftY + 190, text_max_width, 1.0f, C_WHITE);
  }

  char stat_line[48];
  std::snprintf(stat_line, sizeof(stat_line), "T%lu R%lu W%lu", static_cast<unsigned long>(state.total),
                static_cast<unsigned long>(state.running), static_cast<unsigned long>(state.waiting));
  if(!should_show_clock()) {
    draw_centered_fit(stat_line, center_x, kLeftY + 204, text_max_width, 1.0f, C_DIM);
  }
}

void draw_clock_panel() {
  panel_header("DOCK CLOCK");

  static const char* kWeekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  const uint32_t epoch = current_local_epoch();
  std::time_t raw = static_cast<std::time_t>(epoch);
  std::tm local_tm{};
  gmtime_r(&raw, &local_tm);

  char time_line[16];
  char date_line[32];
  std::snprintf(time_line, sizeof(time_line), "%02d:%02d", local_tm.tm_hour, local_tm.tm_min);
  std::snprintf(date_line, sizeof(date_line), "%s %s %02d", kWeekdays[local_tm.tm_wday % 7],
                kMonths[local_tm.tm_mon % 12], local_tm.tm_mday);

  draw_centered(time_line, kRightX + (kRightW / 2), kRightY + 54, 4.0f, C_WHITE);
  draw_centered(date_line, kRightX + (kRightW / 2), kRightY + 112, 1.5f, C_DIM);
  draw_centered_fit(state.owner[0] != '\0' ? state.owner : "USB dock mode", kRightX + (kRightW / 2), kRightY + 132,
                    kRightW - 16, 1.0f, C_WHITE);
  draw_chip(Rect(kRightX + 18, kRightY + 158, 56, 18), C_PANEL_ALT, "USB");
  draw_chip(Rect(kRightX + 82, kRightY + 158, 56, 18), C_GREEN, state.secure ? "linked" : "pair", C_BG);
  draw_centered_fit("USB + time sync + idle", kRightX + (kRightW / 2), kRightY + 188, kRightW - 16, 1.0f, C_DIM);
}

void draw_normal_panel() {
  if(should_show_clock()) {
    draw_clock_panel();
    return;
  }

  panel_header("HOME");

  char chip_left[24];
  char chip_right[24];
  std::snprintf(chip_left, sizeof(chip_left), "%s", state.secure ? "secure" : state.link_connected ? "pairing" : "idle");
  std::snprintf(chip_right, sizeof(chip_right), "%u%% %s", static_cast<unsigned int>(state.battery_percent),
                state.usb_present ? "usb" : "bat");
  draw_chip(Rect(kRightX + 8, kRightY + 30, 66, 18), state.secure ? C_GREEN : C_PANEL_ALT, chip_left,
            state.secure ? C_BG : C_WHITE);
  draw_chip(Rect(kRightX + 82, kRightY + 30, 66, 18), state.battery_low ? C_HOT : C_PANEL_ALT, chip_right,
            state.battery_low ? C_WHITE : C_WHITE);

  int y = kRightY + 58;
  char line[64];
  std::snprintf(line, sizeof(line), "sessions %lu", static_cast<unsigned long>(state.total));
  panel_line(y, line, C_WHITE);
  std::snprintf(line, sizeof(line), "running  %lu", static_cast<unsigned long>(state.running));
  panel_line(y, line);
  std::snprintf(line, sizeof(line), "waiting  %lu", static_cast<unsigned long>(state.waiting));
  panel_line(y, line);
  std::snprintf(line, sizeof(line), "today    %lu", static_cast<unsigned long>(state.tokens_today));
  panel_line(y, line);
  y += 6;

  panel_line(y, "transcript", C_WHITE);

  if(state.line_gen != state.last_line_gen) {
    state.last_line_gen = state.line_gen;
    state.msg_scroll = 0;
  }

  char wrapped[32][28]{};
  uint8_t source[32]{};
  const uint8_t wrapped_count = build_wrapped_transcript(wrapped, source, 32, 20);

  if(state.transcript_enabled && wrapped_count > 0) {
    const int show = 8;
    const int max_back = wrapped_count > show ? wrapped_count - show : 0;
    if(state.msg_scroll > max_back) {
      state.msg_scroll = max_back;
    }
    const int end = static_cast<int>(wrapped_count) - state.msg_scroll;
    int start = end - show;
    if(start < 0) start = 0;
    const uint8_t newest = state.line_count == 0 ? 0 : state.line_count - 1;
    const uint8_t total_pages = static_cast<uint8_t>((wrapped_count + show - 1) / show);
    const uint8_t current_page = static_cast<uint8_t>((state.msg_scroll / show) + 1);
    for(int i = 0; start + i < end; ++i) {
      const uint8_t row = static_cast<uint8_t>(start + i);
      const bool fresh = source[row] == newest && state.msg_scroll == 0;
      panel_line(y, wrapped[row], fresh ? C_WHITE : C_DIM);
    }

    char pager_line[24];
    if(total_pages > 1) {
      std::snprintf(pager_line, sizeof(pager_line), "B more %u/%u", static_cast<unsigned int>(current_page),
                    static_cast<unsigned int>(total_pages));
      draw_text(pager_line, kRightX + 8, kRightY + kRightH - 24, 1.0f, buddySpeciesColor());
    } else {
      draw_text(state.last_ack, kRightX + 8, kRightY + kRightH - 24, 1.0f, C_DIM);
    }
  } else {
    char msg_wrapped[6][28]{};
    const uint8_t rows = wrap_into(state.msg, msg_wrapped, 6, 20);
    for(uint8_t i = 0; i < rows; ++i) {
      panel_line(y, msg_wrapped[i], i == 0 ? C_WHITE : C_DIM);
    }
    draw_text(state.last_ack, kRightX + 8, kRightY + kRightH - 24, 1.0f, C_DIM);
  }
}

void draw_prompt_panel() {
  const int overlay_x = 28;
  const int overlay_y = 40;
  const int overlay_w = 264;
  const int overlay_h = 156;

  set_pen565(C_PANEL_ALT);
  graphics.rectangle(Rect(overlay_x, overlay_y, overlay_w, overlay_h));
  set_pen565(C_HOT);
  graphics.rectangle(Rect(overlay_x, overlay_y, overlay_w, 6));
  draw_centered("APPROVAL", overlay_x + (overlay_w / 2), overlay_y + 12, 1.5f, C_WHITE);

  int y = overlay_y + 34;

  char waited[40];
  const uint32_t waited_seconds = (now_ms() - state.prompt_arrived_ms) / 1000;
  std::snprintf(waited, sizeof(waited), "approve? %lus", static_cast<unsigned long>(waited_seconds));
  draw_text(waited, overlay_x + 12, y, 1.0f, waited_seconds >= 10 ? C_HOT : C_WHITE);
  y += 6;

  if(state.prompt_tool[0] != '\0') {
    draw_text(state.prompt_tool, overlay_x + 12, y, std::strlen(state.prompt_tool) <= 12 ? 2.0f : 1.0f, C_WHITE);
    y += std::strlen(state.prompt_tool) <= 10 ? 24 : 12;
  }

  if(state.prompt_details) {
    char raw_rows[7][28]{};
    const uint8_t rows = wrap_into(state.last_rx, raw_rows, 7, 28);
    for(uint8_t i = 0; i < rows; ++i) {
      draw_text(raw_rows[i], overlay_x + 12, y, 1.0f, i == 0 ? C_WHITE : C_DIM);
      y += 10;
    }
  } else {
    char hint_rows[7][28]{};
    const char* prompt_text = state.prompt_hint[0] != '\0' ? state.prompt_hint : "Claude requested a permission decision.";
    const uint8_t rows = wrap_into(prompt_text, hint_rows, 7, 28);
    for(uint8_t i = 0; i < rows; ++i) {
      draw_text(hint_rows[i], overlay_x + 12, y, 1.0f, i == 0 ? C_WHITE : C_DIM);
      y += 10;
    }
  }

  y = overlay_y + overlay_h - 28;
  if(state.response_sent) {
    draw_text("sent...", overlay_x + 12, y, 1.0f, C_DIM);
  } else {
    draw_text("A approve", overlay_x + 12, y, 1.0f, C_GREEN);
    draw_text("B deny", overlay_x + overlay_w - 62, y, 1.0f, C_HOT);
  }
}

void draw_pet_panel() {
  panel_header("PET", state.pet_page, kPetPageCount);
  int y = kRightY + 34;

  if(state.pet_page == 0) {
    panel_line(y, "mood", C_WHITE);
    char hearts[8] = "....";
    for(uint8_t i = 0; i < mood_tier() && i < 4; ++i) hearts[i] = '*';
    panel_line(y, hearts, buddySpeciesColor());
    y += 4;

    panel_line(y, "fed", C_WHITE);
    char feed[12] = "..........";
    for(uint8_t i = 0; i < 10; ++i) feed[i] = i < fed_progress() ? 'o' : '.';
    panel_line(y, feed, buddySpeciesColor());
    y += 4;

    panel_line(y, "energy", C_WHITE);
    char energy[8] = ".....";
    for(uint8_t i = 0; i < 5; ++i) energy[i] = i < energy_tier() ? '#' : '.';
    panel_line(y, energy, energy_tier() >= 4 ? C_GREEN : energy_tier() >= 2 ? C_GOLD : C_HOT);
    y += 8;

    char line[48];
    std::snprintf(line, sizeof(line), "level    %lu", static_cast<unsigned long>(state.level));
    panel_line(y, line, C_WHITE);
    std::snprintf(line, sizeof(line), "approved %lu", static_cast<unsigned long>(state.approvals));
    panel_line(y, line);
    std::snprintf(line, sizeof(line), "denied   %lu", static_cast<unsigned long>(state.denials));
    panel_line(y, line);
    std::snprintf(line, sizeof(line), "napped   %luh%02lum", static_cast<unsigned long>(state.nap_seconds / 3600),
                  static_cast<unsigned long>((state.nap_seconds / 60) % 60));
    panel_line(y, line);
    std::snprintf(line, sizeof(line), "tokens   %lu", static_cast<unsigned long>(state.tokens_total));
    panel_line(y, line);
    std::snprintf(line, sizeof(line), "today    %lu", static_cast<unsigned long>(state.tokens_today));
    panel_line(y, line);
  } else {
    panel_line(y, "MOOD", buddySpeciesColor());
    panel_line(y, "approve fast = up");
    panel_line(y, "deny lots = down");
    y += 4;
    panel_line(y, "FED", buddySpeciesColor());
    panel_line(y, "50K tokens = level");
    panel_line(y, "level up = confetti");
    y += 4;
    panel_line(y, "ENERGY", buddySpeciesColor());
    panel_line(y, "hold Y to nap/wake");
    panel_line(y, "naps refill energy");
    y += 4;
    panel_line(y, "A mode  B page");
    panel_line(y, "hold A menu");
    panel_line(y, "X pet   hold X dizzy");
  }
}

void draw_info_panel() {
  panel_header("INFO", state.info_page, kInfoPageCount);
  int y = kRightY + 34;
  char line[64];

  switch(state.info_page) {
    case 0:
      panel_line(y, "ABOUT", C_WHITE);
      panel_line(y, "Pico-first Claude Buddy");
      y += 4;
      panel_line(y, "Left side keeps the pet");
      panel_line(y, "alive. Right side adapts");
      panel_line(y, "to status, prompts, and");
      panel_line(y, "dock-clock moments.");
      break;
    case 1:
      panel_line(y, "BUTTONS", C_WHITE);
      panel_line(y, "A  next screen");
      panel_line(y, "B  page / scroll");
      panel_line(y, "X  next pet");
      panel_line(y, "Y  home / back");
      panel_line(y, "hold A  settings/menu");
      panel_line(y, "hold X  dizzy");
      panel_line(y, "hold Y  nap");
      break;
    case 2:
      panel_line(y, "STATES", C_WHITE);
      panel_line(y, "sleep  disconnected / nap");
      panel_line(y, "idle   linked + calm");
      panel_line(y, "busy   Claude running");
      panel_line(y, "attention waiting prompt");
      panel_line(y, "celebrate task or level win");
      panel_line(y, "dizzy  manual easter egg");
      panel_line(y, "heart  quick approval");
      break;
    case 3:
      panel_line(y, "CLAUDE", C_WHITE);
      std::snprintf(line, sizeof(line), "sessions %lu", static_cast<unsigned long>(state.total));
      panel_line(y, line);
      std::snprintf(line, sizeof(line), "running  %lu", static_cast<unsigned long>(state.running));
      panel_line(y, line);
      std::snprintf(line, sizeof(line), "waiting  %lu", static_cast<unsigned long>(state.waiting));
      panel_line(y, line);
      std::snprintf(line, sizeof(line), "state    %s", state_names[state.active_state]);
      panel_line(y, line);
      break;
    case 4:
      panel_line(y, "DEVICE", C_WHITE);
      std::snprintf(line, sizeof(line), "battery  %u.%02uV", state.battery_mv / 1000, (state.battery_mv % 1000) / 10);
      panel_line(y, line);
      std::snprintf(line, sizeof(line), "usb      %s", state.usb_present ? "connected" : "battery");
      panel_line(y, line);
      std::snprintf(line, sizeof(line), "uptime   %lus", static_cast<unsigned long>(now_ms() / 1000));
      panel_line(y, line);
      std::snprintf(line, sizeof(line), "owner    %s", state.owner[0] != '\0' ? state.owner : "-");
      panel_line(y, line);
      panel_line(y, state.clock_enabled ? "dock clock ready" : "dock clock off");
      break;
    case 5:
      panel_line(y, "BLUETOOTH", C_WHITE);
      panel_line(y, state.link_connected ? "connected" : "discoverable",
                 state.link_connected ? C_GREEN : state.secure ? C_GREEN : C_DIM);
      panel_line(y, state.secure ? "encrypted" : "unencrypted");
      if(state.passkey[0] != '\0' && !state.secure) {
        std::snprintf(line, sizeof(line), "passkey  %s", state.passkey);
        panel_line(y, line, C_WHITE);
      }
      panel_line(y, "Developer >");
      panel_line(y, "Hardware Buddy");
      break;
    default:
      panel_line(y, "BUILD", C_WHITE);
      panel_line(y, "Anthropic buddy protocol");
      panel_line(y, "ported for Pico 2 W +");
      panel_line(y, "Pimoroni Display Pack 2.8");
      panel_line(y, installed_pack.valid ? "custom pack installed" : "ASCII species built in");
      panel_line(y, state.demo_mode ? "demo mode on" : "demo mode off");
      break;
  }
}

void draw_menu_like_overlay(const char* title, const char* const* items, const uint8_t count, const uint8_t selected,
                            const char* footer) {
  const int overlay_x = 44;
  const int overlay_y = 36;
  const int overlay_w = 232;
  const int overlay_h = count > 6 ? 186 : 170;
  const int row_step = count > 6 ? 18 : 22;

  set_pen565(C_PANEL_ALT);
  graphics.rectangle(Rect(overlay_x, overlay_y, overlay_w, overlay_h));
  set_pen565(buddySpeciesColor());
  graphics.rectangle(Rect(overlay_x, overlay_y, overlay_w, 6));
  draw_centered(title, overlay_x + (overlay_w / 2), overlay_y + 12, 1.5f, C_WHITE);

  int y = overlay_y + 32;
  for(uint8_t i = 0; i < count; ++i) {
    const bool active = i == selected;
    if(active) {
      set_pen565(buddySpeciesColor());
      graphics.rectangle(Rect(overlay_x + 12, y - 2, overlay_w - 24, row_step - 4));
    }
    draw_text(items[i], overlay_x + 18, y, 1.0f, active ? C_BG : C_WHITE);
    y += row_step;
  }
  draw_text(footer, overlay_x + 12, overlay_y + overlay_h - 18, 1.0f, C_DIM);
}

void draw_menu_overlay() {
  char item0[20] = "settings";
  char item1[20] = "screen off";
  char item2[20] = "help";
  char item3[20] = "about";
  char item4[20];
  char item5[20] = "close";
  std::snprintf(item4, sizeof(item4), "demo: %s", state.demo_mode ? "on" : "off");
  const char* items[] = {item0, item1, item2, item3, item4, item5};
  draw_menu_like_overlay("MENU", items, kMenuCount, state.menu_sel, "A next  B choose");
}

void draw_settings_overlay() {
  char item0[24];
  char item1[24];
  char item2[24];
  char item3[24];
  char item4[24];
  char item5[24];
  char item6[24] = "reset";
  char item7[24] = "back";
  std::snprintf(item0, sizeof(item0), "bright: %u/4", static_cast<unsigned int>(state.brightness_level));
  std::snprintf(item1, sizeof(item1), "led: %s", state.led_enabled ? "on" : "off");
  std::snprintf(item2, sizeof(item2), "transcript: %s", state.transcript_enabled ? "on" : "off");
  std::snprintf(item3, sizeof(item3), "demo: %s", state.demo_mode ? "on" : "off");
  std::snprintf(item4, sizeof(item4), "dock clock: %s", state.clock_enabled ? "on" : "off");
  std::snprintf(item5, sizeof(item5), "pet: %s", buddySpeciesName());
  const char* items[] = {item0, item1, item2, item3, item4, item5, item6, item7};
  draw_menu_like_overlay("SETTINGS", items, kSettingsCount, state.settings_sel, "A next  B choose");
}

void draw_reset_overlay() {
  char item0[24];
  char item1[24] = "factory reset";
  char item2[24] = "back";
  std::snprintf(item0, sizeof(item0), installed_pack.valid ? "delete char" : "delete char -");
  const char* items[] = {item0, item1, item2};
  draw_menu_like_overlay("RESET", items, kResetCount, state.reset_sel, "tap B twice to confirm");
  if(state.reset_confirm_idx != 0xff && now_ms() < state.reset_confirm_until) {
    draw_text("armed", 214, 188, 1.0f, C_HOT);
  }
}

void draw_transfer_panel() {
  panel_header("INSTALL");
  draw_text(state.xfer_name[0] != '\0' ? state.xfer_name : "custom pack", kRightX + 8, kRightY + 36, 2.0f, C_WHITE);

  char line[48];
  std::snprintf(line, sizeof(line), "%lu / %lu bytes", static_cast<unsigned long>(state.xfer_total_written),
                static_cast<unsigned long>(state.xfer_total));
  draw_text(line, kRightX + 8, kRightY + 64, 1.0f, C_DIM);

  set_pen565(C_PANEL_ALT);
  graphics.rectangle(Rect(kRightX + 8, kRightY + 84, kRightW - 16, 12));
  if(state.xfer_total > 0) {
    const int fill = static_cast<int>(((kRightW - 20) * state.xfer_total_written) / state.xfer_total);
    set_pen565(buddySpeciesColor());
    graphics.rectangle(Rect(kRightX + 10, kRightY + 86, std::max(0, fill), 8));
  }

  draw_text(state.xfer_file_path[0] != '\0' ? state.xfer_file_path : "manifest.json", kRightX + 8, kRightY + 108, 1.0f, C_WHITE);
  draw_text("Waiting for char_end ack...", kRightX + 8, kRightY + 126, 1.0f, C_DIM);
}

void draw_right_panel() {
  set_pen565(C_PANEL);
  graphics.rectangle(Rect(kRightX, kRightY, kRightW, kRightH));
  set_pen565(C_PANEL_ALT);
  graphics.rectangle(Rect(kRightX, kRightY, kRightW, 4));

  if(state.xfer_active) {
    draw_transfer_panel();
    return;
  }

  switch(state.display_mode) {
    case DisplayMode::Normal:
      draw_normal_panel();
      break;
    case DisplayMode::Pet:
      draw_pet_panel();
      break;
    case DisplayMode::Info:
      draw_info_panel();
      break;
  }
}

void draw_top_bar() {
  char top[48];
  std::snprintf(top, sizeof(top), "Claude Pico  %s", state.status_line);
  draw_text(top, 12, 2, 1.0f, C_DIM);
  if(!state.secure && state.passkey[0] != '\0' && state.link_connected) {
    char passkey[32];
    std::snprintf(passkey, sizeof(passkey), "pair %s", state.passkey);
    draw_text(passkey, 224, 2, 1.0f, C_GOLD);
  }
}

void draw_footer() {
  set_pen565(C_PANEL_ALT);
  graphics.rectangle(Rect(0, kFooterY, 320, 14));

  if(state.prompt_active && !state.response_sent) {
    draw_text("A ok  B deny  X raw  Y home  HA menu", 8, kFooterY + 2, 1.0f, C_WHITE);
    return;
  }
  if(state.menu_open || state.settings_open || state.reset_open) {
    draw_text("A next  B choose  Y back", 8, kFooterY + 2, 1.0f, C_WHITE);
    return;
  }

  switch(state.display_mode) {
    case DisplayMode::Normal:
      draw_text(should_show_clock() ? "A mode  X pet  Y home  HA menu" : "A mode  B page  X pet  Y home  HA menu", 8, kFooterY + 2,
                1.0f, C_WHITE);
      break;
    case DisplayMode::Pet:
    case DisplayMode::Info:
      draw_text("A mode  B page  X pet  Y home  HA menu", 8, kFooterY + 2, 1.0f, C_WHITE);
      break;
  }
}

void render_screen() {
  set_pen565(C_BG);
  graphics.clear();
  draw_top_bar();
  draw_left_column();
  draw_right_panel();
  if(state.prompt_active) {
    draw_prompt_panel();
  }
  if(state.menu_open) {
    draw_menu_overlay();
  } else if(state.settings_open) {
    draw_settings_overlay();
  } else if(state.reset_open) {
    draw_reset_overlay();
  }
  draw_footer();
  st7789.set_backlight(current_backlight());
  st7789.update(&graphics);
}

void update_led() {
  auto apply_led = [](const uint8_t r, const uint8_t g, const uint8_t b) {
    led.set_rgb(static_cast<uint8_t>((r * kLedMax) / 255), static_cast<uint8_t>((g * kLedMax) / 255),
                static_cast<uint8_t>((b * kLedMax) / 255));
  };

  if(!state.led_enabled) {
    apply_led(0, 0, 0);
    return;
  }
  if(state.xfer_active) {
    if((now_ms() / 250) % 2 == 0) apply_led(255, 0, 255);
    else apply_led(32, 0, 64);
    return;
  }
  if(state.battery_low) {
    apply_led(255, 0, 0);
    return;
  }
  if(state.prompt_active && !state.response_sent) {
    if((now_ms() / 300) % 2 == 0) apply_led(255, 160, 0);
    else apply_led(0, 0, 0);
    return;
  }
  if(!state.link_connected) {
    apply_led(0, 0, 0);
    return;
  }
  if(!state.secure) {
    apply_led(255, 0, 255);
    return;
  }
  switch(state.active_state) {
    case P_BUSY:
      apply_led(0, 0, 255);
      break;
    case P_CELEBRATE:
    case P_HEART:
      apply_led(255, 64, 160);
      break;
    case P_ATTENTION:
      apply_led(255, 180, 0);
      break;
    default:
      apply_led(0, 255, 0);
      break;
  }
}

void apply_menu_action() {
  switch(state.menu_sel) {
    case 0:
      state.menu_open = false;
      state.settings_open = true;
      state.settings_sel = 0;
      break;
    case 1:
      state.menu_open = false;
      state.screen_off = true;
      break;
    case 2:
      state.menu_open = false;
      state.display_mode = DisplayMode::Info;
      state.info_page = 1;
      break;
    case 3:
      state.menu_open = false;
      state.display_mode = DisplayMode::Info;
      state.info_page = 0;
      break;
    case 4:
      state.demo_mode = !state.demo_mode;
      state.menu_open = false;
      update_snapshot_banner();
      persist_state();
      break;
    default:
      state.menu_open = false;
      break;
  }
}

void apply_setting() {
  switch(state.settings_sel) {
    case 0:
      state.brightness_level = (state.brightness_level + 1) % 5;
      persist_state();
      break;
    case 1:
      state.led_enabled = !state.led_enabled;
      persist_state();
      break;
    case 2:
      state.transcript_enabled = !state.transcript_enabled;
      persist_state();
      break;
    case 3:
      state.demo_mode = !state.demo_mode;
      persist_state();
      break;
    case 4:
      state.clock_enabled = !state.clock_enabled;
      persist_state();
      break;
    case 5:
      buddyNextSpecies();
      persist_state();
      break;
    case 6:
      state.settings_open = false;
      state.reset_open = true;
      state.reset_sel = 0;
      state.reset_confirm_idx = 0xff;
      return;
    default:
      state.settings_open = false;
      return;
  }
}

void apply_reset() {
  if(state.reset_sel == 2) {
    state.reset_open = false;
    return;
  }

  const bool armed = state.reset_confirm_idx == state.reset_sel && now_ms() < state.reset_confirm_until;
  if(!armed) {
    state.reset_confirm_idx = state.reset_sel;
    state.reset_confirm_until = now_ms() + 3000;
    return;
  }

  if(state.reset_sel == 0) {
    delete_custom_pack();
    set_ack("Deleted character");
  } else if(state.reset_sel == 1) {
    if(state.connection_handle != HCI_CON_HANDLE_INVALID) {
      gap_delete_bonding(state.peer_addr_type, state.peer_addr);
    }
    state.paired = false;
    state.secure = false;
    state.security_level = 0;
    factory_reset_state();
    set_ack("Factory reset");
  }
  state.reset_open = false;
  state.reset_confirm_idx = 0xff;
}

void wake_screen() {
  state.screen_off = false;
  state.last_input = get_absolute_time();
}

DisplayMode next_display_mode(const DisplayMode current) {
  switch(current) {
    case DisplayMode::Normal:
      return DisplayMode::Pet;
    case DisplayMode::Pet:
      return DisplayMode::Info;
    case DisplayMode::Info:
      return DisplayMode::Normal;
  }
  return DisplayMode::Normal;
}

void handle_buttons() {
  const absolute_time_t now = get_absolute_time();
  const bool a = button_a.raw();
  const bool b = button_b.raw();
  const bool x = button_x.raw();
  const bool y = button_y.raw();

  bool a_pressed = false;
  bool a_held = false;
  bool x_pressed = false;
  bool x_held = false;
  bool y_held = false;
  const bool b_pressed = b && !state.last_b;
  const bool y_pressed = y && !state.last_y;

  if(a && !state.last_a) {
    state.a_down_since = now;
    state.a_hold_fired = false;
  }
  if(x && !state.last_x) {
    state.x_down_since = now;
    state.x_hold_fired = false;
  }
  if(y && !state.last_y) {
    state.y_down_since = now;
    state.y_hold_fired = false;
  }

  if(a && !state.a_hold_fired && !is_nil_time(state.a_down_since) &&
     absolute_time_diff_us(state.a_down_since, now) > static_cast<int64_t>(kHoldThresholdMs) * 1000) {
    a_held = true;
    state.a_hold_fired = true;
  }
  if(x && !state.x_hold_fired && !is_nil_time(state.x_down_since) &&
     absolute_time_diff_us(state.x_down_since, now) > static_cast<int64_t>(kHoldThresholdMs) * 1000) {
    x_held = true;
    state.x_hold_fired = true;
  }
  if(y && !state.y_hold_fired && !is_nil_time(state.y_down_since) &&
     absolute_time_diff_us(state.y_down_since, now) > static_cast<int64_t>(kHoldThresholdMs) * 1000) {
    y_held = true;
    state.y_hold_fired = true;
  }

  if(!a && state.last_a) {
    if(!state.a_hold_fired) {
      a_pressed = true;
    }
    state.a_down_since = nil_time;
    state.a_hold_fired = false;
  }
  if(!x && state.last_x) {
    if(!state.x_hold_fired) {
      x_pressed = true;
    }
    state.x_down_since = nil_time;
    state.x_hold_fired = false;
  }
  if(!y && state.last_y) {
    state.y_down_since = nil_time;
    state.y_hold_fired = false;
  }

  if(a_pressed || a_held || b_pressed || x_pressed || x_held || y_pressed || y_held) {
    state.last_input = now;
  }

  if(state.screen_off) {
    if(a_pressed || b_pressed || x_pressed || x_held || y_pressed || a_held || y_held) {
      wake_screen();
    }
    state.last_a = a;
    state.last_b = b;
    state.last_x = x;
    state.last_y = y;
    return;
  }

  if(state.napping) {
    if(a_pressed || b_pressed || x_pressed || y_pressed || x_held || y_held) {
      toggle_nap();
    }
    state.last_a = a;
    state.last_b = b;
    state.last_x = x;
    state.last_y = y;
    return;
  }

  if(y_held) {
    toggle_nap();
    state.last_a = a;
    state.last_b = b;
    state.last_x = x;
    state.last_y = y;
    return;
  }

  if(x_held && !state.prompt_active && !state.menu_open && !state.settings_open && !state.reset_open) {
    trigger_one_shot(P_DIZZY, 2200);
    set_ack("Dizzy");
    state.last_a = a;
    state.last_b = b;
    state.last_x = x;
    state.last_y = y;
    return;
  }

  if(state.prompt_active && !state.response_sent) {
    if(a_pressed) {
      handle_prompt_decision("once");
    } else if(b_pressed) {
      handle_prompt_decision("deny");
    } else if(x_pressed) {
      state.prompt_details = !state.prompt_details;
    } else if(y_pressed) {
      state.display_mode = DisplayMode::Normal;
    }
    state.last_a = a;
    state.last_b = b;
    state.last_x = x;
    state.last_y = y;
    return;
  }

  if(a_held) {
    state.menu_open = !state.menu_open;
    if(state.menu_open) {
      state.settings_open = false;
      state.reset_open = false;
      state.menu_sel = 0;
    }
    state.last_a = a;
    state.last_b = b;
    state.last_x = x;
    state.last_y = y;
    return;
  }

  if(state.menu_open) {
    if(a_pressed) state.menu_sel = (state.menu_sel + 1) % kMenuCount;
    else if(b_pressed) apply_menu_action();
    else if(y_pressed) state.menu_open = false;
  } else if(state.settings_open) {
    if(a_pressed) state.settings_sel = (state.settings_sel + 1) % kSettingsCount;
    else if(b_pressed) apply_setting();
    else if(y_pressed) state.settings_open = false;
  } else if(state.reset_open) {
    if(a_pressed) {
      state.reset_sel = (state.reset_sel + 1) % kResetCount;
      state.reset_confirm_idx = 0xff;
    } else if(b_pressed) {
      apply_reset();
    } else if(y_pressed) {
      state.reset_open = false;
      state.reset_confirm_idx = 0xff;
    }
  } else {
    if(a_pressed) {
      state.display_mode = next_display_mode(state.display_mode);
    }
    if(b_pressed) {
      if(state.display_mode == DisplayMode::Pet) {
        state.pet_page = (state.pet_page + 1) % kPetPageCount;
      } else if(state.display_mode == DisplayMode::Info) {
        state.info_page = (state.info_page + 1) % kInfoPageCount;
      } else if(!should_show_clock()) {
        char wrapped[32][28]{};
        uint8_t source[32]{};
        const uint8_t wrapped_count = build_wrapped_transcript(wrapped, source, 32, 20);
        const uint8_t show = 8;
        const uint8_t max_back = wrapped_count > show ? wrapped_count - show : 0;
        if(max_back == 0) {
          state.msg_scroll = 0;
        } else {
          const uint8_t next = static_cast<uint8_t>(state.msg_scroll + show);
          state.msg_scroll = next > max_back ? 0 : next;
        }
      }
    }
    if(x_pressed) {
      buddyNextSpecies();
      persist_state();
    }
    if(y_pressed) {
      state.display_mode = DisplayMode::Normal;
      state.msg_scroll = 0;
    }
  }

  state.last_a = a;
  state.last_b = b;
  state.last_x = x;
  state.last_y = y;
}

void nordic_spp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
  UNUSED(channel);

  static char line_buffer[768];
  static size_t line_length = 0;

  switch(packet_type) {
    case HCI_EVENT_PACKET:
      if(hci_event_packet_get_type(packet) != HCI_EVENT_GATTSERVICE_META) {
        break;
      }
      switch(hci_event_gattservice_meta_get_subevent_code(packet)) {
        case GATTSERVICE_SUBEVENT_SPP_SERVICE_CONNECTED:
          state.connection_handle = gattservice_subevent_spp_service_connected_get_con_handle(packet);
          state.nus_linked = true;
          set_status(state.secure ? "Claude linked" : "Linked, securing");
          update_snapshot_banner();
          break;
        case GATTSERVICE_SUBEVENT_SPP_SERVICE_DISCONNECTED:
          state.nus_linked = false;
          set_status(state.link_connected ? "Connected" : "Advertising");
          break;
        default:
          break;
      }
      break;

    case RFCOMM_DATA_PACKET:
      for(uint16_t i = 0; i < size; ++i) {
        const char ch = static_cast<char>(packet[i]);
        if(ch == '\r') {
          continue;
        }
        if(ch == '\n') {
          line_buffer[line_length] = '\0';
          if(line_length > 0) {
            handle_protocol_line(line_buffer);
          }
          line_length = 0;
          continue;
        }
        if(line_length < sizeof(line_buffer) - 1) {
          line_buffer[line_length++] = ch;
        }
      }
      break;

    default:
      break;
  }
}

void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
  UNUSED(channel);
  UNUSED(size);

  if(packet_type != HCI_EVENT_PACKET) {
    return;
  }

  switch(hci_event_packet_get_type(packet)) {
    case BTSTACK_EVENT_STATE:
      if(btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
        set_status("Advertising");
        set_banner("Waiting for Claude Desktop");
      }
      break;

    case HCI_EVENT_META_GAP:
      switch(hci_event_gap_meta_get_subevent_code(packet)) {
        case GAP_SUBEVENT_LE_CONNECTION_COMPLETE:
          state.connection_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
          state.peer_addr_type = static_cast<bd_addr_type_t>(gap_subevent_le_connection_complete_get_peer_address_type(packet));
          gap_subevent_le_connection_complete_get_peer_address(packet, state.peer_addr);
          state.link_connected = true;
          state.advertising = false;
          state.secure = false;
          state.security_level = 0;
          state.connected_since = get_absolute_time();
          state.last_input = get_absolute_time();
          state.reconnects += 1;
          state.screen_off = false;
          set_status("Connected");
          set_banner("Requesting secure pairing");
          sm_request_pairing(state.connection_handle);
          break;
        default:
          break;
      }
      break;

    case GAP_EVENT_SECURITY_LEVEL: {
      const uint8_t level = gap_event_security_level_get_security_level(packet);
      state.security_level = level;
      state.secure = level >= LEVEL_2;
      state.paired = level >= LEVEL_3;
      update_snapshot_banner();
      break;
    }

    case HCI_EVENT_ENCRYPTION_CHANGE:
    case HCI_EVENT_ENCRYPTION_CHANGE_V2:
      if(hci_event_encryption_change_get_status(packet) == ERROR_CODE_SUCCESS &&
         hci_event_encryption_change_get_encryption_enabled(packet) != 0) {
        set_banner("Link encryption enabled");
      }
      break;

    case HCI_EVENT_DISCONNECTION_COMPLETE:
      state.connection_handle = HCI_CON_HANDLE_INVALID;
      state.link_connected = false;
      state.secure = false;
      state.paired = false;
      state.nus_linked = false;
      state.advertising = true;
      state.security_level = 0;
      state.passkey[0] = '\0';
      state.running = 0;
      state.waiting = 0;
      state.recently_completed = false;
      clear_prompt();
      set_status("Advertising");
      set_banner("Waiting for Claude Desktop");
      break;

    default:
      break;
  }
}

void sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
  UNUSED(channel);
  UNUSED(size);

  if(packet_type != HCI_EVENT_PACKET) {
    return;
  }

  switch(hci_event_packet_get_type(packet)) {
    case SM_EVENT_JUST_WORKS_REQUEST:
      sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
      break;

    case SM_EVENT_PAIRING_STARTED:
      set_status("Pairing");
      set_banner("Accept the Bluetooth pairing prompt");
      break;

    case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
      std::snprintf(state.passkey, sizeof(state.passkey), "%06" PRIu32, sm_event_passkey_display_number_get_passkey(packet));
      set_status("Enter passkey");
      set_banner("Enter the passkey on your Mac");
      break;

    case SM_EVENT_PAIRING_COMPLETE:
      if(sm_event_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
        state.secure = true;
        state.paired = true;
        set_status("Paired");
        set_banner("Secure link ready");
      } else {
        state.secure = false;
        state.paired = false;
        set_status("Pairing failed");
        set_banner("Retry pairing from Claude Desktop");
      }
      break;

    case SM_EVENT_REENCRYPTION_COMPLETE:
      if(sm_event_reencryption_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
        state.secure = true;
        state.paired = true;
        set_status("Secure reconnect");
        set_banner("Bonded reconnect complete");
      } else {
        state.secure = false;
        set_status("Reconnect failed");
      }
      break;

    default:
      break;
  }
}

void ui_timer_handler(btstack_timer_source* ts) {
  update_battery_state();

  if(state.demo_mode) {
    apply_demo_snapshot();
  } else if(desktop_active() == false) {
    state.running = 0;
    state.waiting = 0;
    state.recently_completed = false;
    if(!state.link_connected) {
      copy_string(state.msg, sizeof(state.msg), "No Claude connected");
    }
  }

  if(!state.prompt_active && !state.screen_off && !state.napping && !state.usb_present &&
     !is_nil_time(state.last_input) && now_ms() - to_ms_since_boot(state.last_input) > kScreenOffAfterMs) {
    state.screen_off = true;
  }

  handle_buttons();
  flush_scheduled_persist();
  state.base_state = derive_state();
  if(static_cast<int32_t>(now_ms() - state.one_shot_until_ms) >= 0) {
    state.active_state = state.base_state;
  }
  update_led();
  render_screen();

  btstack_run_loop_set_timer(ts, kUiTickMs);
  btstack_run_loop_add_timer(ts);
}

void setup_ble() {
  l2cap_init();
  sm_init();

  sm_set_secure_connections_only_mode(true);
  sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
  sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_MITM_PROTECTION | SM_AUTHREQ_BONDING);
  sm_use_fixed_passkey_in_display_role(kDefaultPasskey);

  att_server_init(profile_data, nullptr, nullptr);
  nordic_spp_service_server_init(&nordic_spp_packet_handler);

  const uint16_t adv_int_min = 0x0030;
  const uint16_t adv_int_max = 0x0030;
  const uint8_t adv_type = 0;
  bd_addr_t null_addr;
  std::memset(null_addr, 0, sizeof(null_addr));

  gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
  gap_advertisements_set_data(adv_data_len, const_cast<uint8_t*>(adv_data));
  gap_scan_response_set_data(scan_response_data_len, const_cast<uint8_t*>(scan_response_data));
  gap_advertisements_enable(1);

  state.hci_event_callback_registration.callback = &hci_packet_handler;
  hci_add_event_handler(&state.hci_event_callback_registration);

  state.sm_event_callback_registration.callback = &sm_packet_handler;
  sm_add_event_handler(&state.sm_event_callback_registration);
}

}  // namespace

int main() {
  stdio_init_all();
  flash_safe_execute_core_init();

  adc_init();
  adc_gpio_init(29);

  st7789.set_backlight(kBrightnessLevels[state.brightness_level]);

  if(cyw43_arch_init()) {
    while(true) {
      sleep_ms(250);
    }
  }

  graphics.set_font("bitmap6");
  state.passkey[0] = '\0';
  state.last_input = get_absolute_time();
  state.last_nap_end_ms = now_ms();

  buddyInit();
  load_persistent_state();
  update_battery_state();
  setup_ble();
  update_snapshot_banner();
  render_screen();
  update_led();

  state.ui_timer.process = &ui_timer_handler;
  btstack_run_loop_set_timer(&state.ui_timer, kUiTickMs);
  btstack_run_loop_add_timer(&state.ui_timer);

  hci_power_control(HCI_POWER_ON);
  btstack_run_loop_execute();
  return 0;
}
