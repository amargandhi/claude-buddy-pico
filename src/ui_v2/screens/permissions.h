// permissions.h — recent approvals/denials.
//
// Renders the permissions_log ring buffer, newest first, as a scrolling
// list. Each row shows:
//   - decision glyph (✓ / ✗ / … / –)
//   - tier color bar (4 px wide)
//   - tool name (left)
//   - wait time (right)
//   - age (right, faded)
//
// B = page down (older); Y = back (Home).
#pragma once
#include "screen.h"
namespace ui_v2::screens::permissions { const ScreenVTable* vtable(); }
