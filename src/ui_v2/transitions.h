// transitions.h — screen-level transitions (plan §4.6).
//
// Transitions are short, declarative animations that run on top of whatever
// screen is currently drawing. Each transition has:
//   - a start_ms / duration_ms window,
//   - a one-call draw(now_ms) that paints the overlay for the current frame,
//   - an is_done() predicate the screen polls to advance its state machine.
//
// Screens own the transition instance — they decide when to start one and
// when to dismiss it. The engine is deliberately dumb so the same code can
// be used from boot, link-established events, settings exits, demo mode
// entry/exit, and the orange-merge flourish.
//
// All transitions are non-blocking: calling draw() must never sleep or spin.
// They are driven by the 40 ms UI tick. A 400 ms transition is 10 frames.
#pragma once

#include <cstdint>

#include "palette.h"

namespace ui_v2::transitions {

// -----------------------------------------------------------------
// merge_wipe — the signature V2 transition.
//
// Fills the whole panel with CASE_ORANGE outward from center for the first
// half, holds a short beat, then dissolves back to transparent. The eyes
// stay drawn underneath throughout — this is the "blink into the case" move.
//
// Typical durations:
//   - 400 ms in/hold/out — boot, link established, approval dismissed
//   - 1000 ms — demo mode entry/exit (more theatrical)
// -----------------------------------------------------------------
class MergeWipe {
 public:
  void start(uint32_t now_ms, uint32_t duration_ms);
  void draw(uint32_t now_ms);              // paints an overlay for this frame
  bool is_done(uint32_t now_ms) const;
  bool is_active(uint32_t now_ms) const;   // true once started until done
  // How "occluded" the underlying content is, 0..255 — screens may use this
  // to avoid rendering details that would just be painted over anyway.
  uint8_t occlusion(uint32_t now_ms) const;

 private:
  uint32_t start_ms_    = 0;
  uint32_t duration_ms_ = 0;
  bool     armed_       = false;
};

// -----------------------------------------------------------------
// chip_pulse — a 150 ms color blink on a rectangular region.
//
// Used to draw attention to a status-rail chip when its value changes
// (link established, risk tier escalated, etc). The caller supplies the
// rect; this class just decides the pulse color for each frame.
// -----------------------------------------------------------------
class ChipPulse {
 public:
  void start(uint32_t now_ms, Rgb24 pulse_color);
  // Returns true if a pulse is currently running, and fills `out_color` with
  // the blended color for this frame. Returns false when inactive.
  bool sample(uint32_t now_ms, Rgb24 base_color, Rgb24& out_color) const;
  bool is_active(uint32_t now_ms) const;

 private:
  uint32_t start_ms_   = 0;
  Rgb24    pulse_color_{};
  bool     armed_      = false;
};

// -----------------------------------------------------------------
// wipe_down / wipe_up — 200 ms horizontal bands sweeping through.
//
// Used for navigation between screens where the merge_wipe would be too
// heavy (e.g., stats → usage swipe). A 4 px orange band travels top→bottom
// or bottom→top.
// -----------------------------------------------------------------
enum class WipeDirection : uint8_t { Down, Up };

class Wipe {
 public:
  void start(uint32_t now_ms, WipeDirection dir);
  void draw(uint32_t now_ms);
  bool is_done(uint32_t now_ms) const;
  bool is_active(uint32_t now_ms) const;

 private:
  uint32_t      start_ms_ = 0;
  WipeDirection dir_      = WipeDirection::Down;
  bool          armed_    = false;
};

// -----------------------------------------------------------------
// fade — 200 ms global alpha lerp against a target color.
//
// Renders a full-panel flat fill with an alpha factor that ramps from 0 to
// `peak_alpha` (0..255) and back to 0 across the duration. The caller
// supplies the color — usually kBgInk (to darken into nap) or kCaseOrange
// (for a soft flourish).
// -----------------------------------------------------------------
class Fade {
 public:
  void start(uint32_t now_ms, uint32_t duration_ms, Rgb24 color,
             uint8_t peak_alpha);
  void draw(uint32_t now_ms);
  bool is_done(uint32_t now_ms) const;
  bool is_active(uint32_t now_ms) const;

 private:
  uint32_t start_ms_    = 0;
  uint32_t duration_ms_ = 0;
  Rgb24    color_{};
  uint8_t  peak_alpha_  = 0;
  bool     armed_       = false;
};

}  // namespace ui_v2::transitions
