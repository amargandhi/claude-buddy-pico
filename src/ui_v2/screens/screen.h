// screen.h — base interface for every V2 screen.
//
// Screens are a polymorphism-free interface: each implementation is a plain
// struct with the required functions. `ui_core` owns the stack of active
// screens as raw pointers to static instances (no heap).
//
// A Screen reads its inputs and face state and writes to outputs through
// the BuddyInputs/BuddyOutputs snapshot. It does not share state with
// other screens except through those structures or explicit handoff
// arguments passed by ui_core when pushing.
//
// Contract:
//   - enter(): called once when ui_core activates the screen.
//   - tick(in, out, face, now_ms): called every 40 ms. Screen draws and
//     may mutate `face` (e.g., begin_transition to Alert during approval).
//   - on_button(edge, btn, out): called when a button edge fires on the
//     active screen. May set request_pop_screen / request_push_screen
//     outputs via the core (the screen itself just signals intent).
//   - exit(): called once when ui_core removes the screen.
//
// Screens never block. A screen that needs to wait (e.g., a 2 s settings
// confirmation) starts a transition or schedules against now_ms.
#pragma once

#include <cstdint>

#include "../ui_state.h"

namespace ui_v2 {
struct FaceState;
}

namespace ui_v2::screens {

// Identifier space. Keep stable; persisted settings may remember which
// cycle screen is pinned.
enum class ScreenId : uint8_t {
  Boot,
  Sync,       // pairing / passkey
  Home,
  Stats,
  Usage,
  Permissions,
  Info,
  Pet,
  Buddy,
  CharacterPicker,
  // Modals / overlays.
  Approval,
  Transfer,      // character pack incoming from Mac
  Settings,
  SettingsDetail,
  Reset,
  SystemMenu,
  DockClock,
  Demo,
};

// Outputs a screen can request from ui_core. These are separate from
// BuddyOutputs (which are for V1 to act on) — these control the screen
// stack itself.
struct CoreIntent {
  bool     push     = false;
  bool     pop      = false;
  bool     replace  = false;
  ScreenId target   = ScreenId::Home;
};

// Interface. Prefer free functions over virtuals so we keep vtable out of
// flash / RAM. ui_core dispatches via a function-pointer table indexed by
// ScreenId.
struct ScreenVTable {
  void (*enter)(uint32_t now_ms);
  void (*tick)(const BuddyInputs& in, BuddyOutputs& out,
               FaceState& face, CoreIntent& intent, uint32_t now_ms);
  void (*on_button)(Button btn, ButtonEdge edge,
                    BuddyOutputs& out, CoreIntent& intent, uint32_t now_ms);
  void (*exit)(uint32_t now_ms);
};

// Resolve the vtable for a screen id. Returns nullptr if not implemented
// yet — ui_core treats that as "skip to next defined screen".
const ScreenVTable* vtable_for(ScreenId id);

}  // namespace ui_v2::screens
