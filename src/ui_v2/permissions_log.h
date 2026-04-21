// permissions_log.h — in-RAM ring buffer of recent approvals/denials.
//
// The Permissions screen shows the most recent 64 decisions,
// newest first, with tool name, decision (APPROVE / DENY / TIMEOUT),
// tier, and wait-time-to-decision. The buffer is RAM-only; it does NOT
// persist across reboots, which is intentional: this is a live-session
// log for the current owner, not an audit trail.
//
//   64 entries × 8 B = 512 B total
//
// Entry layout (packed, 8 B):
//   uint32_t timestamp_ms    // relative to boot; wraps at ~49 days
//   uint8_t  tool_id         // index into risk::kTable; 0xFF = unknown/other
//   uint8_t  decision        // enum Decision
//   uint16_t wait_ms         // clamped to 65535
//
// The tool name is stored by index so we don't need a char* per entry.
// For unknown tools we display "(other)" in the UI.
//
// The permissions screen reads entries via `get(i)` where 0 is newest.
// `count()` returns how many valid entries exist (0..64).
#pragma once

#include <cstdint>

#include "ui_state.h"

namespace ui_v2::perms {

enum class Decision : uint8_t {
  Approved = 0,
  Denied   = 1,
  Timeout  = 2,
  Dismissed = 3,
};

struct Entry {
  uint32_t timestamp_ms;
  uint8_t  tool_id;    // 0xFF = unknown
  uint8_t  decision;   // Decision cast to u8
  uint16_t wait_ms;
};

// Record a decision. Called by the approval screen when the user lets go
// of the button (or the prompt times out). `tool_name` is matched against
// the risk_tier table to produce a tool_id.
void record(const char* tool_name, Decision d, uint32_t now_ms,
            uint32_t wait_ms);

// Number of valid entries in the log (0..64).
uint32_t count();

// Return the i-th entry, newest first (i=0). Returns a default-initialized
// entry if out of range.
Entry get(uint32_t i);

// Tier of the tool at `tool_id`. Returns DANGER for 0xFF / unknown.
RiskTier tier_of(uint8_t tool_id);

// Human-readable tool name for the given tool_id. "(other)" for unknown.
const char* name_of(uint8_t tool_id);

// Clear the log. Called from Reset > Clear stats.
void clear();

}  // namespace ui_v2::perms
