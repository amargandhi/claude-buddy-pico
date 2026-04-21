// ui_core.h — V2 dispatcher (the only surface V1 calls into).
//
// V1 (the monolith) calls four things per 40 ms cycle:
//
//   ui_v2::core::init(gfx_ptr, now_ms);           // once at boot
//   ui_v2::core::on_button_event(btn, edge, now); // per button event
//   ui_v2::core::tick_40ms(in, out, now_ms);      // once per frame
//
// `in` is the BuddyInputs snapshot V1 fills before the call; `out` is
// drained by V1 after.
//
// The core owns:
//   - A fixed-size screen stack (8 deep, no heap).
//   - A single FaceState.
//   - The global gfx binding (done once in init).
#pragma once

#include <cstdint>

#include "face.h"
#include "ui_state.h"
#include "screens/screen.h"

namespace pimoroni { class PicoGraphics; }

namespace ui_v2::core {

// Initialize the UI. Must be called before tick_40ms().
//   gfx_ptr       — PicoGraphics to render through (V1 owns the panel)
//   initial_seed  — used to seed the blink/saccade RNG (clock_get_us or similar)
void init(pimoroni::PicoGraphics* gfx_ptr, uint32_t initial_seed,
          uint32_t now_ms);

// Deliver a button edge event. Should be called from V1's button polling
// at whatever cadence V1 currently uses.
void on_button_event(Button btn, ButtonEdge edge, uint32_t now_ms);

// Drive the UI for one 40 ms tick.
void tick_40ms(const BuddyInputs& in, BuddyOutputs& out, uint32_t now_ms);

// Access the face state (for logging/debug only; screens receive it via tick).
FaceState& face_state();

// Current topmost screen (debugging).
screens::ScreenId current_screen();

}  // namespace ui_v2::core
