# V2 UI — integrating into the V1 monolith

The V2 UI (`src/ui_v2/`) is a standalone module. `src/claude_buddy_pico.cpp` — the V1 monolith — is untouched and continues to drive the device the way it always has. This doc is the recipe for opting V1 into V2 without rewriting it.

**Nothing in this doc is required to run V2.** The `claude_buddy_pico_v2_smoke` target already boots the full V2 UI against synthetic BuddyInputs on real hardware. Follow this doc only when you want the production firmware (BLE + flash + persist) to render V2 instead of V1's screens.

---

## Design contract

V2 never reads V1's `BuddyState` struct. V1 fills a read-only `BuddyInputs` snapshot once per 40 ms tick, passes it into `ui_v2::core::tick_40ms()`, and drains the resulting `BuddyOutputs` afterward. That snapshot is the only coupling surface. If the snapshot compiles, the integration is complete.

V1 remains authoritative for:

- BLE advertising, pairing, passkey, and secure-connection state
- NUS protocol parsing (prompts, heartbeat, RTC, character pack transfer)
- Flash persistence (counters, settings, pack, RTC offset)
- Battery sampling and USB-VBUS sensing
- Actual send-over-NUS / write-to-flash / LED / backlight side effects

V2 owns:

- Every pixel rendered to `PicoGraphics` once V1 hands off rendering
- All screen-stack navigation and button dispatch during V2 mode
- The single `FaceState` and the full expression library

---

## Build-time switch

`CMakeLists.txt` already defines `BUDDY_UI_V2=1` on the smoke target only. To opt the main firmware target in, add the compile definition to the existing `claude_buddy_pico` executable and link in `${UI_V2_SOURCES}`:

```cmake
# Inside the existing claude_buddy_pico block — not a second target.
target_sources(claude_buddy_pico PRIVATE ${UI_V2_SOURCES})
target_compile_definitions(claude_buddy_pico PRIVATE BUDDY_UI_V2=1)
```

Every hook in V1 is then wrapped in `#if BUDDY_UI_V2 ... #else ... #endif` so the V1-only path remains buildable by flipping the flag back off. The smoke target is unaffected — it already defines `BUDDY_UI_V2=1` directly.

---

## Hook sites in `claude_buddy_pico.cpp`

There are exactly four hooks. All of them sit near code that already exists — no file reshuffling required.

### 1. Include the dispatcher

At the top of the includes block, next to the existing `"buddy.h"`:

```cpp
#if BUDDY_UI_V2
#include "ui_v2/ui_core.h"
#include "ui_v2/ui_state.h"
#endif
```

### 2. Initialize once, after `graphics` exists

V1 constructs `PicoGraphics_PenRGB332 graphics(...)` at file scope and brings up `st7789` in `main()`. After the display backlight is set but before the main loop begins:

```cpp
#if BUDDY_UI_V2
ui_v2::core::init(&graphics, time_us_32(),
                  to_ms_since_boot(get_absolute_time()));
#endif
```

Seed can be anything non-zero — `time_us_32()` is convenient and already drifts between boots.

### 3. Forward button edges

V1's `handle_buttons()` already computes `a_pressed` / `a_held` / `b_pressed` / `x_pressed` / `x_held` / `y_pressed` / `y_held` as edges.

Keep the existing `screen_off` and `napping` wake branches exactly where they are today. Those branches intentionally consume the first wake press and return; V2 should not see those wake-only inputs.

Add the V2 dispatch block after those wake guards but before V1's prompt/menu/settings/reset/display-mode handling:

```cpp
#if BUDDY_UI_V2
const uint32_t now_ms_u32 = to_ms_since_boot(get_absolute_time());
// Down/Up edges are useful for holds; derive from last_* transitions.
if(a && !state.last_a) ui_v2::core::on_button_event(ui_v2::Button::A, ui_v2::ButtonEdge::Down, now_ms_u32);
if(b && !state.last_b) ui_v2::core::on_button_event(ui_v2::Button::B, ui_v2::ButtonEdge::Down, now_ms_u32);
if(x && !state.last_x) ui_v2::core::on_button_event(ui_v2::Button::X, ui_v2::ButtonEdge::Down, now_ms_u32);
if(y && !state.last_y) ui_v2::core::on_button_event(ui_v2::Button::Y, ui_v2::ButtonEdge::Down, now_ms_u32);

if(!a && state.last_a) ui_v2::core::on_button_event(ui_v2::Button::A, ui_v2::ButtonEdge::Up, now_ms_u32);
if(!b && state.last_b) ui_v2::core::on_button_event(ui_v2::Button::B, ui_v2::ButtonEdge::Up, now_ms_u32);
if(!x && state.last_x) ui_v2::core::on_button_event(ui_v2::Button::X, ui_v2::ButtonEdge::Up, now_ms_u32);
if(!y && state.last_y) ui_v2::core::on_button_event(ui_v2::Button::Y, ui_v2::ButtonEdge::Up, now_ms_u32);

if(a_pressed) ui_v2::core::on_button_event(ui_v2::Button::A, ui_v2::ButtonEdge::ShortTap,  now_ms_u32);
if(a_held)    ui_v2::core::on_button_event(ui_v2::Button::A, ui_v2::ButtonEdge::HoldFired, now_ms_u32);
if(b_pressed) ui_v2::core::on_button_event(ui_v2::Button::B, ui_v2::ButtonEdge::ShortTap,  now_ms_u32);
if(x_pressed) ui_v2::core::on_button_event(ui_v2::Button::X, ui_v2::ButtonEdge::ShortTap,  now_ms_u32);
if(x_held)    ui_v2::core::on_button_event(ui_v2::Button::X, ui_v2::ButtonEdge::HoldFired, now_ms_u32);
if(y_pressed) ui_v2::core::on_button_event(ui_v2::Button::Y, ui_v2::ButtonEdge::ShortTap,  now_ms_u32);
if(y_held)    ui_v2::core::on_button_event(ui_v2::Button::Y, ui_v2::ButtonEdge::HoldFired, now_ms_u32);

// Preserve V1's screen_off / nap wake semantics above this block. Once we
// reach here, V2 owns the remaining button behavior for the frame.
state.last_a = a; state.last_b = b; state.last_x = x; state.last_y = y;
return;
#endif
```

The `return` at the end short-circuits V1's prompt/menu/settings/reset/display-mode dispatch. Keep the V1 body below the `#endif` so the flag can flip back to off.

### 4. Replace the V1 render path in `ui_timer_handler`

V1's tick handler today draws a face, composites UI chrome, pushes pixels via `st7789.update(&graphics)`, and dispatches any latched actions. Wrap the body:

```cpp
static void ui_timer_handler(async_context_t*, async_at_time_worker_t*) {
#if BUDDY_UI_V2
  const uint32_t now_ms = to_ms_since_boot(get_absolute_time());

  ui_v2::BuddyInputs in{};
  populate_inputs(in, now_ms);   // see §4 below

  ui_v2::BuddyOutputs out{};
  ui_v2::core::tick_40ms(in, out, now_ms);

  drain_outputs(out, now_ms);     // see §5 below

  st7789.update(&graphics);
#else
  // ... existing V1 body unchanged ...
#endif

  reschedule_ui_timer();
}
```

---

## 4. `populate_inputs` — mapping V1 state into the snapshot

Add this as a file-static helper right above `ui_timer_handler`. This is a translation layer, not a blind field rename. The snippet below uses the current field names from `src/claude_buddy_pico.cpp`.

```cpp
#if BUDDY_UI_V2
static void populate_inputs(ui_v2::BuddyInputs& in, uint32_t now_ms) {
  in.now_ms = now_ms;

  std::snprintf(in.device_name, sizeof(in.device_name), "%s", state.device_name);
  std::snprintf(in.owner,       sizeof(in.owner),       "%s", state.owner);

  // Link state — mapped from today's V1 flags. The current monolith does not
  // expose a separate "Connecting" state, so V2 leaves that enum unused.
  if(!state.link_connected) {
    in.link = state.advertising ? ui_v2::LinkState::Advertising
                                : ui_v2::LinkState::NotAdvertising;
  } else if(!state.secure && state.passkey[0] != '\0') {
    in.link = ui_v2::LinkState::PairingPasskey;
  } else if(state.secure) {
    in.link = ui_v2::LinkState::LinkedSecure;
  } else {
    in.link = ui_v2::LinkState::LinkedInsecure;
  }
  std::snprintf(in.passkey, sizeof(in.passkey), "%s", state.passkey);

  // Power.
  // V1 currently exposes USB presence but not a separate charging signal.
  in.power = state.usb_present ? ui_v2::PowerState::UsbPowered
                               : ui_v2::PowerState::Battery;
  in.battery_pct = state.battery_percent;
  in.battery_mv  = state.battery_mv;
  in.battery_low = state.battery_low;

  // Protocol / heartbeat.
  in.total        = state.total;
  in.running      = state.running;
  in.waiting      = state.waiting;
  std::snprintf(in.msg, sizeof(in.msg), "%s", state.msg);
  in.tokens_today = state.tokens_today;
  in.tokens_total = state.tokens_total;

  // Transcript.
  in.line_count = state.line_count;
  for(uint8_t i = 0; i < in.line_count && i < 8; ++i) {
    std::snprintf(in.lines[i], sizeof(in.lines[i]), "%s", state.lines[i]);
  }
  in.line_gen = state.line_gen;

  // Do not bridge V1's existing `velocity` ring here. Today it stores prompt
  // response latency in seconds, not tokens/min usage samples. Until V1 gains
  // a real usage-rate series, leave V2's usage sparkline empty.
  in.velocity_count = 0;
  std::memset(in.velocity, 0, sizeof(in.velocity));

  // Prompt (permission request).
  in.prompt_active = state.prompt_active;
  in.prompt_sent   = state.response_sent;
  std::snprintf(in.prompt_id,   sizeof(in.prompt_id),   "%s", state.prompt_id);
  std::snprintf(in.prompt_tool, sizeof(in.prompt_tool), "%s", state.prompt_tool);
  std::snprintf(in.prompt_hint, sizeof(in.prompt_hint), "%s", state.prompt_hint);
  in.prompt_arrived_ms = state.prompt_arrived_ms;

  // Persona & persisted counters.
  in.persona     = static_cast<ui_v2::PersonaState>(state.active_state);
  in.approvals   = state.approvals;
  in.denials     = state.denials;
  in.nap_seconds = state.nap_seconds;
  in.level       = state.level;

  // Settings.
  in.brightness_level   = state.brightness_level;
  in.led_enabled        = state.led_enabled;
  in.transcript_enabled = state.transcript_enabled;
  in.demo_enabled       = state.demo_mode;
  in.dock_clock_enabled = state.clock_enabled;
  in.face_animated      = state.face_animated;

  // RTC.
  in.rtc_valid       = state.rtc_valid;
  in.rtc_local_epoch = state.rtc_valid ? current_local_epoch() : 0;

  // Character pack transfer.
  in.xfer_active  = state.xfer_active;
  in.xfer_written = state.xfer_total_written;
  in.xfer_total   = state.xfer_total;
}
#endif
```

Notes:

- `state.face_animated` does not exist in V1 yet. Add it alongside the other persisted settings and include it in `PersistentBlob` before wiring this field.
- `ui_v2::LinkState::Connecting` and `ui_v2::PowerState::UsbCharging` remain available for a future V1 refactor, but today's monolith does not surface those distinctions.
- `current_local_epoch()` already exists in the monolith and should be reused so V2 inherits the same "time since sync" math as V1's dock clock.

---

## 5. `drain_outputs` — routing intents back into V1 side effects

Also above `ui_timer_handler`. Route V2 outputs into the actual current V1 helpers and state mutations:

```cpp
#if BUDDY_UI_V2
static void drain_outputs(const ui_v2::BuddyOutputs& out, uint32_t now_ms) {
  if(out.send_approve_once) handle_prompt_decision("once");
  if(out.send_deny)         handle_prompt_decision("deny");

  if(out.request_nap_toggle) toggle_nap();
  if(out.request_screen_off) state.screen_off = true;

  if(out.request_clear_pack) {
    delete_custom_pack();
    set_ack("Deleted character");
  }

  if(out.request_factory_reset) {
    if(state.connection_handle != HCI_CON_HANDLE_INVALID) {
      gap_delete_bonding(state.peer_addr_type, state.peer_addr);
    }
    state.paired = false;
    state.secure = false;
    state.security_level = 0;
    factory_reset_state();
    set_ack("Factory reset");
  }

  if(out.request_unpair) {
    if(state.connection_handle != HCI_CON_HANDLE_INVALID) {
      gap_delete_bonding(state.peer_addr_type, state.peer_addr);
    }
    state.paired = false;
    state.secure = false;
    state.security_level = 0;
    set_ack("Bond cleared");
  }

  if(out.set_persona) {
    const uint32_t duration_ms =
        out.persona_until_ms > now_ms ? (out.persona_until_ms - now_ms) : 0;
    trigger_one_shot(static_cast<PersonaState>(out.persona_override), duration_ms);
  }

  bool persist_now = false;
  if(out.set_brightness)    { state.brightness_level   = out.brightness_level;   persist_now = true; }
  if(out.set_led_enabled)   { state.led_enabled        = out.led_enabled;        persist_now = true; }
  if(out.set_transcript)    { state.transcript_enabled = out.transcript_enabled; persist_now = true; }
  if(out.set_demo_enabled)  { state.demo_mode          = out.demo_enabled;       persist_now = true; }
  if(out.set_dock_clock)    { state.clock_enabled      = out.dock_clock_enabled; persist_now = true; }
  if(out.set_face_animated) { state.face_animated      = out.face_animated;      persist_now = true; }
  if(persist_now) {
    persist_state();
  } else if(out.dirty_persist) {
    schedule_persist();
  }

  if(out.set_led_rgb) {
    if(state.led_enabled) led.set_rgb(out.led_r, out.led_g, out.led_b);
    else                  led.set_rgb(0, 0, 0);
  }
  if(out.set_backlight) st7789.set_backlight(out.backlight_level);

  (void)now_ms;
}
#endif
```

Two important caveats:

- The current monolith does not have helpers named `send_permission_response`, `enter_screen_off`, `schedule_factory_reset`, `clear_character_pack`, `request_ble_unpair`, or a `state.settings_dirty` flag. Use the real helpers above, or extract small statics from `apply_menu_action()`, `apply_reset()`, and the `cmd:"unpair"` handler if you want to keep `drain_outputs()` tidy.
- V1 settings currently call `persist_state()` immediately from `apply_setting()`, while counters and other deferred writes use `schedule_persist()`. Mirror that split instead of inventing a new debounce flag in the integration layer.

---

## What stays, what goes

With `BUDDY_UI_V2=1`, V1 no longer draws a face or composites screen chrome. The V1 functions that become dead under the flag:

- `draw_face_*`, `draw_*_screen`, `composite_overlay`, and any palette helpers
- persona → expression mapping
- screen-stack bookkeeping inside V1
- anything in `handle_buttons` after the V2 dispatch block

Delete nothing. Wrap each V1-only block in `#if !BUDDY_UI_V2 ... #endif` (the complement of the V2 branch) so flipping the flag returns to today's firmware with no git-archaeology required.

Functions that **stay live regardless of the flag**:

- every BLE / BTstack callback
- every NUS packet parser
- `persist_save` / `persist_load` / the flash bank bookkeeping
- battery sampling and USB-VBUS reads
- the async timer plumbing itself
- nap entry/exit and screen_off wake

---

## Verification checklist (order matters)

1. Build with the flag off. Should produce byte-identical firmware to today.
2. Build with the flag on. Any link errors point at a V2 helper you haven't wrapped with the V1-side hook — fix by wrapping, not by rewriting V2.
3. Flash, pair. Boot screen should last ~1.5 s then hand off to Sync (first boot) or Home.
4. From Home, press A → SystemMenu appears. Y → back to Home. Every navigation path in the plan doc §4 should be reachable.
5. Trigger a prompt from the host. Approval screen should appear with a tier strip matching `docs/new-firmware-ui-plan.md` §11.3. A sends approve; B sends deny; Y dismisses (no NUS send). Confirm heartbeat `approvals` / `denials` counters in Stats increment only on A/B, not on Y.
6. Settings → Face animated → off. Eyes should snap to their target expression and stop blinking. Turn back on. Cycle power. The setting should survive.
7. Factory reset via System Menu → Reset → A-hold twice. Observe merge animation, then Home with counters at zero.

If all seven pass, the V2 integration is load-bearing.
