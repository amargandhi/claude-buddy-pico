// demo.cpp — auto-cycling 21-scene showcase.
//
// Each scene is a closure that fills a SYNTHETIC BuddyInputs and picks
// an expression / mode, then delegates rendering to the real screen or
// directly draws. This way the demo exercises the same visual code that
// ships, not a parallel look-alike.
#include "demo.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../chrome.h"
#include "../face.h"
#include "../geometry.h"
#include "../gfx.h"
#include "../palette.h"
#include "../transitions.h"

#include "home.h"
#include "approval.h"
#include "stats.h"
#include "usage.h"
#include "permissions.h"
#include "info.h"
#include "pet.h"
#include "buddy_mode.h"
#include "dock_clock.h"

namespace ui_v2::screens::demo {

namespace {

constexpr uint8_t kSceneCount = 21;

struct SceneInfo {
  const char* label;
  uint32_t    duration_ms;
};

constexpr SceneInfo kScenes[kSceneCount] = {
    {"Boot merge",       3000},
    {"Sync passkey",     4000},
    {"Home idle",        4000},
    {"Home busy",        4000},
    {"Home alert",       3000},
    {"Approval ask",     2600},
    {"Approval yes",     2800},
    {"Approval ask 2",   2200},
    {"Approval no",      2500},
    {"Buddy auto",       4000},
    {"Buddy auto",       4000},
    {"Buddy auto",       4000},
    {"Stats",            4500},
    {"Usage",            4000},
    {"Permissions",      5000},
    {"Info",             4000},
    {"Dock clock",       4500},
    {"Nap",              3500},
    {"Wink L + R",       3500},
    {"Surprise",         2500},
    {"All-expr merge",   5500},
};

struct State {
  uint8_t  scene    = 0;
  bool     paused   = false;
  uint32_t scene_start_ms = 0;
  bool     scene_action_fired = false;
  uint32_t enter_ms = 0;
  transitions::MergeWipe enter_wipe;
  transitions::MergeWipe exit_wipe;
  bool     exiting  = false;
};
State g_st;

bool is_approval_story_scene(uint8_t scene) {
  return scene >= 5 && scene <= 8;
}

void enter_scene(uint8_t scene, uint32_t now_ms) {
  g_st.scene = scene;
  g_st.scene_start_ms = now_ms;
  g_st.scene_action_fired = false;
  if(is_approval_story_scene(scene)) {
    approval::vtable()->enter(now_ms);
  }
}

void enter(uint32_t now_ms) {
  g_st = State{};
  g_st.enter_ms = now_ms;
  g_st.enter_wipe.start(now_ms, 1000);
  enter_scene(0, now_ms);
}

// Build a synthetic BuddyInputs for scene rendering. Most of the scenes
// delegate to real screen tick() implementations, so we pass a local
// struct with believable values.
BuddyInputs build_synthetic(uint8_t scene, const BuddyInputs& real,
                            uint32_t now_ms) {
  BuddyInputs s = real;   // copy link/battery/RTC
  s.prompt_active = false;
  s.prompt_sent   = false;
  s.prompt_id[0] = '\0';
  s.prompt_tool[0] = '\0';
  s.prompt_hint[0] = '\0';
  s.msg[0] = '\0';
  s.running = 0; s.waiting = 0;
  s.total = 18274;
  s.tokens_today = 412300;
  s.tokens_total = 4820000;
  s.token_rate_per_min = 184;
  s.heartbeat_age_ms = 500;
  s.approvals = 274; s.denials = 31;
  s.level = 7; s.nap_seconds = 11400;
  s.line_count = 3;
  std::strncpy(s.lines[0], "heartbeat 3s ago", sizeof(s.lines[0]));
  std::strncpy(s.lines[1], "tool: Read src/main.cpp", sizeof(s.lines[1]));
  std::strncpy(s.lines[2], "running Bash test suite", sizeof(s.lines[2]));
  for(uint8_t i=0;i<8;++i) s.velocity[i] = 40 + (i*37 % 80);
  s.velocity_count = 8;

  switch(scene) {
    case 2: s.persona = PersonaState::Idle;      break;
    case 3: s.persona = PersonaState::Busy;
            std::strncpy(s.msg, "running 3 tools", sizeof(s.msg));
            s.running = 3; s.waiting = 1; s.token_rate_per_min = 420; break;
    case 4: s.persona = PersonaState::Attention;
            std::strncpy(s.msg, "attention needed", sizeof(s.msg)); break;
    case 5:
    case 6: {
            const uint32_t scene_age = now_ms - g_st.scene_start_ms;
            s.prompt_active = true;
            std::strncpy(s.prompt_tool, "WebFetch", sizeof(s.prompt_tool));
            std::strncpy(s.prompt_hint, "fetch animation timing notes for motion polish",
                         sizeof(s.prompt_hint));
            std::snprintf(s.prompt_id, sizeof(s.prompt_id), "demo-approve");
            s.prompt_arrived_ms = now_ms - std::min<uint32_t>(scene_age + (scene == 6 ? 1100u : 300u), 4200u);
            s.persona = PersonaState::Attention;
            s.running = 1;
            s.waiting = 1;
            std::strncpy(s.msg, scene == 5 ? "mock approval request" : "holding for approval",
                         sizeof(s.msg));
            break;
          }
    case 7:
    case 8: {
            const uint32_t scene_age = now_ms - g_st.scene_start_ms;
            s.prompt_active = true;
            std::strncpy(s.prompt_tool, "Bash", sizeof(s.prompt_tool));
            std::strncpy(s.prompt_hint, "rm -rf build/ && cmake --build build/",
                         sizeof(s.prompt_hint));
            std::snprintf(s.prompt_id, sizeof(s.prompt_id), "demo-deny");
            s.prompt_arrived_ms = now_ms - std::min<uint32_t>(scene_age + (scene == 8 ? 700u : 0u), 2200u);
            s.persona = PersonaState::Attention;
            s.running = 1;
            s.waiting = 1;
            std::strncpy(s.msg, scene == 7 ? "new approval request" : "faster deny pass",
                         sizeof(s.msg));
            break;
          }
    case 9:  s.persona = PersonaState::Heart; break;
    case 10: s.persona = PersonaState::Celebrate; break;
    case 11: s.persona = PersonaState::Dizzy; break;
    case 17: s.persona = PersonaState::Sleep; break;
    default: s.persona = PersonaState::Idle; break;
  }
  return s;
}

// Draw the demo chrome (rail + footer) plus a centered scene counter.
// Demo buttons always do Prev/Next/Auto/Exit regardless of the sub-screen
// being overlaid, so we force-override chrome even when a sub-screen's
// tick() already drew its own.
void draw_demo_chrome(const BuddyInputs& in, uint8_t scene, uint32_t now_ms) {
  chrome::draw_status_rail(in, "Prev", "Auto", now_ms);
  chrome::draw_footer("Next", "Exit");
  char chip[16];
  std::snprintf(chip, sizeof(chip), "%u/21", static_cast<unsigned>(scene + 1));
  gfx::text_aligned(chip, 0, geom::kFooterY + 10, geom::kPanelW,
                    gfx::Align::Center, kTextDim, 1.0f);
}

// Scene dispatcher — draws the scene given its index.
void draw_scene(uint8_t scene, const BuddyInputs& in, BuddyOutputs& out,
                FaceState& face, uint32_t now_ms) {
  // We create a real CoreIntent locally so delegates don't escape up into
  // the actual stack; we ignore whatever they set.
  CoreIntent local_intent{};
  BuddyOutputs local_out{};

  switch(scene) {
    case 0: {
      // Boot merge — paint orange, fade reveal to neutral face.
      gfx::clear(kBgInk);
      if(face.target != ExpressionId::Neutral) {
        face::begin_transition(face, ExpressionId::Neutral, now_ms, 300);
      }
      face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kEyeCy);
      face::tick_and_render(face, now_ms);
      const uint32_t scene_age = now_ms - g_st.scene_start_ms;
      if(scene_age < 800) {
        gfx::fill_rect(0, 0, geom::kPanelW, geom::kPanelH, kCaseOrange);
      }
      draw_demo_chrome(in, scene, now_ms);
      break;
    }
    case 1: {
      // Sync passkey.
      gfx::clear(kBgInk);
      face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kEyeCy);
      if(face.target != ExpressionId::Attentive) {
        face::begin_transition(face, ExpressionId::Attentive, now_ms, 200);
      }
      face::set_ambient(face, true, false);
      face::tick_and_render(face, now_ms);
      gfx::text_aligned("Pairing", 0, geom::kBelowFaceY, geom::kPanelW,
                        gfx::Align::Center, kTextWarm, 1.0f);
      gfx::text_aligned("429183", 0, 128, geom::kPanelW,
                        gfx::Align::Center, kTextPrimary, 4.0f);
      gfx::text_aligned("Enter on your Mac", 0, 172, geom::kPanelW,
                        gfx::Align::Center, kTextDim, 1.0f);
      draw_demo_chrome(in, scene, now_ms);
      break;
    }
    case 2: case 3: case 4:
      face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kEyeCy);
      home::vtable()->tick(in, local_out, face, local_intent, now_ms);
      draw_demo_chrome(in, scene, now_ms);
      break;
    case 5: case 6: case 7: case 8:
      face::set_base(face, 180, 210, 36);
      if((scene == 6 || scene == 8) && !g_st.scene_action_fired) {
        const uint32_t scene_age = now_ms - g_st.scene_start_ms;
        const uint32_t trigger_ms = (scene == 6) ? 1100u : 700u;
        if(scene_age >= trigger_ms) {
          approval::vtable()->on_button(scene == 6 ? Button::A : Button::B,
                                        ButtonEdge::ShortTap, local_out, local_intent, now_ms);
          g_st.scene_action_fired = true;
        }
      }
      approval::vtable()->tick(in, local_out, face, local_intent, now_ms);
      draw_demo_chrome(in, scene, now_ms);
      break;
    case 9: case 10: case 11:
      face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kEyeCy);
      buddy_mode::vtable()->tick(in, local_out, face, local_intent, now_ms);
      draw_demo_chrome(in, scene, now_ms);
      break;
    case 12:
      stats::vtable()->tick(in, local_out, face, local_intent, now_ms);
      draw_demo_chrome(in, scene, now_ms);
      break;
    case 13:
      usage::vtable()->tick(in, local_out, face, local_intent, now_ms);
      draw_demo_chrome(in, scene, now_ms);
      break;
    case 14:
      permissions::vtable()->tick(in, local_out, face, local_intent, now_ms);
      draw_demo_chrome(in, scene, now_ms);
      break;
    case 15:
      info::vtable()->tick(in, local_out, face, local_intent, now_ms);
      draw_demo_chrome(in, scene, now_ms);
      break;
    case 16:
      face::set_base(face, 80, 240, 60);
      dock_clock::vtable()->tick(in, local_out, face, local_intent, now_ms);
      draw_demo_chrome(in, scene, now_ms);
      break;
    case 17: {
      // Nap scene.
      gfx::clear(kBgInk);
      face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kEyeCy);
      if(face.target != ExpressionId::Asleep) {
        face::begin_transition(face, ExpressionId::Asleep, now_ms, 400);
      }
      face::set_ambient(face, false, false);
      face::tick_and_render(face, now_ms);
      gfx::text_aligned("Zzz...", 0, geom::kBelowFaceY + 16, geom::kPanelW,
                        gfx::Align::Center, kTextDim, 2.0f);
      draw_demo_chrome(in, scene, now_ms);
      break;
    }
    case 18: {
      // Wink alternation every 1.5 s.
      gfx::clear(kBgInk);
      face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kEyeCy);
      const uint32_t age = now_ms - g_st.scene_start_ms;
      const ExpressionId which = (age / 1500 % 2 == 0)
          ? ExpressionId::WinkL : ExpressionId::WinkR;
      if(face.target != which) {
        face::begin_transition(face, which, now_ms, 120);
      }
      face::tick_and_render(face, now_ms);
      gfx::text_aligned("L/R Wink", 0, geom::kBelowFaceY + 12, geom::kPanelW,
                        gfx::Align::Center, kTextWarm, 1.5f);
      draw_demo_chrome(in, scene, now_ms);
      break;
    }
    case 19: {
      // Surprise pop.
      gfx::clear(kBgInk);
      face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kEyeCy);
      if(face.target != ExpressionId::Surprise) {
        face::begin_transition(face, ExpressionId::Surprise, now_ms, 60);
      }
      face::tick_and_render(face, now_ms);
      gfx::text_aligned("!", 0, geom::kBelowFaceY, geom::kPanelW,
                        gfx::Align::Center, kDangerRed, 3.0f);
      draw_demo_chrome(in, scene, now_ms);
      break;
    }
    case 20: {
      // All-expression merge: rotate through every expression every 300 ms,
      // full orange flash between transitions.
      gfx::clear(kBgInk);
      face::set_base(face, geom::kEyeCxLeft, geom::kEyeCxRight, geom::kEyeCy);
      const uint32_t age = now_ms - g_st.scene_start_ms;
      const uint8_t total = static_cast<uint8_t>(ExpressionId::Count);
      const uint8_t idx = static_cast<uint8_t>((age / 300) % total);
      const ExpressionId want = static_cast<ExpressionId>(idx);
      if(face.target != want) {
        face::begin_transition(face, want, now_ms, 200);
      }
      face::tick_and_render(face, now_ms);

      // Orange flash for the first 60 ms of each 300 ms window — reinforces
      // the merge/flip feel.
      if((age % 300) < 60) {
        gfx::fill_rect(0, 0, geom::kPanelW, geom::kPanelH, kCaseOrange);
      }

      // Name label.
      gfx::text_aligned(expression_name(want), 0, geom::kBelowFaceY + 16,
                        geom::kPanelW, gfx::Align::Center, kTextWarm, 1.5f);

      draw_demo_chrome(in, scene, now_ms);
      break;
    }
    default: break;
  }
}

void tick(const BuddyInputs& real_in, BuddyOutputs& out, FaceState& face,
          CoreIntent& intent, uint32_t now_ms) {
  // Handle exit wipe first.
  if(g_st.exiting) {
      // Draw the current scene underneath.
      const BuddyInputs syn = build_synthetic(g_st.scene, real_in, now_ms);
      draw_scene(g_st.scene, syn, out, face, now_ms);
    g_st.exit_wipe.draw(now_ms);
    if(g_st.exit_wipe.is_done(now_ms)) {
      intent.replace = true;
      intent.target  = ScreenId::Home;
    }
    return;
  }

  // Advance scene automatically if not paused.
  if(!g_st.paused) {
    const uint32_t dur = kScenes[g_st.scene].duration_ms;
    if((now_ms - g_st.scene_start_ms) >= dur) {
      enter_scene(static_cast<uint8_t>((g_st.scene + 1) % kSceneCount), now_ms);
    }
  }

  const BuddyInputs syn = build_synthetic(g_st.scene, real_in, now_ms);
  draw_scene(g_st.scene, syn, out, face, now_ms);

  // Paint entry wipe on top if still active.
  if(g_st.enter_wipe.is_active(now_ms)) {
    g_st.enter_wipe.draw(now_ms);
  }
}

void on_button(Button btn, ButtonEdge edge, BuddyOutputs& /*out*/,
               CoreIntent& /*intent*/, uint32_t now_ms) {
  if(edge != ButtonEdge::ShortTap) return;
  if(btn == Button::A) {
    enter_scene((g_st.scene == 0) ? (kSceneCount - 1) : (g_st.scene - 1), now_ms);
  } else if(btn == Button::B) {
    enter_scene(static_cast<uint8_t>((g_st.scene + 1) % kSceneCount), now_ms);
  } else if(btn == Button::X) {
    g_st.paused = !g_st.paused;
  } else if(btn == Button::Y) {
    g_st.exiting = true;
    g_st.exit_wipe.start(now_ms, 800);
  }
}

void exit(uint32_t /*now_ms*/) {
  g_st = State{};
}

const ScreenVTable kVT{&enter, &tick, &on_button, &exit};

}  // namespace

const ScreenVTable* vtable() { return &kVT; }

}  // namespace ui_v2::screens::demo
