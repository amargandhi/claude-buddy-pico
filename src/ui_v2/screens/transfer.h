// transfer.h — character-pack transfer progress overlay.
//
// Shown automatically whenever V1 reports an in-flight pack transfer
// (BuddyInputs.xfer_active). Pops as soon as V1 reports the transfer has
// finished (xfer_active goes false) — regardless of whether that ending
// was a completion, a disconnect, or a timeout. V1 owns interpretation;
// V2 just shows what's happening.
//
// Visual grammar:
//   Face band: small Attentive face in top-right (cx=180, cy=36), same
//     placement grammar as Approval so the user's eye knows "this is a
//     modal about something the Mac is doing right now."
//   Content band:
//     - Title: "Receiving character" (scale 2.2, kTextPrimary)
//     - Subtitle: "%u / %u bytes" (scale 1.3, kTextDim)
//     - A single accent-clay progress bar, generously tall (12 px) so it
//       reads across the desk. We do NOT show a spinner or percentage
//       overlay — the bar is the percentage. Two readings of the same
//       number is chrome noise.
//   No button hints in the rail or footer. A transfer is non-cancellable
//   from the Pico side — advertising that "Y cancels" would be a lie, and
//   showing a button we can't honor is the exact failure mode we removed
//   from the Approval X-hint. The rail shows LNK + BAT unchanged so the
//   user can still see connection/power state.
//
// If the transfer ends while the screen is up, we linger for 800 ms with
// either "Saved." (kOkGreen) or "Disconnected." (kDangerRed) depending on
// link state at exit, then pop. Without that linger the screen flashes
// off the instant V1 flips xfer_active, and the user loses the feedback
// that the action they triggered actually landed.
#pragma once

#include "screen.h"

namespace ui_v2::screens::transfer {
const ScreenVTable* vtable();
}
