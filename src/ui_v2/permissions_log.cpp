// permissions_log.cpp — ring-buffer implementation.
//
// This module owns 512 B of static RAM. The tool_id space (0..N-1) mirrors
// the order of risk_tier.cpp's kTable; we re-declare the subset of that
// table we need to resolve name/tier lookups without a circular include.
#include "permissions_log.h"

#include <cstring>

namespace ui_v2::perms {

namespace {

constexpr uint32_t kCap = 64;

// Static ring buffer.
Entry    g_ring[kCap] = {};
uint32_t g_count = 0;    // total stored, clamped at kCap
uint32_t g_head  = 0;    // next write index

// Local mirror of risk_tier.cpp's kTable. We keep this in sync manually.
// This is an intentional duplication: risk_tier.cpp's table is the source
// of truth for classify()/label(); here we only need name+tier per id.
// If a new tool is added to risk_tier.cpp it must be appended here too.
struct ToolRow {
  const char* name;
  RiskTier    tier;
};

constexpr ToolRow kTools[] = {
    // Keep in sync with risk_tier.cpp::kTable order.
    {"Read",          RiskTier::Safe},
    {"Glob",          RiskTier::Safe},
    {"Grep",          RiskTier::Safe},
    {"WebSearch",     RiskTier::Safe},
    {"TaskList",      RiskTier::Safe},
    {"TaskGet",       RiskTier::Safe},
    {"WebFetch",      RiskTier::Neutral},
    {"TodoWrite",     RiskTier::Neutral},
    {"Skill",         RiskTier::Neutral},
    {"TaskStop",      RiskTier::Neutral},
    {"Edit",          RiskTier::Caution},
    {"Write",         RiskTier::Caution},
    {"NotebookEdit",  RiskTier::Caution},
    {"TaskCreate",    RiskTier::Caution},
    {"TaskUpdate",    RiskTier::Caution},
    {"Bash",          RiskTier::Danger},
    {"Agent",         RiskTier::Danger},
    {"ToolSearch",    RiskTier::Danger},
};

constexpr uint8_t kUnknownId = 0xFF;

int ascii_icmp(const char* a, const char* b) {
  if(!a || !b) return a == b ? 0 : (a ? 1 : -1);
  for(;; ++a, ++b) {
    unsigned char ca = static_cast<unsigned char>(*a);
    unsigned char cb = static_cast<unsigned char>(*b);
    if(ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca + 32);
    if(cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb + 32);
    if(ca != cb) return static_cast<int>(ca) - static_cast<int>(cb);
    if(ca == 0) return 0;
  }
}

uint8_t tool_id_of(const char* tool_name) {
  if(!tool_name || !tool_name[0]) return kUnknownId;
  for(uint8_t i = 0; i < sizeof(kTools) / sizeof(kTools[0]); ++i) {
    if(ascii_icmp(kTools[i].name, tool_name) == 0) return i;
  }
  return kUnknownId;
}

}  // namespace

void record(const char* tool_name, Decision d, uint32_t now_ms,
            uint32_t wait_ms) {
  Entry& e = g_ring[g_head];
  e.timestamp_ms = now_ms;
  e.tool_id      = tool_id_of(tool_name);
  e.decision     = static_cast<uint8_t>(d);
  e.wait_ms      = (wait_ms > 65535u) ? 65535u : static_cast<uint16_t>(wait_ms);
  g_head = (g_head + 1) % kCap;
  if(g_count < kCap) ++g_count;
}

uint32_t count() { return g_count; }

Entry get(uint32_t i) {
  if(i >= g_count) return Entry{};
  // Newest first: index = (head - 1 - i) mod kCap.
  const uint32_t idx = (g_head + kCap - 1 - i) % kCap;
  return g_ring[idx];
}

RiskTier tier_of(uint8_t tool_id) {
  if(tool_id >= sizeof(kTools) / sizeof(kTools[0])) return RiskTier::Danger;
  return kTools[tool_id].tier;
}

const char* name_of(uint8_t tool_id) {
  if(tool_id >= sizeof(kTools) / sizeof(kTools[0])) return "(other)";
  return kTools[tool_id].name;
}

void clear() {
  std::memset(g_ring, 0, sizeof(g_ring));
  g_count = 0;
  g_head  = 0;
}

}  // namespace ui_v2::perms
