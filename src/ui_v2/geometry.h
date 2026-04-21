// geometry.h — V2 layout geometry (plan §3.3 + §4.2).
//
// The grid is 8 px with a 4 px half-step. Content coordinates are expressed
// against the full 320x240 panel. The case-derived eye positions below are
// computed from real measurements, not guesses:
//
//   - case main body: 85.5 mm × 57.0 mm
//   - display active area: 57.5 mm × 43.2 mm
//   - pixel scale: 5.565 px/mm horizontal, 5.556 px/mm vertical (isotropic)
//   - case-to-screen offset assuming centered: 14.0 mm horizontal, 6.9 mm vertical
//
// These values land the eyes where the Clawd sprite's eyes would be on the
// whole character, which is the entire point of the orange-merge illusion.
#pragma once

#include <cstdint>

namespace ui_v2::geom {

// Panel.
constexpr int kPanelW = 320;
constexpr int kPanelH = 240;

// Outer padding and gutters.
constexpr int kPadOuter   = 8;
constexpr int kGutter     = 8;
constexpr int kPadInner   = 4;

// Reserved strips.
//
// Status rail (top) and footer (bottom) reserve outer bands where chrome
// lives. The four corners of these bands hold the A/B/X/Y action labels
// because the 3D-printed case hides the physical button letters — spatial
// position is the only reliable mapping from on-screen label to physical
// button, so we use it explicitly.
// Sizing notes (2026-04-20): the 2.8" ST7789V panel is ~143 PPI; at a
// typical 40–50 cm desk viewing distance the minimum comfortable body
// height for glanceable text is ~10 px, and 12–14 px for chips that must
// be scanned without focusing. The old 16-px rail forced chip text to
// scale 1.2 (glyph h = 7) which the user correctly called "squashed".
// Bumped rail to 20 px and footer to 28 px so corner labels and chips
// can be rendered at scale 1.5+ with breathing room. All content screens
// anchor their y-offsets either to kContentY (auto-tracks) or to explicit
// mid-screen constants well clear of the new bands, so the bump is safe.
constexpr int kStatusRailY = 0;
constexpr int kStatusRailH = 20;
constexpr int kFooterH     = 28;
constexpr int kFooterY     = 240 - kFooterH;                 // 212

// Main content window (everything between status rail and footer).
constexpr int kContentY = kStatusRailY + kStatusRailH;       // 16
constexpr int kContentH = kFooterY - kContentY;              // 198

// Two symmetric 152-wide columns (320 = 152 + 8 gutter + 152 + 8 pad).
constexpr int kColLeftX  = kPadOuter;                        // 8
constexpr int kColLeftW  = 152;
constexpr int kColRightX = kColLeftX + kColLeftW + kGutter;  // 168
constexpr int kColRightW = 152;

// ---------------------------------------------------------------
// Eye geometry (plan §4.2)
//
// Defaults computed from the real 85.5 × 57 mm case. The eyes are positioned
// as if the ENTIRE CASE is the head, not as if the display is the head.
// This is the critical design decision: when orange-merge is active and the
// display color matches the case, the eyes must sit where the mascot's eyes
// would sit on the whole character.
// ---------------------------------------------------------------
constexpr int kEyeCxLeft  = 51;   // 27% from case-left ≈ 23.1 mm → 51 px
constexpr int kEyeCxRight = 269;  // 73% from case-left ≈ 62.4 mm → 269 px
constexpr int kEyeCy      = 73;   // 35% from case-top ≈ 19.95 mm → 73 px
constexpr int kApprovalEyeCy = 62;
constexpr int kBuddyEyeCy    = 73;
constexpr int kDockEyeCy     = 69;
constexpr int kEyeW       = 24;   // 5% case width
constexpr int kEyeH       = 48;   // 15% case height
constexpr int kEyeR       = 4;    // square, Clawd-style (not round)
constexpr int kEyeSep     = kEyeCxRight - kEyeCxLeft;  // 218 px, 68% of panel

// Derived: content that sits BELOW the eye band (banner, counters, transcript).
// The eyes occupy y in [kEyeCy - kEyeH/2, kEyeCy + kEyeH/2] = [49, 97].
// Everything under the face band starts at kFaceBandBottom + small margin.
constexpr int kFaceBandTop    = kEyeCy - kEyeH / 2;  // 49
constexpr int kFaceBandBottom = kEyeCy + kEyeH / 2;  // 97
constexpr int kBelowFaceY     = kFaceBandBottom + 16; // 113

// Home content rows (plan §6.4 mockup).
constexpr int kHomeBannerY     = 160;
constexpr int kHomeStatsY      = 180;
constexpr int kHomeTranscriptY = 200;

}  // namespace ui_v2::geom
