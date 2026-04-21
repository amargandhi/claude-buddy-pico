// ui_core.cpp — dispatcher implementation.
#include "ui_core.h"

#include "face.h"
#include "geometry.h"
#include "gfx.h"
#include "palette.h"
#include "screens/screen.h"

namespace ui_v2::core {

namespace {

constexpr uint8_t kStackMax = 8;

screens::ScreenId g_stack[kStackMax] = {};
uint8_t           g_depth = 0;
FaceState         g_face;
bool              g_enter_pending = true;

// Outputs produced by on_button_event are held here until the next tick
// drains them. Every field is initialized to BuddyOutputs' defaults.
BuddyOutputs      g_pending{};

// Rising-edge latches for globally-triggered modals (approval prompts,
// character pack transfers). Without these, a modal only opens if the
// *current* screen happens to check for it — which meant Approval only
// worked when the user was on Home. Centralizing the detection here
// makes the modal layer screen-agnostic: no matter what screen is on
// top, an incoming prompt or pack transfer takes the stack. We also
// suppress the auto-push when the relevant modal is already on top,
// which otherwise self-cycles on an unresponsive host.
bool g_prev_prompt_active = false;
bool g_prev_xfer_active   = false;
enum class UsbEventKind : uint8_t {
  None,
  Plug,
  Unplug,
};
UsbEventKind g_usb_event_kind = UsbEventKind::None;
uint32_t g_usb_event_ms   = 0;
uint16_t g_prev_power_event_seq = 0;

void draw_usb_event_overlay(uint32_t now_ms) {
  if(g_usb_event_kind == UsbEventKind::None || g_usb_event_ms == 0) return;
  const uint32_t age = now_ms - g_usb_event_ms;
  if(age >= 560) {
    g_usb_event_kind = UsbEventKind::None;
    g_usb_event_ms = 0;
    return;
  }

  gfx::fill_rect(0, 0, geom::kPanelW, geom::kPanelH, kCaseOrange);
  if(g_usb_event_kind == UsbEventKind::Unplug) {
    gfx::fill_rect(0, 0, geom::kPanelW, 3, kAccentClayDim);
  } else {
    gfx::fill_rect(0, geom::kPanelH - 3, geom::kPanelW, 3, kTextWarm);
  }

  FaceState overlay = g_face;
  face::set_base(overlay, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kEyeCy);
  face::set_ambient(overlay, false, false);
  face::set_ink(overlay, kBgInkDeep);
  face::snap(overlay,
             g_usb_event_kind == UsbEventKind::Plug
                 ? ExpressionId::GlyphCircle
                 : ExpressionId::GlyphDash,
             now_ms);
  face::render_only(overlay, now_ms);

  gfx::text_aligned(g_usb_event_kind == UsbEventKind::Plug ? "USB CONNECTED"
                                                           : "USB REMOVED",
                    0, 150, geom::kPanelW, gfx::Align::Center,
                    kBgInkDeep, 1.4f);
  gfx::text_aligned(g_usb_event_kind == UsbEventKind::Plug
                        ? "Power detected on the dock"
                        : "Running on battery",
                    0, 172, geom::kPanelW, gfx::Align::Center,
                    kBgInkDeep, 1.0f);
}

void push(screens::ScreenId id) {
  if(g_depth >= kStackMax) {
    // Drop deepest to make room — never block.
    for(uint8_t i = 1; i < kStackMax; ++i) g_stack[i - 1] = g_stack[i];
    --g_depth;
  }
  g_stack[g_depth++] = id;
  g_enter_pending = true;
}

void call_exit(screens::ScreenId id, uint32_t now_ms) {
  const screens::ScreenVTable* vt = screens::vtable_for(id);
  if(vt && vt->exit) vt->exit(now_ms);
}

void pop(uint32_t now_ms) {
  if(g_depth > 1) {
    call_exit(g_stack[g_depth - 1], now_ms);
    --g_depth;
    g_enter_pending = true;
  }
}

void replace(screens::ScreenId id, uint32_t now_ms) {
  if(g_depth == 0) g_stack[g_depth++] = id;
  else {
    call_exit(g_stack[g_depth - 1], now_ms);
    g_stack[g_depth - 1] = id;
  }
  g_enter_pending = true;
}

screens::ScreenId top() {
  return g_depth ? g_stack[g_depth - 1] : screens::ScreenId::Home;
}

// Merge `src` into `dst` using idempotent rules. Boolean "request" flags
// are OR-ed (latch once, V1 clears next tick). Parametric settings are
// overwritten by whichever call was most recent.
void merge_outputs(BuddyOutputs& dst, const BuddyOutputs& src) {
  dst.send_approve_once   |= src.send_approve_once;
  dst.send_deny           |= src.send_deny;
  dst.request_nap_toggle  |= src.request_nap_toggle;
  dst.request_screen_off  |= src.request_screen_off;
  if(src.set_advertising) { dst.set_advertising = true; dst.advertising_enabled = src.advertising_enabled; }
  dst.request_clear_stats   |= src.request_clear_stats;
  dst.request_factory_reset |= src.request_factory_reset;
  dst.request_clear_pack    |= src.request_clear_pack;
  dst.request_unpair        |= src.request_unpair;
  dst.request_pet_feed      |= src.request_pet_feed;
  dst.request_pet_play      |= src.request_pet_play;
  dst.dirty_persist         |= src.dirty_persist;

  if(src.set_brightness)    { dst.set_brightness    = true; dst.brightness_level   = src.brightness_level; }
  if(src.set_led_enabled)   { dst.set_led_enabled   = true; dst.led_enabled        = src.led_enabled; }
  if(src.set_transcript)    { dst.set_transcript    = true; dst.transcript_enabled = src.transcript_enabled; }
  if(src.set_demo_enabled)  { dst.set_demo_enabled  = true; dst.demo_enabled       = src.demo_enabled; }
  if(src.set_dock_clock)    { dst.set_dock_clock    = true; dst.dock_clock_enabled = src.dock_clock_enabled; }
  if(src.set_motion_profile) { dst.set_motion_profile = true; dst.motion_profile = src.motion_profile; }
  if(src.set_face_style)     { dst.set_face_style = true; dst.face_style = src.face_style; }
  if(src.set_character_index) { dst.set_character_index = true; dst.character_index = src.character_index; }
  if(src.set_persona)       { dst.set_persona       = true; dst.persona_override   = src.persona_override;
                              dst.persona_until_ms  = src.persona_until_ms; }
  if(src.set_led_rgb)       { dst.set_led_rgb       = true;
                              dst.led_r = src.led_r; dst.led_g = src.led_g; dst.led_b = src.led_b; }
  if(src.set_backlight)     { dst.set_backlight     = true; dst.backlight_level    = src.backlight_level; }
}

}  // namespace

void init(pimoroni::PicoGraphics* gfx_ptr, uint32_t initial_seed,
          uint32_t now_ms) {
  gfx::bind(gfx_ptr);
  face::seed(initial_seed ? initial_seed : 0xA55A1234u);

  face::set_base(g_face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kEyeCy);
  face::set_ink(g_face, kTextPrimary);
  face::snap(g_face, ExpressionId::Neutral, now_ms);

  g_depth = 0;
  g_stack[g_depth++] = screens::ScreenId::Boot;
  g_enter_pending = true;
  g_pending = BuddyOutputs{};
  g_prev_prompt_active = false;
  g_prev_xfer_active   = false;
  g_usb_event_kind     = UsbEventKind::None;
  g_usb_event_ms       = 0;
  g_prev_power_event_seq = 0;
}

void on_button_event(Button btn, ButtonEdge edge, uint32_t now_ms) {
  const screens::ScreenVTable* vt = screens::vtable_for(top());
  if(!vt || !vt->on_button) return;

  BuddyOutputs local{};
  screens::CoreIntent intent{};
  vt->on_button(btn, edge, local, intent, now_ms);

  // Stash outputs for the next tick to drain.
  merge_outputs(g_pending, local);

  // Screen-stack intents happen immediately.
  if(intent.pop)     pop(now_ms);
  if(intent.replace) replace(intent.target, now_ms);
  if(intent.push)    push(intent.target);
}

void tick_40ms(const BuddyInputs& in, BuddyOutputs& out, uint32_t now_ms) {
  // Drain pending outputs from prior on_button_event into this frame's out.
  merge_outputs(out, g_pending);
  g_pending = BuddyOutputs{};

  // Global modal auto-push. Rising edge only — we never re-push while the
  // same prompt/transfer is still ongoing, and we never push while the
  // relevant modal is already top-of-stack (which would happen if the
  // boot screen is still active when the first prompt arrives and the
  // edge hasn't advanced). Prompt goes first so that if V1 ever starts
  // a transfer simultaneously with a prompt (which shouldn't happen, but
  // we don't crash if it does), the user is asked to approve before we
  // interrupt them with progress UI.
  const bool prompt_now = in.prompt_active && !in.prompt_sent;
  if(prompt_now && !g_prev_prompt_active && top() != screens::ScreenId::Approval) {
    if(top() == screens::ScreenId::Reset) replace(screens::ScreenId::Approval, now_ms);
    else                                  push(screens::ScreenId::Approval);
  }
  if(in.xfer_active && !g_prev_xfer_active && top() != screens::ScreenId::Transfer) {
    push(screens::ScreenId::Transfer);
  }
  g_prev_prompt_active = prompt_now;
  g_prev_xfer_active   = in.xfer_active;
  if(in.power_event_seq != g_prev_power_event_seq) {
    if(in.power_event == PowerEvent::UsbPlug) {
      g_usb_event_kind = UsbEventKind::Plug;
      g_usb_event_ms = now_ms;
    } else if(in.power_event == PowerEvent::UsbUnplug) {
      g_usb_event_kind = UsbEventKind::Unplug;
      g_usb_event_ms = now_ms;
    }
    g_prev_power_event_seq = in.power_event_seq;
  }

  const screens::ScreenId id = top();
  const screens::ScreenVTable* vt = screens::vtable_for(id);
  if(!vt || !vt->tick) {
    gfx::clear(kBgInk);
    gfx::text_aligned("Screen not implemented", 0, 100, geom::kPanelW,
                      gfx::Align::Center, kDangerRed, 1.5f);
    gfx::commit();
    pop(now_ms);
    return;
  }

  if(g_enter_pending) {
    g_enter_pending = false;
    // Reset face base before handing off; screens that want a different
    // position (approval, dock_clock) re-move it in their own tick.
    face::set_base(g_face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kEyeCy);
    face::set_ink(g_face, kTextPrimary);
    face::set_ambient(g_face, true, true);
    if(vt->enter) vt->enter(now_ms);
  }
  face::set_motion_profile(g_face, in.motion_profile);

  screens::CoreIntent intent{};
  vt->tick(in, out, g_face, intent, now_ms);

  if(id != screens::ScreenId::Boot && id != screens::ScreenId::Approval) {
    draw_usb_event_overlay(now_ms);
  }

  if(intent.pop)     pop(now_ms);
  if(intent.replace) replace(intent.target, now_ms);
  if(intent.push)    push(intent.target);

  gfx::commit();
}

FaceState& face_state() { return g_face; }

screens::ScreenId current_screen() { return top(); }

}  // namespace ui_v2::core
