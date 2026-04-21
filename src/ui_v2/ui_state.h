// ui_state.h — read-only snapshot of what V2 screens need from V1.
//
// V1 (the monolith) remains authoritative for BLE state, protocol parsing,
// pairing, battery sampling, and persistence. V2 never reads the V1
// BuddyState struct directly. Instead, V1 fills a BuddyInputs snapshot
// once per 40 ms tick before calling ui_v2::core::tick_40ms().
//
// This keeps V2 testable in isolation: a smoke target fills BuddyInputs
// with synthetic values and the whole UI renders against them without
// any BLE or flash machinery.
//
// V2 writes intents into BuddyOutputs. V1 reads those after the tick and
// acts on them (send approve/deny over NUS, mark persist dirty, etc.).
#pragma once

#include <cstdint>

namespace ui_v2 {

// Physical buttons, matching the Pimoroni Display Pack.
enum class Button : uint8_t { A, B, X, Y };

// Edge type for button events.
enum class ButtonEdge : uint8_t {
  Down,         // press
  Up,           // release
  ShortTap,     // <800 ms from down→up
  HoldFired,    // held ≥800 ms (emitted once, at threshold)
};

// Mirror of the seven V1 persona states. V2 uses its own expression
// library on top, but the base state still drives LED and some defaults.
enum class PersonaState : uint8_t {
  Sleep,
  Idle,
  Busy,
  Attention,
  Celebrate,
  Dizzy,
  Heart,
};

// Connection / security state for the LNK chip.
enum class LinkState : uint8_t {
  NotAdvertising,
  Advertising,
  Connecting,
  PairingPasskey,
  LinkedInsecure,
  LinkedSecure,
};

// Power source / charging state.
enum class PowerState : uint8_t {
  Battery,
  UsbPowered,
  UsbCharging,
};

enum class PowerEvent : uint8_t {
  None,
  UsbPlug,
  UsbUnplug,
};

enum class MotionProfile : uint8_t {
  Full,
  Reduced,
  Static,
};

enum class FaceStyle : uint8_t {
  Classic,
  Hybrid,
  Mascot,
};

// Risk tier computed by risk_tier.cpp from prompt.tool.
enum class RiskTier : uint8_t { Safe, Neutral, Caution, Danger };

// ---------------------------------------------------------------
// Inputs: everything V2 reads. Owned by V1; V1 refreshes before tick.
// ---------------------------------------------------------------
struct BuddyInputs {
  // Time.
  uint32_t now_ms = 0;

  // Identity.
  char device_name[32] = "Claude Pico";
  char owner[32]       = "";

  // Link.
  LinkState  link        = LinkState::NotAdvertising;
  char       passkey[16] = "";

  // Power.
  PowerState power          = PowerState::Battery;
  uint8_t    battery_pct    = 0;    // 0..100, estimate
  uint16_t   battery_mv     = 0;    // raw VSYS reading
  bool       battery_low    = false;
  PowerEvent power_event    = PowerEvent::None;
  uint16_t   power_event_seq = 0;

  // Protocol (heartbeat).
  uint32_t total        = 0;
  uint32_t running      = 0;
  uint32_t waiting      = 0;
  char     msg[96]      = "";
  uint32_t tokens_today = 0;
  uint32_t tokens_total = 0;
  uint16_t token_rate_per_min = 0;
  uint32_t heartbeat_age_ms   = 0;

  // Transcript (up to 8 lines of heartbeat entries[]).
  uint8_t  line_count = 0;
  char     lines[8][96] = {};
  uint16_t line_gen = 0;  // bumped whenever lines change

  // Velocity ring (tokens/min snapshots, 8 slots).
  uint16_t velocity[8] = {};
  uint8_t  velocity_count = 0;

  // Prompt (permission request).
  bool     prompt_active = false;
  bool     prompt_sent   = false;
  char     prompt_id[64] = "";
  char     prompt_tool[32] = "";
  char     prompt_hint[96] = "";
  uint32_t prompt_arrived_ms = 0;

  // Persona base state (V1 drives this from protocol).
  PersonaState persona = PersonaState::Sleep;

  // Persisted counters (V2 renders them; V1 mutates them).
  uint32_t approvals = 0;
  uint32_t denials   = 0;
  uint32_t nap_seconds = 0;
  uint32_t level = 0;

  // Local pet state (synthetic, device-owned).
  uint8_t  pet_fed_pct    = 80;
  uint8_t  pet_energy_pct = 70;
  uint16_t pet_feeds      = 0;
  uint16_t pet_plays      = 0;
  uint32_t pet_age_minutes = 0;

  // Settings.
  uint8_t brightness_level = 4;
  bool    led_enabled         = true;
  bool    transcript_enabled  = true;
  bool    demo_enabled        = false;
  bool    dock_clock_enabled  = true;
  MotionProfile motion_profile = MotionProfile::Full;
  FaceStyle     face_style     = FaceStyle::Hybrid;

  // RTC.
  bool     rtc_valid      = false;
  uint32_t rtc_local_epoch = 0;

  // Character pack transfer.
  bool     xfer_active = false;
  uint32_t xfer_written = 0;
  uint32_t xfer_total   = 0;
};

// ---------------------------------------------------------------
// Outputs: intents the UI produced this tick. Owned by V1.
// V1 resets these to defaults before each tick; V2 only sets them
// when it wants an action. V1 drains them after the tick.
// ---------------------------------------------------------------
struct BuddyOutputs {
  // Prompt response.
  bool send_approve_once = false;
  bool send_deny         = false;

  // Persona override (one-shot expressions).
  bool         set_persona      = false;
  PersonaState persona_override = PersonaState::Idle;
  uint32_t     persona_until_ms = 0;

  // Settings mutations.
  bool    dirty_persist     = false;  // V1 will schedule a flash write
  bool    set_brightness    = false;
  uint8_t brightness_level  = 4;
  bool    set_led_enabled   = false;
  bool    led_enabled       = true;
  bool    set_transcript    = false;
  bool    transcript_enabled = true;
  bool    set_demo_enabled  = false;
  bool    demo_enabled      = false;
  bool    set_dock_clock    = false;
  bool    dock_clock_enabled = true;
  bool          set_motion_profile = false;
  MotionProfile motion_profile     = MotionProfile::Full;
  bool          set_face_style     = false;
  FaceStyle     face_style         = FaceStyle::Hybrid;

  // Lifecycle.
  bool request_screen_off = false;
  bool request_nap_toggle = false;
  bool set_advertising     = false;
  bool advertising_enabled = false;
  bool request_clear_stats  = false;
  bool request_factory_reset = false;
  bool request_clear_pack    = false;
  bool request_unpair        = false;
  bool request_pet_feed      = false;
  bool request_pet_play      = false;
  bool    set_character_index = false;
  uint8_t character_index     = 0;

  // LED (V2 drives LED since it knows screen context).
  bool    set_led_rgb = false;
  uint8_t led_r = 0;
  uint8_t led_g = 0;
  uint8_t led_b = 0;

  // Backlight (V2 asks V1 to set; V1 actually pokes st7789).
  bool    set_backlight     = false;
  uint8_t backlight_level   = 220;  // 0..255
};

}  // namespace ui_v2
