// palette.h — V2 color palette (plan §3.1).
//
// All values are chosen to quantize cleanly to RGB332 (PicoGraphics_PenRGB332).
// The RGB565 form is what st7789 actually drives; the RGB332 form is the
// intermediate pen value PicoGraphics renders through. We store both so
// callers can use whichever matches their API without reconversion.
//
// The ORANGE_MERGE hex is provisional per plan §2.2 — it MUST be tuned
// against a printed case under normal desk light before launch. Only this
// one constant needs to move during calibration.
#pragma once

#include <cstdint>

namespace ui_v2 {

// 24-bit targets (documentation + calibration reference).
//
// Anthropic-native palette. Colors are drawn from the Claude brand system:
//   - Background: a warm near-black ("book ink") instead of pure #000.
//     Backlit LCDs over pure black look bluish/cold; a warm ink reads as
//     warm-paper-at-night and lets the clay accent pop without fighting.
//   - Primary accent: Clay Peach (#D97757) — the exact clay/terracotta used
//     on anthropic.com and the Claude product surfaces.
//   - Text: Linen Cream (#F4EBDB) instead of pure #FFFFFF. Pure white on a
//     backlit panel is harsh; cream feels like book print and matches the
//     page background Anthropic uses in its marketing.
struct Rgb24 { uint8_t r; uint8_t g; uint8_t b; };

constexpr Rgb24 kBgInk           {0x18, 0x14, 0x11};   // warmer book-ink
constexpr Rgb24 kBgInkDeep       {0x0E, 0x0B, 0x09};   // inset ink
constexpr Rgb24 kAccentClay      {0xD9, 0x77, 0x57};   // Anthropic Clay Peach
constexpr Rgb24 kAccentClayDim   {0x93, 0x52, 0x3D};   // dusk clay
constexpr Rgb24 kPageCream       {0xF5, 0xEC, 0xDF};   // softer linen
constexpr Rgb24 kTextPrimary     {0xF5, 0xEC, 0xDF};   // alias, use this everywhere
constexpr Rgb24 kTextDim         {0x9C, 0x8F, 0x81};   // warm grey for captions
constexpr Rgb24 kTextWarm        {0xD8, 0xC4, 0xA6};   // warm secondary text
constexpr Rgb24 kOkGreen         {0x63, 0x99, 0x5C};   // muted sage
constexpr Rgb24 kBusyBlue        {0x79, 0x87, 0xBF};   // soft slate-blue
constexpr Rgb24 kWarnAmber       {0xC9, 0x8D, 0x47};   // clay gold
constexpr Rgb24 kDangerRed       {0xBC, 0x66, 0x4A};   // terracotta red
constexpr Rgb24 kChipPanel       {0x2A, 0x24, 0x20};   // warm chip bg
constexpr Rgb24 kChipPanelHi     {0x3C, 0x34, 0x2F};   // raised chip bg

// Backwards-compatible aliases for pre-existing call sites that said
// "kCaseOrange". The name was ambiguous (case paint vs. brand accent);
// semantically we always meant the clay accent, so alias them.
constexpr Rgb24 kCaseOrange      = kAccentClay;
constexpr Rgb24 kCaseOrangeDim   = kAccentClayDim;

// Converts 24-bit to RGB332 pen byte. PicoGraphics_PenRGB332 uses 3/3/2 packing
// but accepts full 8-8-8 set_rgb() and snaps internally; we keep both APIs
// available so callers choose clarity or speed.
constexpr uint8_t rgb332(Rgb24 c) {
  // 3 bits R | 3 bits G | 2 bits B
  const uint8_t r = static_cast<uint8_t>((c.r * 7 + 127) / 255) & 0x07;
  const uint8_t g = static_cast<uint8_t>((c.g * 7 + 127) / 255) & 0x07;
  const uint8_t b = static_cast<uint8_t>((c.b * 3 + 127) / 255) & 0x03;
  return static_cast<uint8_t>((r << 5) | (g << 2) | b);
}

// Convenience snapped accessors (compile-time).
constexpr uint8_t kPenBgInk        = rgb332(kBgInk);
constexpr uint8_t kPenBgInkDeep    = rgb332(kBgInkDeep);
constexpr uint8_t kPenAccentClay   = rgb332(kAccentClay);
constexpr uint8_t kPenAccentClayDim= rgb332(kAccentClayDim);
constexpr uint8_t kPenCaseOrange   = kPenAccentClay;     // alias
constexpr uint8_t kPenOrangeDim    = kPenAccentClayDim;  // alias
constexpr uint8_t kPenPageCream    = rgb332(kPageCream);
constexpr uint8_t kPenTextPrimary  = rgb332(kTextPrimary);
constexpr uint8_t kPenTextDim      = rgb332(kTextDim);
constexpr uint8_t kPenTextWarm     = rgb332(kTextWarm);
constexpr uint8_t kPenOkGreen      = rgb332(kOkGreen);
constexpr uint8_t kPenBusyBlue     = rgb332(kBusyBlue);
constexpr uint8_t kPenWarnAmber    = rgb332(kWarnAmber);
constexpr uint8_t kPenDangerRed    = rgb332(kDangerRed);
constexpr uint8_t kPenChipPanel    = rgb332(kChipPanel);
constexpr uint8_t kPenChipPanelHi  = rgb332(kChipPanelHi);

// Linear-interpolate between two Rgb24 colors. Used by transitions / tweens.
// t is 0..255; 0 returns a, 255 returns b.
constexpr Rgb24 lerp(Rgb24 a, Rgb24 b, uint8_t t) {
  return Rgb24{
      static_cast<uint8_t>(a.r + ((static_cast<int>(b.r) - a.r) * t) / 255),
      static_cast<uint8_t>(a.g + ((static_cast<int>(b.g) - a.g) * t) / 255),
      static_cast<uint8_t>(a.b + ((static_cast<int>(b.b) - a.b) * t) / 255),
  };
}

}  // namespace ui_v2
