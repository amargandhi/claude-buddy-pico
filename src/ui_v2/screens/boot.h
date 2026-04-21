// boot.h — 1.5 s boot sequence (plan §6.2).
//
// Timeline (milliseconds from enter):
//     0 -  400  full-panel CASE_ORANGE, no face (merge state).
//   400 -  700  orange wipes open from center — face fades in, neutral.
//   700 - 1100  short Attentive "waking" moment with a blink.
//  1100 - 1500  "Claude Pico"/owner name chip slides up from below,
//                face settles to Neutral.
//  >= 1500      intent.replace = Home (or Sync if not paired).
#pragma once

#include "screen.h"

namespace ui_v2::screens::boot {
const ScreenVTable* vtable();
}  // namespace ui_v2::screens::boot
