// system_menu.h — root menu pushed from A on any cycle screen (plan §6.11).
//
// A vertical list of choices. Selection moves with A (prev) and B (next).
// X activates; Y returns. Entries may open other screens (Settings,
// DockClock, Demo, Reset) or fire one-shot actions (Unpair, Sleep now).
#pragma once
#include "screen.h"
namespace ui_v2::screens::system_menu { const ScreenVTable* vtable(); }
