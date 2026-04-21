// dock_clock.h — "sitting on the desk" display (plan §6.12).
//
// When USB-powered and idle, draw a calm clock surface:
//   - Big HH:MM in the center
//   - Date row below (if RTC valid)
//   - Owner name or "Claude Pico" at top
//   - One ambient blink every ~20 s
//   - Saccades disabled (too busy for desk-glance)
//
// If the pico is battery-powered, show a warning and fall back to Home.
#pragma once
#include "screen.h"
namespace ui_v2::screens::dock_clock { const ScreenVTable* vtable(); }
