// risk_tier.cpp — classifier implementation.
//
// Comparisons are done with a small static table and an ascii_icmp helper.
// No heap, no std::string. The table is sorted by tier so adding a new tool
// is a one-line change.
//
// DESIGN NOTE (honesty): this classifier runs on the buddy, not in the
// Claude process. It does NOT see the tool's actual arguments — only the
// name. That means "Bash ls" and "Bash rm -rf /" both classify as DANGER.
// For the approval UI this is correct: if the tool's default risk is
// elevated, we want the user to read the hint, not rely on firmware
// heuristics. The hint string (prompt_hint) comes from upstream with the
// first ~96 chars of the actual command; the user sees that, not just
// the classification.
#include "risk_tier.h"

#include <cstring>

namespace ui_v2::risk {

namespace {

// Case-insensitive ASCII strcmp. Returns 0 on equal.
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

// Case-insensitive prefix match: true if `s` starts with `prefix`.
bool ascii_istarts(const char* s, const char* prefix) {
  if(!s || !prefix) return false;
  for(; *prefix; ++s, ++prefix) {
    if(!*s) return false;
    unsigned char cs = static_cast<unsigned char>(*s);
    unsigned char cp = static_cast<unsigned char>(*prefix);
    if(cs >= 'A' && cs <= 'Z') cs = static_cast<unsigned char>(cs + 32);
    if(cp >= 'A' && cp <= 'Z') cp = static_cast<unsigned char>(cp + 32);
    if(cs != cp) return false;
  }
  return true;
}

struct Entry {
  const char* name;
  RiskTier    tier;
  const char* label;
};

// Table of well-known Claude tool names. Anything not in this table falls
// through to DANGER.
constexpr Entry kTable[] = {
    // SAFE — read-only, local or web.
    {"Read",            RiskTier::Safe,    "Read file"},
    {"Glob",            RiskTier::Safe,    "Find files"},
    {"Grep",            RiskTier::Safe,    "Search text"},
    {"WebSearch",       RiskTier::Safe,    "Web search"},
    {"TaskList",        RiskTier::Safe,    "List tasks"},
    {"TaskGet",         RiskTier::Safe,    "Read task"},

    // NEUTRAL — external fetch, scheduling, or informational actions.
    {"WebFetch",        RiskTier::Neutral, "Fetch URL"},
    {"TodoWrite",       RiskTier::Neutral, "Write todos"},
    {"Skill",           RiskTier::Neutral, "Run skill"},
    {"TaskStop",        RiskTier::Neutral, "Stop task"},

    // CAUTION — local mutation.
    {"Edit",            RiskTier::Caution, "Edit file"},
    {"Write",           RiskTier::Caution, "Write file"},
    {"NotebookEdit",    RiskTier::Caution, "Edit notebook"},
    {"TaskCreate",      RiskTier::Caution, "Create task"},
    {"TaskUpdate",      RiskTier::Caution, "Update task"},

    // DANGER — execution.
    {"Bash",            RiskTier::Danger,  "Shell command"},
    {"Agent",           RiskTier::Danger,  "Spawn agent"},
    {"ToolSearch",      RiskTier::Danger,  "Load new tool"},
};

// Look up a tool name. Returns nullptr if not found.
const Entry* find(const char* tool_name) {
  if(!tool_name || !tool_name[0]) return nullptr;
  for(const Entry& e : kTable) {
    if(ascii_icmp(e.name, tool_name) == 0) return &e;
  }
  return nullptr;
}

}  // namespace

RiskTier classify(const char* tool_name) {
  if(const Entry* e = find(tool_name)) return e->tier;

  // Some tools come through with namespaces like "mcp__foo__bar" or
  // "plugin:skill". The taxonomy for these is inherently unknown to the
  // firmware, so we stay cautious — but we can recognize a few broad
  // prefixes where it's safe-ish to soften the tier. This is explicitly
  // one-way: unknowns stay DANGER.
  if(tool_name) {
    // MCP and plugin tools are almost always side-effectful but could be
    // read-only lookups. Without arguments we can't know. Keep DANGER.
    if(ascii_istarts(tool_name, "mcp__")) return RiskTier::Danger;
    if(ascii_istarts(tool_name, "plugin_")) return RiskTier::Danger;
  }
  return RiskTier::Danger;
}

void label(const char* tool_name, char* out, size_t out_size) {
  if(!out || out_size == 0) return;
  if(const Entry* e = find(tool_name)) {
    // Copy with bounded length.
    size_t i = 0;
    for(; i + 1 < out_size && e->label[i]; ++i) out[i] = e->label[i];
    out[i] = '\0';
    return;
  }
  // Unknown: show the raw tool name, truncated.
  const char* src = (tool_name && tool_name[0]) ? tool_name : "unknown";
  size_t i = 0;
  for(; i + 1 < out_size && src[i]; ++i) out[i] = src[i];
  out[i] = '\0';
}

const char* tier_name(RiskTier t) {
  switch(t) {
    case RiskTier::Safe:    return "SAFE";
    case RiskTier::Neutral: return "OK";
    case RiskTier::Caution: return "CAUTION";
    case RiskTier::Danger:  return "DANGER";
  }
  return "?";
}

}  // namespace ui_v2::risk
