// demo.h — 21-scene auto-cycle demo (plan §6.13).
//
// Runs through the complete visual grammar in ~90 s without any BLE
// traffic. This is the screen Amar wants to "put it in someone's hands
// and let them watch everything it can do."
//
// Entry: A 1 s merge_wipe from whatever screen, then scene 1.
// Exit: Y → merge_wipe → Home.
//
// Controls:
//   A: next scene (manual mode)
//   B: previous scene (manual mode)
//   X: toggle auto/manual
//   Y: exit to Home
//
// The status rail shows a DEMO n/21 chip so it's clear nothing is real.
#pragma once
#include "screen.h"
namespace ui_v2::screens::demo { const ScreenVTable* vtable(); }
