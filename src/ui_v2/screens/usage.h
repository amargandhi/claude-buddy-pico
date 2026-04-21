// usage.h — token usage screen.
//
// Shows tokens today and total, with a big central number and a mini-bar
// of today vs total. Because upstream does NOT publish a quota or rate
// limit (buddy protocol gap), we deliberately show raw totals — no
// "% of limit" trick that would imply we know the ceiling.
//
// Buttons:
//   A Menu | B Pet | X Next (Permissions) | Y Back (Stats)
#pragma once
#include "screen.h"
namespace ui_v2::screens::usage { const ScreenVTable* vtable(); }
