// info.h — device info screen.
//
// Device name, owner, firmware version, uptime, battery raw mV, RTC status.
// Static content; the only dynamic values are uptime and clock.
#pragma once
#include "screen.h"
namespace ui_v2::screens::info { const ScreenVTable* vtable(); }
