// stats.h — session/lifetime stats screen.
//
// A data-forward screen with live activity + counters. No face — full area
// is numbers.
//
// Layout:
//   Left column  : TOTAL | RUNNING | WAITING | TOKENS today/total
//   Right column : Live rate + heartbeat age + APPROVE/DENY bars
//
// Buttons:
//   A Menu | B Pet | X Next (Usage) | Y Back (Home)
#pragma once
#include "screen.h"
namespace ui_v2::screens::stats { const ScreenVTable* vtable(); }
