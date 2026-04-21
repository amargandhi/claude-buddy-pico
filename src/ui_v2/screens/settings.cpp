// settings.cpp — editable settings list.
#include "settings.h"

#include <cstdio>

#include "../chrome.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"

namespace ui_v2::screens::settings {

namespace {

enum class Row : uint8_t {
  Brightness = 0,
  Led,
  Transcript,
  Demo,
  DockClock,
  Motion,
  kCount,
};

uint8_t g_sel = 0;
// Pending-activate flag set by on_button, consumed by tick. This avoids
// needing BuddyInputs inside on_button.
bool g_activate = false;

void enter(uint32_t /*now_ms*/) { g_sel = 0; g_activate = false; }

const char* label_for(Row r) {
  switch(r) {
    case Row::Brightness:    return "Brightness";
    case Row::Led:           return "LED";
    case Row::Transcript:    return "Transcript";
    case Row::Demo:          return "Demo on boot";
    case Row::DockClock:     return "Dock Clock";
    case Row::Motion:        return "Animation";
    case Row::kCount:        return "";
  }
  return "";
}

void value_for(const BuddyInputs& in, Row r, char* out, size_t n) {
  switch(r) {
    case Row::Brightness:
      std::snprintf(out, n, "%u/5",
                    static_cast<unsigned>(in.brightness_level) + 1); break;
    case Row::Led:
      std::snprintf(out, n, "%s", in.led_enabled ? "ON" : "off"); break;
    case Row::Transcript:
      std::snprintf(out, n, "%s", in.transcript_enabled ? "ON" : "off"); break;
    case Row::Demo:
      std::snprintf(out, n, "%s", in.demo_enabled ? "ON" : "off"); break;
    case Row::DockClock:
      std::snprintf(out, n, "%s", in.dock_clock_enabled ? "ON" : "off"); break;
    case Row::Motion: {
      const char* label = "Full";
      if(in.motion_profile == MotionProfile::Reduced) label = "Reduced";
      else if(in.motion_profile == MotionProfile::Static) label = "Static";
      std::snprintf(out, n, "%s", label);
      break;
    }
    case Row::kCount: out[0] = '\0'; break;
  }
}

void activate(const BuddyInputs& in, BuddyOutputs& out, Row r) {
  out.dirty_persist = true;
  switch(r) {
    case Row::Brightness:
      out.set_brightness = true;
      out.brightness_level = (in.brightness_level + 1) % 5;
      break;
    case Row::Led:
      out.set_led_enabled = true;
      out.led_enabled = !in.led_enabled;
      break;
    case Row::Transcript:
      out.set_transcript = true;
      out.transcript_enabled = !in.transcript_enabled;
      break;
    case Row::Demo:
      out.set_demo_enabled = true;
      out.demo_enabled = !in.demo_enabled;
      break;
    case Row::DockClock:
      out.set_dock_clock = true;
      out.dock_clock_enabled = !in.dock_clock_enabled;
      break;
    case Row::Motion:
      out.set_motion_profile = true;
      if(in.motion_profile == MotionProfile::Full) {
        out.motion_profile = MotionProfile::Reduced;
      } else if(in.motion_profile == MotionProfile::Reduced) {
        out.motion_profile = MotionProfile::Static;
      } else {
        out.motion_profile = MotionProfile::Full;
      }
      break;
    case Row::kCount: break;
  }
}

void tick(const BuddyInputs& in, BuddyOutputs& /*out*/, FaceState& /*face*/,
          CoreIntent& /*intent*/, uint32_t now_ms) {
  gfx::clear(kBgInk);
  gfx::text("SETTINGS", geom::kPadOuter, geom::kContentY + 4, kTextDim, 1.0f);

  const int y0 = 36;
  const int row_h = 22;
  for(uint8_t i = 0; i < static_cast<uint8_t>(Row::kCount); ++i) {
    const Row r = static_cast<Row>(i);
    const int y = y0 + i * row_h;
    const bool sel = (i == g_sel);
    if(sel) {
      gfx::rounded_rect(geom::kPadOuter, y, geom::kPanelW - geom::kPadOuter * 2,
                        row_h - 4, 4, kCaseOrangeDim);
    }
    gfx::text(label_for(r), geom::kPadOuter + 10, y + 4,
              sel ? kTextPrimary : kTextWarm, 1.5f);

    char val[16];
    value_for(in, r, val, sizeof(val));
    gfx::text_aligned(val, 0, y + 4, geom::kPanelW - geom::kPadOuter - 10,
                      gfx::Align::Right,
                      sel ? kTextPrimary : kTextDim, 1.5f);
  }

  chrome::draw_status_rail(in, "Up", "Edit", now_ms);
  chrome::draw_footer("Down", "Back");
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& out,
               CoreIntent& intent, uint32_t /*now_ms*/) {
  if(edge != ButtonEdge::ShortTap) return;
  const uint8_t n = static_cast<uint8_t>(Row::kCount);
  if(btn == Button::A) {
    g_sel = (g_sel == 0) ? (n - 1) : (g_sel - 1);
  } else if(btn == Button::B) {
    g_sel = (g_sel + 1) % n;
  } else if(btn == Button::X) {
    // We don't have `in` in the button callback. Defer the mutation to
    // the next tick: set a pending flag that `tick_with_activate` consumes
    // so we can read the fresh BuddyInputs there.
    g_activate = true;
  } else if(btn == Button::Y) {
    intent.pop = true;
  }
}

void tick_with_activate(const BuddyInputs& in, BuddyOutputs& out,
                        FaceState& face, CoreIntent& intent, uint32_t now_ms) {
  if(g_activate) {
    activate(in, out, static_cast<Row>(g_sel));
    g_activate = false;
  }
  tick(in, out, face, intent, now_ms);
}

void exit(uint32_t /*now_ms*/) { g_activate = false; }

const ScreenVTable kVT{&enter, &tick_with_activate, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::settings
