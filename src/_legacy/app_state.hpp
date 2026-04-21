#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "pico/time.h"

enum class UiPage : uint8_t {
  Home,
  Activity,
  Prompt,
  Stats,
  Settings,
};

enum class BuddyState : uint8_t {
  Sleep,
  Idle,
  Busy,
  Attention,
  Celebrate,
  Heart,
};

enum class LedMode : uint8_t {
  Off,
  Green,
  Blue,
  Amber,
  Magenta,
  Red,
};

struct PromptState {
  bool active = false;
  std::string id;
  std::string tool;
  std::string hint;
};

struct SessionSnapshot {
  int total = 0;
  int running = 0;
  int waiting = 0;
  int tokens = 0;
  int tokens_today = 0;
  std::string msg = "Booting";
  std::array<std::string, 3> entries{};
  PromptState prompt{};
  absolute_time_t last_update = nil_time;
};

struct BatteryStatus {
  uint16_t millivolts = 0;
  uint8_t percent = 0;
  bool usb_present = false;
  bool low = false;
  absolute_time_t last_sample = nil_time;
};

struct LinkState {
  bool connected = false;
  bool secure = false;
  absolute_time_t connected_since = nil_time;
};

struct RuntimeStats {
  uint32_t approvals = 0;
  uint32_t denials = 0;
  uint32_t reconnects = 0;
};

struct UiState {
  UiPage page = UiPage::Home;
  bool prompt_details = false;
  bool transcript_expanded = false;
  uint8_t activity_scroll = 0;
  bool keep_awake = false;
};

struct SettingsData {
  std::string device_name = "Claude Pico";
  std::string owner_name;
  uint8_t brightness = 90;
  uint16_t idle_dim_seconds = 30;
};

struct ButtonState {
  bool a_pressed = false;
  bool b_pressed = false;
  bool x_pressed = false;
  bool y_pressed = false;
  bool a_held = false;
};

struct AppState {
  SessionSnapshot snapshot{};
  BatteryStatus battery{};
  LinkState link{};
  RuntimeStats stats{};
  UiState ui{};
  SettingsData settings{};
  BuddyState buddy_state = BuddyState::Sleep;
  LedMode led_mode = LedMode::Off;
};
