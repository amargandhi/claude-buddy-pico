// reset.h — destructive action confirmation.
//
// Two destructive actions funnel through this screen:
//   - Clear Stats (approvals/denials/level/nap/tokens)
//   - Factory Reset (all persisted state + unpair)
//
// The first hold-A arms the action and shows a 3-second countdown. A
// second A hold actually fires. Any other button cancels.
#pragma once
#include "screen.h"
namespace ui_v2::screens::reset { const ScreenVTable* vtable(); }
namespace ui_v2::screens::reset {

enum class Mode : uint8_t {
  ClearStats,
  FactoryReset,
};

// Call before pushing/replacing the Reset screen so the confirmation copy and
// emitted output match the destructive action the user actually picked.
void prepare(Mode mode);

}  // namespace ui_v2::screens::reset
