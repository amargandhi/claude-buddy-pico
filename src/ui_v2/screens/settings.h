// settings.h — editable settings list (plan §6.11).
//
// Shows the boolean toggles and the brightness level. Selection moves with
// A/B; X activates (toggle / cycle brightness); Y returns.
//
// Toggling any setting marks dirty_persist so V1 schedules a flash write.
#pragma once
#include "screen.h"
namespace ui_v2::screens::settings { const ScreenVTable* vtable(); }
