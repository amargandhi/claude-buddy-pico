// vtable_registry.cpp — resolves ScreenId → ScreenVTable*.
//
// Lives with the screens so that when a new screen is added we only have
// to update this file and the ScreenId enum. Not implemented screens
// return nullptr — ui_core falls back to Home.
#include "screen.h"

#include "boot.h"
#include "sync.h"
#include "home.h"
#include "approval.h"
#include "transfer.h"
#include "stats.h"
#include "usage.h"
#include "permissions.h"
#include "info.h"
#include "pet.h"
#include "buddy_mode.h"
#include "character_picker.h"
#include "settings.h"
#include "reset.h"
#include "system_menu.h"
#include "dock_clock.h"
#include "demo.h"

namespace ui_v2::screens {

const ScreenVTable* vtable_for(ScreenId id) {
  switch(id) {
    case ScreenId::Boot:           return boot::vtable();
    case ScreenId::Sync:           return sync::vtable();
    case ScreenId::Home:           return home::vtable();
    case ScreenId::Stats:          return stats::vtable();
    case ScreenId::Usage:          return usage::vtable();
    case ScreenId::Permissions:    return permissions::vtable();
    case ScreenId::Info:           return info::vtable();
    case ScreenId::Pet:            return pet::vtable();
    case ScreenId::Buddy:          return buddy_mode::vtable();
    case ScreenId::CharacterPicker:return character_picker::vtable();
    case ScreenId::Approval:       return approval::vtable();
    case ScreenId::Transfer:       return transfer::vtable();
    case ScreenId::Settings:       return settings::vtable();
    case ScreenId::SettingsDetail: return settings::vtable();  // same for now
    case ScreenId::Reset:          return reset::vtable();
    case ScreenId::SystemMenu:     return system_menu::vtable();
    case ScreenId::DockClock:      return dock_clock::vtable();
    case ScreenId::Demo:           return demo::vtable();
  }
  return nullptr;
}

}  // namespace ui_v2::screens
