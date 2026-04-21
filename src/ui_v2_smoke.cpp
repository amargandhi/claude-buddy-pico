// ui_v2_smoke.cpp — bench target for the V2 UI module.
//
// This program stands up just enough hardware (display, buttons, LED, VSYS
// ADC, CYW43 for VBUS sensing) to drive the ui_v2 dispatcher against
// synthetic BuddyInputs. It never touches BLE, flash, or the V1 monolith.
//
// What you get on the panel:
//   - the full 15-screen V2 UI rendering at 25 fps
//   - a scripted "state machine" of synthetic inputs that cycles through
//     idle → busy → approval → idle so every screen has something to show
//   - button edges (ShortTap / HoldFired / Down / Up) forwarded to ui_core
//   - BuddyOutputs drained each tick and printed to stdio so you can see
//     what V1 would have acted on (send/deny, persist, LED, etc.)
//
// Build:   cmake --build build --target claude_buddy_pico_v2_smoke
// Flash:   standard .uf2 drop onto RPI-RP2 mount.

#include <cstdio>
#include <cstring>

#include "hardware/adc.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "button.hpp"
#include "drivers/st7789/st7789.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"
#include "pico_display_28.hpp"
#include "rgbled.hpp"

#include "ui_v2/ui_core.h"
#include "ui_v2/ui_state.h"

using namespace pimoroni;

namespace {

// ----------------------------------------------------------------------------
// Hardware.
// ----------------------------------------------------------------------------

constexpr float kAdcReferenceVolts    = 3.3f;
constexpr float kVoltageDividerScale  = 3.0f;
constexpr uint32_t kHoldThresholdMs   = 800;
constexpr uint32_t kTickIntervalMs    = 40;

ST7789 st7789(PicoDisplay28::WIDTH, PicoDisplay28::HEIGHT, ROTATE_0, false,
              get_spi_pins(BG_SPI_FRONT));
PicoGraphics_PenRGB332 graphics(st7789.width, st7789.height, nullptr);

RGBLED led(PicoDisplay28::LED_R, PicoDisplay28::LED_G, PicoDisplay28::LED_B);

Button button_a(PicoDisplay28::A);
Button button_b(PicoDisplay28::B);
Button button_x(PicoDisplay28::X);
Button button_y(PicoDisplay28::Y);

// ----------------------------------------------------------------------------
// Button edge tracker.
//
// The Pimoroni Button helper only gives us raw() level; V2 needs explicit
// Down / Up / ShortTap / HoldFired edges. We keep per-button state here and
// translate raw transitions into those events.
// ----------------------------------------------------------------------------
struct BtnTrack {
  ui_v2::Button id;
  bool          last      = false;
  uint32_t      down_ms   = 0;
  bool          hold_fired = false;
};

BtnTrack bt_a{ui_v2::Button::A};
BtnTrack bt_b{ui_v2::Button::B};
BtnTrack bt_x{ui_v2::Button::X};
BtnTrack bt_y{ui_v2::Button::Y};

void track_button(BtnTrack& bt, bool pressed, uint32_t now_ms) {
  if(pressed && !bt.last) {
    // Press edge.
    bt.down_ms    = now_ms;
    bt.hold_fired = false;
    ui_v2::core::on_button_event(bt.id, ui_v2::ButtonEdge::Down, now_ms);
  } else if(!pressed && bt.last) {
    // Release edge.
    ui_v2::core::on_button_event(bt.id, ui_v2::ButtonEdge::Up, now_ms);
    if(!bt.hold_fired && (now_ms - bt.down_ms) < kHoldThresholdMs) {
      ui_v2::core::on_button_event(bt.id, ui_v2::ButtonEdge::ShortTap, now_ms);
    }
    bt.hold_fired = false;
  } else if(pressed && !bt.hold_fired &&
            (now_ms - bt.down_ms) >= kHoldThresholdMs) {
    bt.hold_fired = true;
    ui_v2::core::on_button_event(bt.id, ui_v2::ButtonEdge::HoldFired, now_ms);
  }
  bt.last = pressed;
}

uint16_t read_vsys_mv() {
  adc_select_input(3);
  sleep_us(10);
  const uint16_t raw = adc_read();
  const float volts = (static_cast<float>(raw) * kAdcReferenceVolts *
                       kVoltageDividerScale) / 4095.0f;
  return static_cast<uint16_t>(volts * 1000.0f);
}

// ----------------------------------------------------------------------------
// Synthetic input generator.
//
// Rotates through the ten-second story below so the UI has plausible data:
//   0–3 s   idle: persona=Idle, running=0, no prompt
//   3–6 s   busy: persona=Busy, running=2, tokens climbing, transcript grows
//   6–10 s  prompt: a CAUTION-tier Edit request; operator can A/B/Y it
//   10+ s   loops
// ----------------------------------------------------------------------------
struct Script {
  uint32_t start_ms = 0;
  uint32_t tokens_today = 142300;
  uint32_t tokens_total = 8421900;
  uint32_t approvals    = 21;
  uint32_t denials      = 2;
  uint32_t heartbeats   = 1877;
  uint32_t level        = 4;
  uint16_t line_gen     = 7;
  uint32_t prompt_loop_start_ms = 0;
  char     prompt_id[32] = "";

  // Persona override latch (last applied via BuddyOutputs.set_persona).
  ui_v2::PersonaState persona_override      = ui_v2::PersonaState::Idle;
  uint32_t             persona_override_until = 0;

  // Settings latched from outputs.
  uint8_t brightness         = 4;
  bool    led_enabled        = true;
  bool    transcript_enabled = true;
  bool    demo_enabled       = false;
  bool    dock_clock_enabled = true;
  ui_v2::MotionProfile motion_profile = ui_v2::MotionProfile::Full;
  ui_v2::FaceStyle     face_style     = ui_v2::FaceStyle::Hybrid;
  ui_v2::PowerEvent    power_event    = ui_v2::PowerEvent::None;
  uint16_t             power_event_seq = 0;
  bool                 prev_usb_present = false;

  // Prompt dismissed by operator. Held until story-loop rolls around again.
  bool prompt_closed = false;
  uint32_t last_phase_tokens_stamp = 0;
};

Script g_script;

void fill_inputs(ui_v2::BuddyInputs& in, uint32_t now_ms, bool usb_present,
                 uint16_t vsys_mv) {
  in = ui_v2::BuddyInputs{};
  in.now_ms = now_ms;
  std::snprintf(in.device_name, sizeof(in.device_name), "Claude Pico");
  std::snprintf(in.owner,       sizeof(in.owner),       "demo");

  // Link: pretend we linked-secure 2 s after boot.
  const uint32_t since_boot = now_ms - g_script.start_ms;
  if(since_boot < 1500)      in.link = ui_v2::LinkState::Advertising;
  else if(since_boot < 2500) in.link = ui_v2::LinkState::PairingPasskey;
  else                        in.link = ui_v2::LinkState::LinkedSecure;
  std::snprintf(in.passkey, sizeof(in.passkey), "426931");

  // Power.
  in.power       = usb_present ? ui_v2::PowerState::UsbPowered
                                : ui_v2::PowerState::Battery;
  in.battery_mv  = vsys_mv;
  in.battery_pct = vsys_mv >= 4100 ? 100
                    : vsys_mv >= 3700 ? static_cast<uint8_t>(50 + (vsys_mv - 3700) / 8)
                    : vsys_mv >= 3200 ? static_cast<uint8_t>((vsys_mv - 3200) / 10)
                    : 5;
  in.battery_low = in.battery_pct < 15 && !usb_present;
  if(since_boot > 0 && usb_present != g_script.prev_usb_present) {
    g_script.power_event = usb_present ? ui_v2::PowerEvent::UsbPlug
                                       : ui_v2::PowerEvent::UsbUnplug;
    g_script.power_event_seq = static_cast<uint16_t>(g_script.power_event_seq + 1u);
  }
  g_script.prev_usb_present = usb_present;
  in.power_event = g_script.power_event;
  in.power_event_seq = g_script.power_event_seq;

  // Scripted scene.
  const uint32_t t = since_boot % 10000;
  if(t < 3000) {
    // Idle.
    in.persona = ui_v2::PersonaState::Idle;
    std::snprintf(in.msg, sizeof(in.msg), "idle");
    in.total        = g_script.heartbeats;
    in.running      = 0;
    in.waiting      = 0;
    g_script.prompt_closed = false;
  } else if(t < 6000) {
    // Busy: tokens tick up.
    if(now_ms - g_script.last_phase_tokens_stamp > 500) {
      g_script.last_phase_tokens_stamp = now_ms;
      g_script.tokens_today += 420;
      g_script.tokens_total += 420;
      g_script.heartbeats++;
    }
    in.persona = ui_v2::PersonaState::Busy;
    in.total        = g_script.heartbeats;
    in.running      = 2;
    in.waiting      = 0;
    std::snprintf(in.msg, sizeof(in.msg), "writing firmware");
  } else {
    // Prompt: CAUTION Edit, unless operator already closed it this loop.
    in.persona = ui_v2::PersonaState::Attention;
    in.total        = g_script.heartbeats;
    in.running      = 1;
    in.waiting      = 1;
    std::snprintf(in.msg, sizeof(in.msg), "awaiting approval");
    if(!g_script.prompt_closed) {
      const uint32_t loop_start_ms =
          g_script.start_ms + (since_boot / 10000) * 10000 + 6000;
      if(g_script.prompt_loop_start_ms != loop_start_ms) {
        g_script.prompt_loop_start_ms = loop_start_ms;
        std::snprintf(g_script.prompt_id, sizeof(g_script.prompt_id), "p-%lu",
                      static_cast<unsigned long>(loop_start_ms));
      }
      in.prompt_active = true;
      in.prompt_sent   = false;
      std::snprintf(in.prompt_id, sizeof(in.prompt_id), "%s", g_script.prompt_id);
      std::snprintf(in.prompt_tool, sizeof(in.prompt_tool), "Edit");
      std::snprintf(in.prompt_hint, sizeof(in.prompt_hint),
                    "modify src/ui_v2/face.cpp — adjust blink cadence to 4s");
      in.prompt_arrived_ms = loop_start_ms;
    }
  }

  // Transcript fill: always three lines that echo the scene.
  in.line_count = 3;
  std::snprintf(in.lines[0], sizeof(in.lines[0]), "heartbeat %lu",
                static_cast<unsigned long>(in.total));
  std::snprintf(in.lines[1], sizeof(in.lines[1]), "tokens today %lu",
                static_cast<unsigned long>(g_script.tokens_today));
  std::snprintf(in.lines[2], sizeof(in.lines[2]), "running %lu waiting %lu",
                static_cast<unsigned long>(in.running),
                static_cast<unsigned long>(in.waiting));
  in.line_gen = g_script.line_gen++;

  // Velocity ring: fake curve.
  in.velocity_count = 8;
  for(uint8_t i = 0; i < 8; ++i) {
    const uint32_t phase = (since_boot / 250 + i) % 10;
    in.velocity[i] = static_cast<uint16_t>(50 + phase * 35);
  }

  // Tokens & counters.
  in.tokens_today = g_script.tokens_today;
  in.tokens_total = g_script.tokens_total;
  in.token_rate_per_min = (t < 3000) ? 18 : (t < 6000 ? 280 : 96);
  in.heartbeat_age_ms = (since_boot < 2500) ? 8000 : 500;
  in.approvals    = g_script.approvals;
  in.denials      = g_script.denials;
  in.nap_seconds  = 612;
  in.level        = g_script.level;

  // Settings.
  in.brightness_level   = g_script.brightness;
  in.led_enabled        = g_script.led_enabled;
  in.transcript_enabled = g_script.transcript_enabled;
  in.demo_enabled       = g_script.demo_enabled;
  in.dock_clock_enabled = g_script.dock_clock_enabled;
  in.motion_profile     = g_script.motion_profile;
  in.face_style         = g_script.face_style;

  // RTC: present, epoch starts at 2026-04-20T10:00:00Z plus uptime.
  in.rtc_valid       = true;
  in.rtc_local_epoch = 1776996000u + (now_ms / 1000);

  // No transfer in progress.
  in.xfer_active = false;

  // Persona override: if an output asked for one, pass it back in as the
  // active persona until its expiry. (V1 does this with its own scheduler;
  // we mimic the contract here.)
  if(now_ms < g_script.persona_override_until) {
    in.persona = g_script.persona_override;
  }
}

// ----------------------------------------------------------------------------
// Outputs drain — mirror what V1 would do, but log to stdio instead.
// ----------------------------------------------------------------------------
void drain_outputs(const ui_v2::BuddyOutputs& out, uint32_t now_ms) {
  if(out.send_approve_once) {
    std::printf("[ui_v2] APPROVE sent\n");
    g_script.approvals++;
    g_script.prompt_closed = true;
  }
  if(out.send_deny) {
    std::printf("[ui_v2] DENY sent\n");
    g_script.denials++;
    g_script.prompt_closed = true;
  }
  if(out.request_nap_toggle)  std::printf("[ui_v2] request_nap_toggle\n");
  if(out.request_screen_off)  std::printf("[ui_v2] request_screen_off\n");
  if(out.request_factory_reset) std::printf("[ui_v2] request_factory_reset\n");
  if(out.request_clear_pack)    std::printf("[ui_v2] request_clear_pack\n");
  if(out.request_unpair)        std::printf("[ui_v2] request_unpair\n");
  if(out.set_character_index)   std::printf("[ui_v2] character=%u\n", out.character_index);
  if(out.dirty_persist)         std::printf("[ui_v2] dirty_persist\n");

  if(out.set_brightness) {
    g_script.brightness = out.brightness_level;
    std::printf("[ui_v2] brightness=%u\n", out.brightness_level);
  }
  if(out.set_led_enabled)    g_script.led_enabled        = out.led_enabled;
  if(out.set_transcript)     g_script.transcript_enabled = out.transcript_enabled;
  if(out.set_demo_enabled)   g_script.demo_enabled       = out.demo_enabled;
  if(out.set_dock_clock)     g_script.dock_clock_enabled = out.dock_clock_enabled;
  if(out.set_motion_profile) g_script.motion_profile     = out.motion_profile;
  if(out.set_face_style)     g_script.face_style         = out.face_style;

  if(out.set_persona) {
    g_script.persona_override       = out.persona_override;
    g_script.persona_override_until = out.persona_until_ms;
  }

  if(out.set_led_rgb) {
    if(g_script.led_enabled) {
      led.set_rgb(out.led_r, out.led_g, out.led_b);
    } else {
      led.set_rgb(0, 0, 0);
    }
  }

  if(out.set_backlight) {
    st7789.set_backlight(out.backlight_level);
  }

  (void)now_ms;
}

}  // namespace

int main() {
  stdio_init_all();

  adc_init();
  adc_gpio_init(29);

  const bool cyw43_ready = cyw43_arch_init() == 0;

  st7789.set_backlight(220);
  led.set_rgb(0, 0, 0);

  const uint32_t boot_ms = to_ms_since_boot(get_absolute_time());
  g_script.start_ms = boot_ms;

  ui_v2::core::init(&graphics, boot_ms, boot_ms);

  uint32_t next_tick_ms = boot_ms;
  std::printf("ui_v2_smoke: cyw43=%s\n", cyw43_ready ? "ok" : "failed");

  while(true) {
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    // Poll buttons every loop iteration so short taps aren't missed between
    // 40 ms ticks; edges flow into ui_core which stashes outputs.
    track_button(bt_a, button_a.raw(), now_ms);
    track_button(bt_b, button_b.raw(), now_ms);
    track_button(bt_x, button_x.raw(), now_ms);
    track_button(bt_y, button_y.raw(), now_ms);

    if(static_cast<int32_t>(now_ms - next_tick_ms) >= 0) {
      next_tick_ms += kTickIntervalMs;

      const uint16_t vsys_mv = read_vsys_mv();
      const bool usb_present = cyw43_ready
          ? (cyw43_arch_gpio_get(CYW43_WL_GPIO_VBUS_PIN) != 0) : true;

      ui_v2::BuddyInputs  in{};
      ui_v2::BuddyOutputs out{};
      fill_inputs(in, now_ms, usb_present, vsys_mv);

      ui_v2::core::tick_40ms(in, out, now_ms);

      drain_outputs(out, now_ms);
      st7789.update(&graphics);
    } else {
      // Idle a little so we don't peg the core just to poll buttons.
      sleep_us(500);
    }
  }
}
