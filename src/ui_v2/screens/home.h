// home.h — face-dominant home screen (plan §6.4).
//
// Layout (320x240):
//   Status rail  0..16   LNK + BAT + counters
//   Face band    49..97  eyes at (51, 73) / (269, 73)
//   Banner       108..126 "Running: <msg>" OR "Idle" OR "Asleep"
//   Stats row    132..148 Approvals / Denials / Level
//   Transcript   155..218 up to 4 lines, monospaced tail (if enabled)
//   Footer       226..240 A=Menu/Open* B=Pet X=Stats Y=Nap
//     *A relabels to "Open" (push Approval) when a prompt is pending;
//      "Menu" (push SystemMenu) otherwise. A-hold = Reset is suppressed
//      while a prompt is pending so a long A-press can't drop a user
//      trying to reopen Approval into the factory-reset flow.
//
// Face expression mapping (one-way; V2 overrides V1 persona):
//   Sleep     → Asleep
//   Idle      → Neutral with saccades
//   Busy      → Focused
//   Attention → Alert pulse, then Attentive
//   Celebrate → Joy (bounce)
//   Dizzy     → Dizzy
//   Heart     → Heart
//
// Buttons:
//   A short tap  → SystemMenu (Settings root), OR Approval if a prompt is pending
//   A hold       → Reset confirm (disabled while a prompt is pending)
//   B short tap  → Pet
//   X short tap  → Stats (next cycle screen)
//   Y short tap  → request nap toggle
#pragma once

#include "screen.h"

namespace ui_v2::screens::home {
const ScreenVTable* vtable();
}
