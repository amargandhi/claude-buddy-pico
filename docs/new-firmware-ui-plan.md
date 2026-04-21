# New Firmware UI Plan

This document is a deep design plan for a rewritten UI on the current Claude Buddy Pico hardware stack. It is not an implementation spec; it defines the visual language, button model, screen architecture, animation system, and eye-expression vocabulary so the firmware work that follows has a shared reference.

It assumes the hardware profile documented in `docs/advanced-firmware-capability-review.md` and the protocol surface verified against `anthropics/claude-desktop-buddy/REFERENCE.md`.

This plan is opinionated. Where the existing firmware does something reasonable, it stays. Where the upstream reference design is a bad fit for this screen, case, and mascot, it is discarded.

## 0. Guiding Principles

1. **The case is the character.** The physical enclosure is a literal 3D sprite of the Claude Code mascot. The firmware's job is to finish the illusion, not compete with it. Anything that makes the device look like a generic IoT gadget is working against the build.
2. **The screen is not a modal dialog.** Upstream ships a M5Stick-shaped portrait UI where one screen owns the frame at a time. This build has a 320x240 landscape panel behind a face. Imitating the M5Stick layout wastes the aspect ratio and fights the case.
3. **Trust state is never a footnote.** This device's most valuable job is approving or denying tool calls that can touch the user's machine. Whether the BLE link is paired, secure-but-not-bonded, or degraded has to be visually unavoidable before any permission decision is presented.
4. **Do not fake precision.** Battery percent is a voltage estimate. Usage "limits" are not in the protocol. Tokens-per-level is a local heuristic. All of these can be shown, but the visual language must tell the truth about which numbers are measured vs. estimated vs. synthetic.
5. **Two channels, two jobs.** The LCD speaks meaning. The RGB LED speaks urgency. If they are ever saying the same thing, one of them is wasted.
6. **Buttons are physical, not metaphorical.** A and B are on the left edge of the panel; X and Y are on the right edge. Footer hints must sit physically under the button they label. Anything else forces the user to translate.

## 1. Hardware Constraints (Verified)

| Item | Value | Source |
| --- | --- | --- |
| Panel | 2.8" IPS LCD, 320x240, ST7789V | `pico_display_28.hpp` |
| Active area | 57.5 x 43.2 mm | datasheet |
| Framebuffer | `PicoGraphics_PenRGB332` (~76.8 KB) | `claude_buddy_pico.cpp:169` |
| Buttons | A (GP12), B (GP13), X (GP14), Y (GP15) | `claude_buddy_pico.cpp:172-175` |
| RGB LED | R=GP26, G=GP27, B=GP28 | pin map |
| UI tick | 40 ms (25 fps) | `kUiTickMs` |
| Long-press | 800 ms | `kHoldThresholdMs` |
| Persist debounce | 2000 ms | `kPersistDebounceMs` |
| RAM | 520 KB SRAM | RP2350 |
| Flash | 4 MB, last sector = storage | board header |

**Practical implications.**

- 25 fps is plenty for eye animation but tight for anything particle-heavy. Budget animations in keyframes, not per-pixel.
- RGB332 gives 8 levels of R, 8 of G, 4 of B (256 colors total). Gradients banding on the panel is not a bug, it is the format. Design accordingly.
- Every full-screen fill is ~76.8 KB of writes at minimum. Partial redraws matter.
- Four buttons, two on each side, physically fixed. Any screen that needs five or more actions has to use holds or context switches.

## 2. The Orange-Merge Identity Trick

The most important visual idea in this build is that the display *can* disappear — not that it usually does.

The case is printed in an orange filament. When the screen renders solid case-orange across every pixel, the bezel gap becomes the only visible break and the device reads as a solid orange object with whatever pixel detail we leave black on top. Because the mascot is an orange body with two black eye rectangles plus a few interior black lines, a well-calibrated orange-merge animation makes the whole Pico look like the mascot is physically embodied on your desk.

**The default UI is NOT orange.** Orange-merge is a flourish reserved for specific moments (boot, link state changes, exit transitions, demo mode). For 99% of the time the device is on, the main UI runs in the refined Anthropic palette: warm-inked near-black background, cream text, targeted orange used only as an accent for brand chips and the merge-event color. Orange as a full background is loud; used constantly it would be exhausting and cheap. Used rarely and well, it becomes the device's signature flourish.

### 2.1 When to use orange-merge

Orange-merge is a flourish. It is not a chrome layer over normal operation. Overusing it will make the normal UI feel like a constant interruption.

| Event | Orange-merge usage |
| --- | --- |
| Cold boot | Full merge, eyes open into position, status fades in beneath |
| BLE link established (first time after disconnect) | Quick pulse — background flashes to orange for ~250 ms, eyes do a joyful blink, then fades back to normal UI |
| USB plugged in | Bottom third of screen washes orange like a battery fill, holds ~500 ms, eases back |
| Exiting Settings overlay | Brief merge wipe used as transition out of the overlay |
| Factory reset complete | Merge, eyes close, eyes reopen as fresh boot |
| Entering / exiting Demo Mode | Long merge sequence (~1 s), theatrical — demo is where the flourishes get shown off |

It is explicitly **not used for**:

- Every menu transition (would be exhausting)
- Permission prompts (would steal attention from the decision)
- Heartbeat ticks (would cause flicker)

### 2.2 Calibration gotcha

The pixel orange and the filament orange will not match until measured. The backlight has a color temperature, the panel has its own gamut, the PLA absorbs and reflects at a certain curve, and ambient light shifts both. This is a **physical measurement problem**, not a software one. Plan to iterate the hex value against a physical photo of the case under normal desk light, not against Pantone or web hex codes.

Provisional target hex for the firmware palette (to be tuned against the printed case):

- `ORANGE_MERGE` ≈ `#DA7756` → snapped to RGB332 as `R=6 G=3 B=1` → 24-bit `#DB6D55`

Leave a `CASE_ORANGE` compile-time constant so the calibration pass only touches one value.

### 2.3 What gets drawn on top of the merge

When the screen is in merge state, only these elements render in black:

- The two eye rectangles (at fixed positions matching mascot proportions, see §4)
- Optional "body seam" lines that correspond to the mascot's interior black gaps
- No text, no status chips, no counters

This keeps the illusion clean. Any text that appears during a merge should fade in **after** the merge completes, not during it.

## 2.5 Data Availability Audit

Before committing screens, this is the explicit check: every planned screen has been walked against the verified upstream protocol surface (`REFERENCE.md`, confirmed 2026-04-20). Columns tell the truth about where every pixel's data comes from and what's missing.

Legend:
- **Y** — protocol field exists, we can render it directly
- **P** — persisted locally (flash), truthful but local
- **D** — derived from other truthful data (e.g., a rate computed from two counters)
- **N** — not available in protocol; would require either a host-side extension or a change of plan
- **—** — not required by this screen

| Screen | Primary fields needed | Source | Verdict |
| --- | --- | --- | --- |
| Boot | none (local only) | — | Fully renderable |
| Sync / Pairing | BLE state, passkey | local | Fully renderable |
| Home | `total`, `running`, `waiting`, `msg`, `entries[]`, `tokens_today`, `bat.pct`, `bat.usb`, `sec` | Y for all | Fully renderable |
| Pet (main cycle) | local pet stats (fed/energy/mood), recent approvals (`appr`), `tokens_today` | P + Y | Fully renderable |
| Stats | `appr`, `deny`, `vel`, `nap`, `tokens`, `tokens_today`, derived rate and MTTA | P + D | Fully renderable. MTTA (mean time-to-approve) requires new plumbing around prompt-arrival timestamps — feasible locally, not in protocol. |
| Usage | `tokens_today`, `tokens`, velocity (derived), projected EOD (derived) | Y + D | Fully renderable for what we measure. Any "quota %" or "messages remaining" bar is **N** — fabricate or drop. Plan drops it. |
| Permissions history | `id`, `tool`, `hint`, local ring buffer of decisions + timestamps | Y + P (buffer) | Fully renderable. Ring buffer is new state; RAM cost ~512 B for 64 entries. |
| Approval overlay | `prompt.id`, `prompt.tool`, `prompt.hint` | Y | Fully renderable. Path / diff / command previews are **N** — only `hint` is available. Tool-aware risk tiering is a local heuristic, labeled as such. |
| Info pages | static strings + `status` envelope + BLE state | local + Y | Fully renderable |
| Settings | local state only | P | Fully renderable |
| Reset | local state only | P | Fully renderable |
| Dock clock | time sync `[epoch, offset]`, owner | Y + P | Fully renderable |
| Demo Mode | synthesized heartbeat, persisted demo scenarios | local | Fully renderable (current firmware already has a demo loop — extend it) |

**Bottom line.** Every screen in this plan can be fully driven by real data except two honestly-flagged gaps:

1. Usage **limits** / quota — the word "limit" implies a ceiling we don't know. Screen is renamed to `USAGE` and scoped to measured data only.
2. Permission previews beyond the `hint` string — we can render the three fields the protocol sends, no more. Tool-aware color coding is our local inference.

Nothing else in this plan requires data the protocol does not expose.

## 3. Visual Language

### 3.1 Palette

RGB332 snapped values. Hex is the 24-bit rendering target; the panel will quantize to the nearest RGB332 cell.

| Name | Hex (~) | Use |
| --- | --- | --- |
| `BG_INK` | `#000000` | Default background |
| `CASE_ORANGE` | `#DB6D55` | Orange-merge, primary accent fill, Claude-brand accent |
| `CASE_ORANGE_DIM` | `#923624` | Orange-merge fade frames, disabled orange elements |
| `TEXT_PRIMARY` | `#FFFFFF` | Body text at full strength |
| `TEXT_DIM` | `#808080` | Labels, tertiary info |
| `TEXT_WARM` | `#E8E0C0` | Claude-brand warm text on orange backgrounds |
| `OK_GREEN` | `#36B000` | Linked + idle, approvals, trusted link |
| `BUSY_BLUE` | `#0036FF` | Claude is running |
| `WARN_AMBER` | `#FFAA00` | Approval pending, batt < 20% |
| `DANGER_RED` | `#FF0000` | Unpaired link attempted, batt critical, denial toast |
| `CHIP_PANEL` | `#1C1C1C` | Status chip backgrounds |

**Rules.**

- Any status chip is opaque. No transparent overlays; RGB332 banding makes them ugly.
- Color is a second carrier for state. Never rely on color alone — pair with an icon or letter (`LNK`, `USB`, `BAT`).
- Orange is reserved. It is the brand color and the merge color; using it for filler borders dilutes both roles.

### 3.2 Typography

The Pimoroni library ships bitmap fonts. We do not have Anthropic's Copernicus or Styrene B at 8 px. We stop pretending.

| Role | Font | Size |
| --- | --- | --- |
| Display headline (device name, big numerals) | `bitmap14` or custom monospace at 24 px | 1-2 lines max |
| Section headers | `bitmap8` uppercase, 1 px letter-spacing | 8-10 px |
| Body text | default 8x8 | 8 px |
| Footer hints | `bitmap6` or compressed 8x8 | 6-8 px |
| Big clock / token numerals | custom 7-segment-ish bitmap at 32-48 px tall | dock & stats only |

Character-packing principle: when text has to be dense, drop case rather than size. Upper-case compressed bitmap fonts stay legible at 6 px where mixed-case falls apart.

### 3.3 Spacing system

The landscape panel splits naturally into a 1:1 halving plus an 8 px gutter. Lock the layout to an 8 px grid with a 4 px half-step and stop debating pixel values in reviews.

- Outer padding: 8 px all edges
- Column gutter: 8 px (gives a 152/8/152 split at 320 wide)
- Text block inner padding: 4 px
- Footer strip: 14 px (y=226..239), reserved for button hints only
- Status rail: 16 px (y=0..15), reserved for link/power chips only

This leaves a useable 320x210 content window with two symmetric 152-wide columns. It is intentionally generous — the screen is small and overcrowding is the bigger risk than wasted space.

### 3.4 Chrome

Two persistent UI elements that never disappear except during full-screen overlays and orange-merge:

- **Top status rail** (y=0..15): link chip (left) + name pill (center) + power chip (right)
- **Bottom footer** (y=226..239): four slots labeled `A` `B` `X` `Y`, positioned physically under the corresponding button, each showing the current action (or blank if unused)

Keeping these nailed to the edges means the center panel can change freely without the user losing orientation.

## 4. The Face System

### 4.1 Anatomy

The mascot is not "a character" — it is a face. The whole device is the head. So the firmware face system runs at the highest layer (on top of normal UI when in "face-forward" modes, and always as an animated overlay in orange-merge).

The mascot face has two graphical elements that matter:

1. Two eye rectangles
2. Optional body-seam black lines

Everything else the mascot reads as (mouth, cheeks, limbs) is physical — printed into the case. The firmware never draws those; the case does.

### 4.2 Eye parameters

Each eye is a parameterized rectangle. Borrowed from the Cozmo/Vector parameter model (Anki, Carlos Baena/Pixar).

```
Eye {
  cx, cy      // center position, in pixels
  w, h        // width, height
  r           // corner radius (0 = hard rect, 4 = full Clawd style)
  tilt        // rotation in tenths of a degree, -900..+900
  lid_top     // how many px the top lid obscures (0 = full open)
  lid_bot     // how many px the bottom lid obscures
  brow_y      // y offset of the brow bar above the eye (INT16_MIN = no brow)
  brow_tilt   // brow angle in tenths of a degree
  brow_len    // length of the brow line in px
  squish      // horizontal squash factor, 0..255 (128 = neutral)
}
```

Two eyes, independent but usually synchronized. Asymmetry is the micro-expression channel — an eyebrow raised 2 px on one side reads as "skeptical" with zero other changes.

**Defaults derived from the real case geometry, not the screen.** This is the critical point: the eyes should be positioned and sized as if the whole 85.5 × 57 mm case is the head, not as if the 320 × 240 display is the head. When orange-merge is active and the display color matches the case, the eyes must sit where the mascot's eyes would sit on the *whole character* for the illusion to land.

Measurements:

- Case main body: 85.5 mm × 57 mm
- Display active area: 57.5 mm × 43.2 mm
- Pixel scale: 320 px / 57.5 mm = 5.565 px/mm horizontal; 240 px / 43.2 mm = 5.556 px/mm vertical (effectively isotropic)
- Case-to-screen offset (assuming centered): 14.0 mm each side horizontally, 6.9 mm top and bottom

Mascot proportions (read from the Clawd sprite reference, conservative values — tune against the printed case under normal room light):

- Eye horizontal position: ~27% and ~73% from case-left → 23.1 mm and 62.4 mm → screen-x ≈ **51 px** and **269 px**
- Eye vertical position: ~35% from case-top → 19.95 mm → screen-y ≈ **73 px**
- Eye width: ~5% of case width → 4.3 mm → **24 px**
- Eye height: ~15% of case height → 8.6 mm → **48 px**
- Corner radius: **4 px** (reads as squared, not round — Clawd-style)

Default parameter set:

- `cx_left = 51, cy = 73, w = 24, h = 48, r = 4`
- `cx_right = 269, cy = 73, w = 24, h = 48, r = 4`
- Center-to-center separation: 218 px (68% of the screen width — eyes sit near the edges)

These are the values that produce "neutral Clawd" on the display when the background is `CASE_ORANGE`. They will also read correctly during normal (non-merge) use when the face is rendered on the dark UI background — the eyes just stop being attached to the case body visually and become part of the on-screen avatar. The tradeoff worth knowing: eyes this far apart eat most of the screen width, so content strips (banner, counters) have to sit in the vertical band *below* the eyes, not between them. The screen layout (§6) is designed around this.

**Tune procedure.** Render a full `CASE_ORANGE` screen with the default eye rects. Place the Pico flat on a white surface. Photograph straight-on in normal desk light. Compare eye positions to the mascot reference sprite overlaid on the photo. Adjust `cx_*` ±4 px, `cy` ±4 px, `w` and `h` ±2 px until the printed face reads as one continuous character. Expect 1-2 iterations.

### 4.3 Expression library

Eighteen base expressions. Each is a target parameter set. Transitions between expressions are tweened, not snapped — the tween is where emotion lives.

| Name | Shape cue | Timing hint | Used for |
| --- | --- | --- | --- |
| `neutral` | Default rects | — | Default idle |
| `happy` | lids_top += 4, lids_bot += 4, squish 140, slight tilt outward | 120 ms ease-out | Boot complete, approval sent |
| `joy` | happy + 2 px squash and 2 px upward bounce | 180 ms spring, 200 ms settle | Celebrate, connected |
| `focused` | h += 4, w -= 2, lids neutral | 80 ms linear | Busy / Claude running |
| `attentive` | wider (w+2), tall (h+4), small separation bounce | 120 ms ease-in, hold | Waiting for user |
| `alert` | w+4, h+6, no lid, tiny inward tilt | 60 ms snap | Approval arrived |
| `skeptical` | left eye brow lowered, right eye brow raised, asymmetric | 200 ms lead-lag | Prompt details shown |
| `sleepy` | lids_top += 12, lids_bot += 4 | 300 ms long ease | Idle auto-dim |
| `asleep` | lids_top = h, (closed) | 400 ms slow close, breathe loop | Napping, disconnected |
| `blink` | lids_top += h/2, lids_bot += h/2, 80ms, reverse | instantaneous return | Every 4-8 s, randomized |
| `wink_l` / `wink_r` | one eye blink only | same as blink | Easter egg, easter egg |
| `squint` | w -= 4, lids_top += 6, lids_bot += 6 | 80 ms | Error, bad prompt data |
| `sad` | lids_top neutral, lids_bot += 6, tilt inward | 250 ms slow | Denial sent, low battery enter |
| `dizzy` | eyes drift in small circles, desynced | 1000 ms loop | `Hold X` easter egg |
| `heart` | h += 4, squish 160, rapid double-blink | 200 ms | Quick-approve burst |
| `scan` | gaze sweeps left→right→left with saccade snap | 600 ms loop | Pairing, searching, thinking |
| `surprise` | h += 8, w += 4, tilt 0, 1 frame delay then blink | 40 ms punch, 120 ms recover | New prompt arrives while idle |
| `proud` | happy, chin-up (eyes shift down 2 px, brow up 2 px) | 200 ms | Level up |

### 4.4 Animation infrastructure

Every expression is a state; transitions are tweens between parameter sets over a duration. Needs one small state machine with:

```
struct EyeAnim {
  Expression from;
  Expression to;
  uint32_t t_start_ms;
  uint16_t duration_ms;
  Easing ease;  // linear, ease_in, ease_out, ease_in_out, spring
};
```

Tween the 11 parameters per eye per frame. At 25 fps that's 22 floats × 25 = 550 updates/s, trivial for RP2350.

**Two background behaviors always layered on top:**

1. **Blink cadence.** Randomized 4-8 s interval, 80-120 ms blink duration. Never predictable; never omitted. This is the single cheapest line of code that makes the device feel alive.
2. **Idle saccades.** Every 1-3 s, shift gaze 1-3 px on one axis, hold 200-400 ms, return. Gives the "thinking / looking" quality without any explicit state.

Both disable during "wide awake" states like `alert` and during overlays where the user is making a decision.

### 4.5 Gaze targeting

Gaze can track a 2D point. `EyeAnim.gaze_x` and `.gaze_y` range -1.0 to 1.0 (per Anki convention) and offset both eye centers by up to ±6 px. Useful for:

- Looking at a button the user should press (subtle)
- Scanning motion for "thinking"
- Looking at the transcript region when a new message arrives

Never use gaze to communicate required data. It is atmosphere, not signal.

## 5. Button Model

### 5.1 Physical map (kept)

```
     +-----------------------------+
  A  |                             |  X     <- top-left / top-right
     |          SCREEN             |
  B  |          320x240            |  Y     <- bottom-left / bottom-right
     +-----------------------------+
```

### 5.2 Global rules

- Every screen owns the footer. Whatever a button currently does, the footer says so. No hidden actions.
- `Y` is always "back / home" unless the screen explicitly overrides and the footer says otherwise. This is the only non-negotiable mapping.
- `Hold A` is reserved globally as "open menu" (system-level escape hatch).
- `Hold Y` is reserved globally as "nap toggle".
- `Hold X` is reserved for the dizzy easter egg.
- `Hold B` is free; currently unused. Reserving for a future "power/cycle" gesture.

### 5.3 Navigation architecture — main cycle kept, content rebuilt

The existing firmware's main-cycle model is right. `A` cycles main screens, `B` pages within, `Y` goes home, `Hold A` opens the system menu. We keep that spine and rebuild what each screen *contains*.

Main cycle:

```
HOME → PET → STATS → USAGE → PERMISSIONS → INFO → HOME
```

Six main screens. Each is a first-class view, not hidden inside Info or a submenu. Any of them is reachable from any other by tapping `A` a few times; each is also the direct destination when `Y` is pressed from within a nested state (always goes back to the last main screen you were on).

Principles:

- `A` is never a destructive or send action on main screens. It cycles. That way muscle memory never kills a session or fires an approval unintentionally.
- `B` is in-screen pagination / confirmation, not navigation.
- `X` is context-aware per screen (transcript scrub, expression preview, risk filter, etc.).
- `Y` is home / back. On Home itself, short-press `Y` opens a toast with the current connection state; hold-`Y` naps.
- `Hold A` opens the **system menu** (settings, demo, reset, about, screen-off, close). This is the only non-cycle route out of the main flow.

Approval overlay and pairing overlay are the two modal interrupts — they suspend whatever main screen is active and resume it on dismiss.

### 5.4 Per-screen action table

| Screen | A | B | X | Y |
| --- | --- | --- | --- | --- |
| Home | next main screen (→ Pet) | page transcript | toggle face-focus view | home-toast (short), nap (hold) |
| Pet | next main screen (→ Stats) | feed | play | back to Home |
| Stats | next main screen (→ Usage) | next stats page | change range (today / 7d / all) | back to Home |
| Usage | next main screen (→ Permissions) | toggle lifetime / today view | — | back to Home |
| Permissions | next main screen (→ Info) | page decision history | filter (all / approved / denied) | back to Home |
| Info | next main screen (→ Home) | next info page | — | back to Home |
| Approval overlay | **APPROVE** | **DENY** | toggle raw details | dismiss overlay back to previous main (prompt stays live) |
| System menu (Hold A) | move selection ↓ | confirm | move selection ↑ | close menu |
| Settings | move selection ↓ | confirm / cycle value | move selection ↑ | back |
| Reset | move selection ↓ | arm → confirm | — | back |
| Demo mode | step forward | step backward | auto/manual toggle | exit demo |
| Dock clock | dismiss back to Home | — | — | menu (hold) |
| Sync / pairing | unpair | — | — | cancel (disconnect) |

**Why pet stays in the main cycle.** Pet is not bonus content — it is one of the main ways the device expresses personality when Claude is idle. Hiding it behind the system menu makes it feel like a feature to discover, which buries the character work. Keeping it one tap away from Home is cheap and on-brand.

**Why Pet comes right after Home.** Left-to-right priority: the user's eye is on Home, one tap of the most-used button shows personality, then another tap shows numbers. Personality before data. That ordering is consistent with treating the case as a character, not a dashboard.

### 5.5 Footer label anchoring

The footer is NOT a single label strip. It is four label slots, each physically under its button:

```
+--------+--------+--------+--------+
|   A    |   B    |   X    |   Y    |
|  NEXT  |  PAGE  |  FACE  |  HOME  |
+--------+--------+--------+--------+
```

On Home specifically:

```
A: NEXT    B: PAGE    X: FACE    Y: HOME
```

When the Home transcript has multiple pages, `B` label becomes `PAGE 1/3`. Hidden during merge / boot / nap.

## 6. Screen Map

### 6.1 Overview

```
    BOOT → SYNC → ┌─ HOME ─── PET ─── STATS ─── USAGE ─── PERMISSIONS ─── INFO ──┐
                  │    ↑                                                          │
                  │    └──────────────────────  A cycles  ────────────────────────┘
                  │
                  ├─ APPROVAL OVERLAY  (modal, triggered by protocol)
                  ├─ SYSTEM MENU (Hold A from any main screen)
                  │    ├─ SETTINGS
                  │    │    └─ RESET
                  │    ├─ DEMO MODE
                  │    ├─ ABOUT
                  │    ├─ SCREEN OFF
                  │    └─ CLOSE
                  └─ DOCK CLOCK  (auto, USB + idle + time-synced, interrupts Home only)
```

Pet sits in the main cycle — a tap of `A` from Home lands you on it. Stats, Usage, Permissions, and Info are the next four main screens; they're discoverable by cycling, not hidden behind a menu. The system menu (`Hold A`) is reserved for chrome: settings, demo, about, screen-off.

### 6.2 Boot screen

**Purpose.** Establish identity in under 2 seconds. Let the device look like itself before it does any work.

**Sequence** (total ~2200 ms):

1. T=0: full orange-merge fill, eyes closed. No text. (100 ms)
2. T=100: eyes open (lid tween), settle to neutral. (300 ms)
3. T=400: single blink. (100 ms)
4. T=500: status text fades in beneath eye region: `CLAUDE PICO` in white, centered, bitmap14. (200 ms)
5. T=700: small tick text `booting...` in TEXT_DIM. (hold)
6. T=~1800 (when BLE stack up): text morphs to `ready`. Eyes do `happy` expression briefly.
7. T=~2000: orange-merge dissolves to Home background (radial or top-down wipe, 200 ms).

Boot screen never uses the status rail or footer. It is pure identity time.

### 6.3 Sync screen (first connect or reconnect)

Appears when link is connecting but not yet secure. Persists as long as `nus_linked && !secure`.

```
+------+------+--------------------------+------+------+
|      |                                        0%  bat |   ← status rail (LNK: pairing)
|                                                        |
|       [eyes doing scan animation]                      |
|                                                        |
|                  Pairing with Claude                   |
|                   passkey: 123456                      |
|                                                        |
|                                                        |
|    UNPAIR          —            —        CANCEL        |   ← footer, A/B/X/Y
+--------------------------------------------------------+
```

Note: passkey displayed in TEXT_WARM at 24 px tall, hard-kerned. This is security-critical and should be the largest number on the device by a wide margin.

### 6.4 Home screen

Home is the default when linked. Two-column but asymmetric (not the current left=face, right=content split — the face owns the whole panel and content is underlaid/overlaid).

```
+------ LNK secure ----- CLAUDE PICO ----- USB 82% ------+   y=0..15
|                                                        |
|                                                        |
|                     [FACE: eyes]                       |
|                                                        |
|                                                        |
|   banner: "Buddy connected"                            |   banner at y=160
|                                                        |
|   T 3   R 1   W 0     today 12k tok                    |   stats strip at y=180
|                                                        |
|   > reading index.md                                   |   transcript at y=200
|   > searching docs...                                  |
+----- NEXT ------- PAGE ------- FACE ----- HOME --------+   y=226..239
```

**Why the face dominates.** The case is a face. Placing a tiny pet in the corner and filling the panel with data undoes the physical illusion. The face is the device; data is secondary and lives in a band under the eyes.

**When the face becomes data.** When `running > 0` or `waiting > 0`, the face switches expression (`focused` / `attentive`). That expression change IS the status indicator. The text banner still shows, but becomes redundant for trained users — exactly the Anki principle.

**Eye positioning on Home.** Eyes render at the default case-aligned coordinates (cx_left=51, cx_right=269, cy=73, see §4.2). The stats strip and transcript sit below the eye band; there is no content *between* the eyes, because the physical mascot has no content there either. Respecting that empty band is what keeps the case illusion working even when the display is dark rather than merged.

**Transcript handling.** Transcript lines occupy y=200..222. If more lines than fit exist, footer `B` label shows `PAGE 1/3`. If transcript is disabled in settings, banner moves down and transcript band disappears. The transcript never overtakes the face band.

### 6.5 Approval overlay

This is the single most important screen on the device. Spend the design budget here.

Triggered by heartbeat `prompt` object arriving. The data available is ONLY `{id, tool, hint}` — no path, command text, or diff. Be honest about what we can render.

**Layout** (replaces whole Home content except status rail):

```
+------ LNK secure ----- CLAUDE PICO ----- USB 82% ------+
|                                                        |
|                 CLAUDE NEEDS PERMISSION                 |   header, 14 px white
|                                                        |
|  TOOL    Read                                           |   label row, 8 px dim + 14 px warm
|                                                        |
|  HINT    read path /home/amar/.ssh/config               |   wraps 2 lines max
|                                                        |
|                [ eyes: attentive ]                     |   eyes render small, top-right
|                                                        |
|  WAITED  00:14                                         |   timer, counts up, 8 px
|                                                        |
+--- APPROVE -------- DENY -------- DETAILS ----- HOME --+
```

**Color strategy.**

- Header text white.
- `TOOL` value colored by risk tier (see §6.5.2).
- Background stays `BG_INK` even in orange-merge-capable frames — do NOT merge an approval screen; it should stop the user.
- LED goes amber and pulses at 1 Hz. When a decision is sent, LED flashes green (approve) or red (deny) once and goes back to its linked color.

**Raw details toggle (X).** Shows the raw JSON payload in a monospace panel. Useful for power users; default is off.

**Sent-state.** After A or B, button labels grey out and the timer is replaced with `sent...`. The overlay persists until the next heartbeat clears the `prompt` field (consistent with existing firmware).

#### 6.5.1 Tool-aware prompt rendering

Protocol gives us `prompt.tool` as a string. We hash it into risk tiers locally:

| Tier | Example tools | Color | Default expression | Extra UI |
| --- | --- | --- | --- | --- |
| SAFE | `Read`, `Glob`, `Grep`, `WebSearch` | `OK_GREEN` | focused | none |
| NEUTRAL | `WebFetch`, `TodoWrite`, `Skill` | `TEXT_PRIMARY` | attentive | none |
| CAUTION | `Edit`, `Write`, `NotebookEdit`, `TaskCreate` | `WARN_AMBER` | alert | 500 ms entry shake |
| DANGER | `Bash`, anything unknown | `DANGER_RED` | alert + squint | 500 ms entry shake, LED pulse 2 Hz |

Honest caveat: this is a local heuristic. The protocol never marks a tool as dangerous. An `Edit` can be harmless, a `Bash` can be `ls`. The tiering exists to shift the approval bar — CAUTION/DANGER should feel like they want more attention, not to block the user. Tuning this without being paternalistic is the design risk.

#### 6.5.2 Timing

Approval timer counts up from prompt arrival. Color shifts:

- 0..10 s: dim grey
- 10..30 s: white
- 30 s+: amber
- 60 s+: red, LED at 2 Hz

This surfaces "I've been waiting a while" without the desktop needing to tell us.

### 6.6 Stats screen (main cycle)

**Data sources (real).** From persisted `PersistentBlob` + live heartbeat.

| Metric | Source | Honest? |
| --- | --- | --- |
| Approvals total | `approvals` | yes |
| Denials total | `denials` | yes |
| Approval rate | `approvals / (approvals + denials)` | yes |
| Mean time to decide | computed from prompt-arrival to decision timestamps | needs new plumbing, feasible |
| Response velocity | `velocity[8]` ring | yes (token velocity) |
| Level | `level` | local fiction (tokens_per_level) — label clearly |
| Nap time | `nap_seconds` | yes |
| Total tokens | `tokens_total` | yes |
| Tokens today | heartbeat `tokens_today` | yes |

**Layout.** Paginated via `B`. `A` cycles to next main screen (Usage); `X` changes the time range for applicable metrics; `Y` returns home.

```
+------ LNK secure ------ STATS -------------- BAT 82 ---+
|                                                        |
|     APPROVED          DENIED         RATE              |
|       142               18          88%                |   large numerals
|                                                        |
|     MEAN TIME-TO-APPROVE     12.4s                     |
|     LONGEST WAIT             00:47                     |
|     NAP TIME                 03h 21m                   |
|                                                        |
|     TOKENS TOTAL             1.4M                      |
|     TOKENS TODAY             12,481                    |
|     LEVEL (local)            28                        |
|                                                        |
|     [small velocity sparkline, 8 bars]                 |
|                                                        |
+----- NEXT -------- PAGE -------- RANGE ----- HOME -----+
```

**Gap being flagged:** "Level" is a local gamification fiction. Labeling it `(local)` in TEXT_DIM tells the truth.

### 6.7 Usage screen (main cycle)

This is the screen with the biggest honest-design problem.

**What the user asked for.** A "Usage Limits" screen.

**What the protocol exposes.** `tokens` (cumulative), `tokens_today` (day). That's it. No weekly Claude Pro/Max quota, no rate limits, no context-window percent, no remaining-messages signal. See §9.2 for verification.

**What we can honestly show.**

- `tokens_today` with a big numeral
- `tokens` lifetime
- Velocity trend (how fast tokens are being consumed right now)
- A derived "on pace for X tokens today" projection at mid-day, based on the last hour

**What we refuse to fabricate.**

- A "weekly limit bar" that pretends to know the Claude Pro quota
- A "messages remaining" counter
- A "rate limit distance" chip

**Proposed name.** Rename `USAGE LIMITS` to `USAGE` (just drop "limits"). The screen answers "how much am I using?" honestly without pretending to know "how much am I allowed?".

**Layout:**

```
+------ LNK ------------- USAGE --------------- BAT ----+
|                                                        |
|                     TOKENS TODAY                       |
|                      12,481                            |   very large numeral
|                                                        |
|     PROJECTED EOD                                      |
|     ~28,400                       [only if > 3 h data] |
|                                                        |
|     VELOCITY (tok/min)       186                       |
|     PEAK TODAY               1,420 tok/min @ 10:42     |
|                                                        |
|     LIFETIME                 1.4M tokens               |
|                                                        |
+----- NEXT -------- VIEW -------- ------- ----- HOME --+
```

**Upgrade path.** If Amar can push a host-side protocol extension to include `quota_pct` or `limit_remaining`, this screen picks it up as a top strip. Until then, no fake bars.

### 6.8 Permissions screen (main cycle)

New screen. Uses the existing `approvals` / `denials` counters but adds a ring buffer of the last N decisions.

**Storage cost.** Each entry = `{timestamp: 4B, tool_hash: 1B, decision: 1B, wait_ms: 2B}` = 8 B. A 64-deep ring = 512 B. Fits in RAM; persisting it is optional and deferred.

**Layout:**

```
+------ LNK ------------ PERMISSIONS ---------- BAT ----+
|                                                        |
|  10:42  Edit          APPROVED       02.1s             |
|  10:38  Bash          DENIED         05.8s             |
|  10:31  Read          APPROVED       00.9s             |
|  10:30  Read          APPROVED       00.4s             |
|  10:28  Read          APPROVED       00.3s             |
|  10:21  WebFetch      APPROVED       01.2s             |
|  10:19  Edit          APPROVED       03.6s             |
|  10:05  Bash          DENIED         08.1s             |
|                                                        |
|  filter: ALL                                          |
|                                                        |
+----- NEXT -------- PAGE --------- FILTER ---- HOME ---+
```

Color: decision word tinted green/red. Tool name tinted by risk tier (same rules as §6.5.1).

**Value prop.** Lets the user audit: "wait, did I really approve that Bash call earlier?" This is a real security-hygiene use case that the device is uniquely positioned to support.

### 6.9 Info screen (main cycle)

Keep the current 7-page model (`About`, `Buttons`, `States`, `Claude`, `Device`, `Bluetooth`, `Build`). It works. New plan just restyles them with the Stats/Usage visual language for consistency.

Small addition: `Bluetooth` page should prominently show the bonded peer name and MAC, with a `clear bonds` action available via B-hold. Makes the "who is this paired to?" question answerable on-device.

### 6.10 Pet screen (main cycle)

Pet is the second stop in the main cycle (Home → **Pet** → Stats → ...). One tap of `A` from Home lands here. It is not a mode and it is not a menu item. It's a first-class screen with a distinct design brief: **show the character's inner state, not the protocol state.**

```
+------ LNK secure ----- CLAUDE PICO ----- USB 82% ------+   y=0..15
|                                                        |
|                [eyes: joy / neutral / idle             |
|                       depending on pet mood]           |
|                                                        |
|   mood:  happy                    age:  3d 14h         |
|   fed:   ████████░░  82%          feeds: 128           |   stat rows
|   energy:██████░░░░  64%          plays: 47            |
|                                                        |
|   streak:  12 approvals in a row                       |
|                                                        |
+----- NEXT ------- FEED ------- PLAY ------ HOME -------+
```

**Why Pet stays in the cycle, not the menu.** The character is not a feature to opt into. The case is a physical mascot on a desk; hiding the animated personality behind a menu buries what the device is *for* when Claude isn't actively driving it. Current firmware gets this right — this plan keeps it right.

**What changed from the current Pet screen.**

- Visual language snaps to the refined Anthropic palette (black ink background, warm ivory text, orange accent only on the mood label when celebrating). No orange-merge here during normal use — too loud if you're cycling to it every few minutes.
- Stats are paired left/right on the same row instead of stacked in a single column. Reads faster.
- `B` directly feeds, `X` directly plays. No sub-pages, no reaching into a menu. Pet engagement has to be one button press or it doesn't happen.
- Eyes in the face band do real pet-mood-driven expressions — the face is the pet; the stats are the dashboard.

**Pet stats semantics.**

- `fed`: ticks down by 1%/minute while screen is on; feed with `B`, snaps to 100.
- `energy`: ticks down with activity (approvals, button presses); nap or sleep recovers.
- `mood`: computed from fed + energy + recent approvals (uses real `approvals` and `denials` counters).
- `streak`: consecutive approvals without a denial, resets on deny. Real, from existing counters.
- `age`: monotonic, persisted across power cycles.

**Honest note.** The pet's stats are synthetic local state. They aren't "what Claude is doing" — they're "what the character on your desk is doing." That's fine as long as the UI doesn't pretend the pet's hunger reflects anything in the protocol. The stat names (`fed`, `energy`) are character-coded, not data-coded, so the framing holds.

**Interaction on Pet screen.** `A` cycles forward to Stats. `B` feeds (one-shot animation: eyes do `joy`, fed fills). `X` plays (eyes sweep + small bounce, energy drains a bit, mood lifts). `Y` goes home.

### 6.11 Settings

Keep the current settings list. Re-style with new visual language. Add one row:

- `face: animated / static` — lets user disable all idle saccades and blink micro-behaviors (useful for video recording or low-power)

### 6.12 Dock clock

Keep the current dock-clock logic (USB + time-synced + idle + home). Redesign visuals:

- Clock glyphs in very large 7-segment bitmap, orange on black
- Eyes in a small strip above the clock doing slow idle blinks
- Owner name below in TEXT_WARM

This turns the dock clock into a "sleeping Clawd with a clock face" — a legitimate standalone desk object.

### 6.13 Demo Mode

A dedicated mode that walks through every screen, every expression, and every transition on the device. Purpose: showcase the build for a viewer (friend, coworker, conference booth, camera) without needing Claude Desktop to be driving real work. Also the single best QA tool for the firmware — if every scene renders correctly in demo mode, the UI is healthy.

**Entry / exit.** Reachable from the System Menu (`Hold A → Demo Mode → B`). Entry and exit both use the long `merge_wipe` transition (~1 s each way) — demo mode is where the orange-merge flourish earns its keep. On exit, return to whichever main screen was active when demo was entered.

**Driving data.** Demo mode does not touch real protocol state. The existing firmware already carries a `demo_scenarios[]` table (`src/claude_buddy_pico.cpp:~132`) that synthesizes heartbeats — extend that table to cover every new screen and prompt shape rather than building a parallel system. Any real heartbeat that arrives during demo mode is queued and applied on exit; demo does not corrupt live state.

**Scene list** (order matters — arranged for a coherent narrative):

1. Orange-merge boot (100 ms merge, 300 ms eyes-open, 200 ms name fade-in)
2. Sync / pairing screen with `scan` expression, fake passkey `123456`
3. Home screen, idle, `neutral` expression, blinks and saccades
4. Home screen, busy, `focused` expression, running count climbing
5. Home screen, transcript filling, page indicator appearing
6. Approval overlay — SAFE tier (`Read`), short hint, auto-approves at 3 s
7. Approval overlay — NEUTRAL tier (`WebFetch`), longer hint
8. Approval overlay — CAUTION tier (`Edit`), alert expression, amber LED
9. Approval overlay — DANGER tier (`Bash`), squint expression, red LED pulse, auto-denies at 4 s
10. Heart expression burst (quick-approve feedback)
11. Pet screen, `joy` expression, fake feed animation
12. Pet screen, `sleepy` expression, energy ticking down
13. Stats screen page 1 (counters)
14. Stats screen page 2 (velocity sparkline)
15. Usage screen (tokens-today climbing animation)
16. Permissions history screen, ring buffer pre-filled with 8 fake entries, filter cycle
17. Info screen — one page per tick
18. Dock clock (simulated USB + time-synced)
19. Nap — `asleep` expression, backlight dim, breathing loop
20. Easter eggs: `dizzy`, `wink_l`, `wink_r`, `surprise`, `proud`
21. Orange-merge flourish — full merge with all 18 eye expressions cycling at 300 ms each, ~5.4 s total

Total auto-cycle runtime: ~90 seconds. Each scene holds for ~3-5 s in auto mode; manual mode advances only on button press.

**Button controls while in Demo Mode:**

| Button | Action |
| --- | --- |
| A | step forward to next scene |
| B | step backward to previous scene |
| X | toggle auto / manual advance |
| Y | exit demo mode (merge_wipe back to origin screen) |

Footer always shows `STEP+` / `STEP-` / `AUTO` (or `MANU`) / `EXIT`. No hidden actions.

**Scene overlay.** While demo runs, a small chip in the status rail reads `DEMO N/21` so the viewer (and the user) always know this is synthesized. This is the non-negotiable honesty flag — demo mode must never be mistakable for a live device.

**Why demo matters for this build specifically.** The character work in this plan (orange-merge, 18 expressions, risk tiers, ring-buffer history) is the part that's easiest to show off and hardest to stumble into in normal use. Without a demo mode, 80% of the personality work is invisible unless the user happens to hit the right edge case. Demo mode makes the character budget visible and reviewable.

## 7. Animation System

### 7.1 Tick budget

At 25 fps (40 ms tick), per-frame budget:

- BLE / protocol poll: up to 10 ms
- Eye tween update (22 floats): <1 ms
- Framebuffer blit: ~5-10 ms for partial, ~15 ms full-screen
- Remaining: ~15 ms headroom

Plenty of room. Stop worrying about animation cost; worry about flash-write cost (those stall the main loop).

### 7.2 Layer stack

Conceptual draw order per frame:

1. Background (bg_ink or case_orange during merge)
2. Status rail (top 16 px)
3. Main content (screen-specific)
4. Face overlay (eyes) — render position depends on current screen
5. Banner / toast (transient)
6. Modal overlay (approval / menu / settings)
7. Footer (bottom 14 px)

Each layer can be dirtied independently. A blink only dirties ~32x48 px in the eye bounding box.

### 7.3 Transitions

A small library of transitions between screens/modes. Goal: each transition takes under 300 ms so the UI stays snappy.

- `fade` (200 ms): whole panel lerp alpha → done. Expensive; avoid on full screens.
- `wipe_down` / `wipe_up` (200 ms): 4 px bands march. Cheap.
- `merge_wipe` (400 ms): panel fills with `CASE_ORANGE` from center outward (diamond), holds 80 ms, then eyes reposition and screen dissolves back. Reserved for mode-level transitions (Pet enter/exit, factory reset, boot).
- `chip_pulse` (150 ms): just a status chip blinks color once — cheapest transition, used for "status changed" without a full animation.

## 8. Data Source Mapping

For every element on every screen, this table says which protocol field drives it and whether the value is trustworthy.

| UI element | Protocol source | Trust |
| --- | --- | --- |
| Name pill | `status.data.name` + local `device_name` | real |
| LNK chip (idle/linked/secure/pairing) | `status.data.sec` + BLE link state | real |
| USB chip | `status.data.bat.usb` | real |
| Battery % | `status.data.bat.pct` | estimate (voltage only) |
| Battery mA | `status.data.bat.mA` | currently hardcoded 0 — suppress until real |
| Total sessions | heartbeat `total` | real |
| Running | heartbeat `running` | real |
| Waiting | heartbeat `waiting` | real |
| Message banner | heartbeat `msg` | real |
| Transcript lines | heartbeat `entries[]` | real |
| Tokens today | heartbeat `tokens_today` | real |
| Tokens lifetime | persisted `tokens_total` | real |
| Velocity | persisted `velocity[8]` ring | real |
| Turn content | `evt:"turn"` (capped ~4KB) | real but heterogeneous — summarize, don't parse deeply |
| Prompt id | heartbeat `prompt.id` | real |
| Prompt tool | heartbeat `prompt.tool` | real |
| Prompt hint | heartbeat `prompt.hint` | real |
| Risk tier | derived locally from `tool` name | **local heuristic, label as such** |
| Level | `kTokensPerLevel` fiction | **local, label as `(local)`** |
| Mood | derived from fed + energy + approvals | **synthetic** |
| Time / date | time-sync `[epoch, offset]` | real |
| Heap | `status.data.sys.heap` | hardcoded 0 — suppress until real |
| Usage limit % | **not in protocol** | **do not fabricate** |
| Quota remaining | **not in protocol** | **do not fabricate** |

## 9. Honest Gaps

### 9.1 Usage limits are not in the protocol

`REFERENCE.md` exposes `tokens` and `tokens_today`. No quota, no limit, no rate-limit header. Any "usage limits" screen that shows a percent-full bar is a lie. Options:

- A: Rename screen to `USAGE` and show only what we measure (recommended).
- B: Ship a host-side protocol extension (`quota`, `limit`) and surface it when available.
- C: Lie. Not recommended.

### 9.2 Prompt context is extremely thin

Prompt = `{id, tool, hint}`. We cannot show the file path, command, or diff unless it's in `hint`. Tool-aware UX is the best we can do. Don't promise the user a "preview" we can't render.

### 9.3 Turn events are capped

`evt:"turn"` is ~4 KB cap per upstream convention. We cannot do an on-device session browser. We can do a short "last turn digest" banner. The advanced-firmware review flagged this as a P0 opportunity — still true.

### 9.4 Battery telemetry is not precise

VSYS-only. The doc's "battery is a comfort signal" principle holds. Visualize as 5-step chip, not a percentage with decimals.

### 9.5 Orange-merge requires physical calibration

No amount of correct hex values will fix a mismatch between LCD backlight, panel gamut, filament batch, and ambient lighting. Plan for a calibration pass — eyeball + photo.

### 9.6 RGB332 palette truncates ~everything

Anthropic's actual brand orange `#DA7756` quantizes cleanly, but fine gradients (ivory on orange, warm greys) will band. Don't design with web-quality gradients in mind. Design with chips, blocks, and hard edges.

## 10. Implementation Order

Phased, so each phase is independently reviewable and leaves the device working.

**Phase 1 — Foundation (no user-visible changes).** Refactor current source to split face rendering, screen rendering, and transition system into three separate modules. Introduce the eye parameter struct. Run existing UI on top of the new face engine.

**Phase 2 — Orange-merge + boot.** Implement `CASE_ORANGE` constant, boot screen, merge-wipe transition, basic eye expressions (neutral, blink, scan, happy).

**Phase 3 — Home redesign.** New layout with face-dominant Home, status rail, footer slot labels. Keep approval overlay unchanged behaviorally.

**Phase 4 — Approval overlay rebuild.** Tool-aware rendering, risk tiers, LED cadence, timer.

**Phase 5 — Stats / Usage / Permissions main screens.** Three new first-class main-cycle screens. Implement permissions ring buffer.

**Phase 6 — Pet screen redesign.** Stays in the main cycle (no structural move); restyle against the new visual language, pair stat rows, wire B=feed / X=play directly, hook pet mood to eye expression.

**Phase 7 — Info restyle + settings additions.** Lowest priority; visual polish.

**Phase 8 — Demo Mode.** Extend the existing `demo_scenarios[]` table to cover every new scene (§6.13). This ships last because it depends on every screen and expression being implemented first.

**Phase 9 — Animation polish.** Full expression library tuned, saccades, idle micro-movements, spring easing, final merge-wipe timing.

If the scope is too big, cut after Phase 4. Boot + Home + Approvals is a complete device; Stats / Usage / Permissions / Pet / Demo are upsells in descending order of daily utility.

## 11. Open Decisions for Amar

These are choices the plan deliberately leaves to you. Flagging so they don't become surprises during implementation.

1. **Orange-merge calibration.** Need a printed-case photo under normal desk lighting to pick the pixel hex. I can draft a calibration procedure (4-5 test hexes rendered full-screen, photograph each next to the case, pick the closest) as a follow-up artifact if useful.
2. **Risk-tier tool list.** The SAFE/CAUTION/DANGER mapping in §6.5.1 is my heuristic. You should review and edit — you know which tools your workflow uses most.
3. **Level numbering.** Remove entirely (honest), keep and label `(local)` (honest but retained), or reframe as "streak" / "days active" (more honest). Minor choice; pick whichever fits your sense of the device.
4. **Hold-B reservation.** Currently unassigned. Proposing to leave it reserved. If you have a specific action in mind (e.g., "toggle LED on/off without entering settings"), assign it now.
5. **Usage limit protocol extension.** If you can get a host-side change merged to push quota data, the Usage screen upgrades itself. Worth a one-line PR against the upstream if you care about the data.
6. **Face-only Home.** Home as proposed has face band + data band. A more radical version would have ONLY the face (no transcript, no counters) on Home and move all data into overlays. It feels right for the case aesthetic but costs ambient glanceability. Your call.
7. **Demo Mode scene count and runtime.** 21 scenes, ~90 s auto-cycle is a rough target. If that feels too long for a casual demo, consider a "short demo" (just boot + Home + one approval + pet + flourish, ~20 s) gated behind a `Hold X` in the demo itself, with the full 21-scene version as the default.
8. **Demo Mode auto-start.** Should demo mode auto-launch if the device boots without ever seeing a BLE link in the first 60 seconds (i.e., on a new user's first power-on, they get the tour)? Argues for discoverability; argues against being annoying for someone who's just temporarily disconnected. Default in this plan: **no auto-start**, user has to choose it. Worth reconsidering.

## 12. What This Plan Deliberately Does Not Do

- It does not write any firmware code.
- It does not implement GIF pet packs (explicitly scoped out per the capability review).
- It does not propose OTA updates (same).
- It does not propose Wi-Fi features (tight scope — Wi-Fi is P1, not P0).
- It does not copy the M5Stick upstream UI shape. Where this plan departs from upstream, the depart is intentional.

## References

- `docs/advanced-firmware-capability-review.md` — hardware + protocol audit this plan is built on
- `docs/feature-matrix.md` — current upstream-to-Pico feature mapping
- `docs/protocol-map.md` — current button + state + LED map
- `docs/user-guide.md` — user-facing doc for current firmware (useful diff reference)
- `anthropics/claude-desktop-buddy/REFERENCE.md` — upstream protocol source of truth
- Anki/Vector design philosophy (Baena): two rects are enough when timing does the work
- `randym32.github.io/Anki.Vector.Documentation` — eye-animation parameter inspiration
- `github.com/playfultechnology/esp32-eyes` — open-source reference implementation of Cozmo-style eye rendering
