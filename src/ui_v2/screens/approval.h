// approval.h — tool-aware approval overlay.
//
// Visual grammar:
//   Status rail top: TIER chip on the right (SAFE / OK / CAUTION / DANGER)
//     with a color that matches the tier (green / blue / amber / red).
//   Content band:
//     - Tool label in large text, centered  ("Read file" / "Shell command")
//     - Tool name in small text below        ("Read" / "Bash")
//     - Hint box: monospaced, up to 3 wrapped rows, scrollable if long
//     - Wait-time progress bar (subtle, bottom of content area)
//   Face band: small face in top-right corner (cx=180, cy=36)
//     - Expression depends on tier:
//         SAFE    → Neutral
//         NEUTRAL → Attentive
//         CAUTION → Skeptical
//         DANGER  → Alert
//
// Ambient blink/saccade is OFF during approval — no distraction.
//
// Buttons:
//   A short tap → Approve (send_approve_once; log Approved)
//   B short tap → Deny    (send_deny;         log Denied)
//   X          → unbound — the top-right corner is intentionally blank
//                on this screen. A no-op with a visible label would train
//                users to distrust the chrome on the most security-
//                sensitive screen, and a full-screen hint viewer does
//                not yet exist. When it does, wire it here.
//   Y short tap → "Later" — pop without responding; prompt stays alive
//                upstream. Logged as Dismissed.
//
// If the prompt is revoked upstream (prompt_active goes false with no
// prior decision) the screen records a Timeout and pops.
#pragma once

#include "screen.h"

namespace ui_v2::screens::approval {
const ScreenVTable* vtable();
}
