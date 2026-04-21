// sync.h — pairing / passkey entry screen.
//
// Behavior:
//   - If LinkState is NotAdvertising, show a message + "Press A to advertise".
//   - If Advertising, show "Looking for Claude..." with a Scan animation.
//   - If PairingPasskey, show the 6-digit passkey BIG in the center.
//   - If Connecting / LinkedInsecure, quick transient state.
//   - If LinkedSecure, trigger a merge_wipe and replace with Home.
//
// Buttons:
//   - A: start/stop advertising (only when disconnected)
//   - Y: skip to Home (for demo / development; prod build can hide)
#pragma once

#include "screen.h"

namespace ui_v2::screens::sync {
const ScreenVTable* vtable();
}
