// risk_tier.h — tool-name → RiskTier classifier (plan §6.5.1).
//
// The buddy firmware sees the tool name in the approval prompt and must
// pick a color/icon/tone without any network lookup. The taxonomy is
// intentionally coarse so the user can reason about it at a glance.
//
//   SAFE    — read-only, can't change state (Read, Glob, Grep, WebSearch…)
//   NEUTRAL — external fetch or informational, no side effects (WebFetch,
//             TodoWrite, most Skill invocations)
//   CAUTION — local mutation (Edit, Write, NotebookEdit, TaskCreate)
//   DANGER  — arbitrary execution or unknown (Bash, unfamiliar tools)
//
// IMPORTANT: When unsure, we escalate — not de-escalate. An unknown tool
// is DANGER, not NEUTRAL. This is deliberate: the cost of a false green
// is a bad approval; the cost of a false red is one extra button press.
//
// The classifier also returns a short one-line tag suitable for display
// on the approval screen's tier chip (e.g., "SAFE • Read file",
// "DANGER • Shell command"). The tag is written to a caller-owned buffer
// so no allocation happens at approval time.
#pragma once

#include <cstddef>

#include "ui_state.h"

namespace ui_v2::risk {

// Classify a tool by name. `tool_name` is a NUL-terminated, ASCII-ish string
// from the upstream JSON. Case-insensitive comparison.
RiskTier classify(const char* tool_name);

// Short human-readable tag for the chip, e.g. "Read file" for Read.
// Writes at most `out_size - 1` chars plus NUL into `out`.
void label(const char* tool_name, char* out, size_t out_size);

// The tier name in upper-case, for the chip prefix. Never returns null.
const char* tier_name(RiskTier t);

}  // namespace ui_v2::risk
