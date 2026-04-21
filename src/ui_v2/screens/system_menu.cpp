// system_menu.cpp — root menu.
#include "system_menu.h"

#include <cstdio>

#include "../chrome.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"
#include "reset.h"

namespace ui_v2::screens::system_menu {

namespace {

enum class Item : uint8_t {
  Settings = 0,
  Character,
  Pet,
  DockClock,
  DemoMode,
  ClearStats,
  Unpair,
  SleepNow,
  FactoryReset,
  kCount,
};

struct Row { Item item; const char* label; };
constexpr Row kRows[] = {
    {Item::Settings,     "Settings"},
    {Item::Character,    "Character"},
    {Item::Pet,          "Pet"},
    {Item::DockClock,    "Dock Clock"},
    {Item::DemoMode,     "Demo Mode"},
    {Item::ClearStats,   "Clear Stats"},
    {Item::Unpair,       "Unpair"},
    {Item::SleepNow,     "Sleep Now"},
    {Item::FactoryReset, "Factory Reset"},
};
constexpr uint8_t kRowCount = sizeof(kRows) / sizeof(kRows[0]);

uint8_t g_sel = 0;

void enter(uint32_t /*now_ms*/) { g_sel = 0; }

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& /*face*/,
          CoreIntent& /*intent*/, uint32_t now_ms) {
  gfx::clear(kBgInk);

  gfx::text("MENU", geom::kPadOuter, geom::kContentY + 4, kTextDim, 1.0f);

  const int y0 = 32;
  const int row_h = 20;
  for(uint8_t i = 0; i < kRowCount; ++i) {
    const int y = y0 + i * row_h;
    const bool sel = (i == g_sel);
    if(sel) {
      gfx::rounded_rect(geom::kPadOuter, y, geom::kPanelW - geom::kPadOuter * 2,
                        row_h - 4, 4, kCaseOrangeDim);
    }
    gfx::text(kRows[i].label, geom::kPadOuter + 10, y + 4,
              sel ? kTextPrimary : kTextWarm, 1.5f);
  }

  chrome::draw_status_rail(in, "Up", "OK", now_ms);
  chrome::draw_footer("Down", "Back");
}

void activate(Item it, BuddyOutputs& out, CoreIntent& intent) {
  switch(it) {
    case Item::Settings:
      intent.replace = true; intent.target = ScreenId::Settings; break;
    case Item::Character:
      intent.replace = true; intent.target = ScreenId::CharacterPicker; break;
    case Item::Pet:
      intent.replace = true; intent.target = ScreenId::Pet; break;
    case Item::DockClock:
      intent.replace = true; intent.target = ScreenId::DockClock; break;
    case Item::DemoMode:
      intent.replace = true; intent.target = ScreenId::Demo; break;
    case Item::ClearStats:
      reset::prepare(reset::Mode::ClearStats);
      intent.replace = true; intent.target = ScreenId::Reset; break;
    case Item::Unpair:
      out.request_unpair = true;
      intent.pop = true;
      break;
    case Item::SleepNow:
      out.request_screen_off = true;
      intent.pop = true;
      break;
    case Item::FactoryReset:
      reset::prepare(reset::Mode::FactoryReset);
      intent.replace = true; intent.target = ScreenId::Reset; break;
    case Item::kCount: break;
  }
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& out,
               CoreIntent& intent, uint32_t /*now_ms*/) {
  if(edge != ButtonEdge::ShortTap) return;
  if(btn == Button::A) {
    g_sel = (g_sel == 0) ? (kRowCount - 1) : (g_sel - 1);
  } else if(btn == Button::B) {
    g_sel = (g_sel + 1) % kRowCount;
  } else if(btn == Button::X) {
    activate(kRows[g_sel].item, out, intent);
  } else if(btn == Button::Y) {
    intent.pop = true;
  }
}

void exit(uint32_t /*now_ms*/) {}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::system_menu
