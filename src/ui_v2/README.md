# V2 UI module tree

This directory is the implementation of `docs/new-firmware-ui-plan.md`.

It is **deliberately separate** from the V1 monolith (`src/claude_buddy_pico.cpp`). `docs/project-plan.md` §2.2 says V1 ships with the monolith intact; §3 says V2 code lives on a branch until V1 is tagged. This tree honors both constraints by:

1. Not modifying `claude_buddy_pico.cpp` at all. V1 builds and flashes identically.
2. Being independently compilable — the module tree has no include dependency on the V1 state struct.
3. Offering an explicit integration surface (`ui_core.h`) that V1 can opt into behind a `BUDDY_UI_V2` CMake option when V2 is ready to take over.

## Layout

```
src/ui_v2/
├── README.md               — this file
├── palette.h               — RGB332-safe palette per plan §3.1
├── geometry.h              — layout constants per plan §3.3 + case-derived eye positions §4.2
├── gfx.h / gfx.cpp         — thin text / chip / rounded-rect helpers over PicoGraphics
├── eye.h / eye.cpp         — Eye parameter struct + rounded-rect renderer
├── expression.h / .cpp     — 18 expression parameter sets per plan §4.3
├── face.h / face.cpp       — face engine: tween, blink cadence, saccades
├── transitions.h / .cpp    — merge_wipe, chip_pulse, wipe_down, fade
├── risk_tier.h / .cpp      — tool-name → SAFE/NEUTRAL/CAUTION/DANGER classifier
├── permissions_log.h / .cpp — 64-entry ring buffer of approval decisions
├── ui_state.h              — read-only state snapshot the screens consume
├── ui_core.h / ui_core.cpp — main dispatcher: picks screen, runs face, commits frame
└── screens/
    ├── screen.h            — base Screen interface
    ├── boot.*              — §6.2 boot sequence
    ├── sync.*              — §6.3 pairing screen with large passkey
    ├── home.*              — §6.4 face-dominant home
    ├── approval.*          — §6.5 tool-aware approval overlay
    ├── stats.*             — §6.6 paginated stats
    ├── usage.*             — §6.7 tokens-today / projected EOD
    ├── permissions.*       — §6.8 decision history
    ├── info.*              — §6.9 7-page reference
    ├── pet.*               — §6.10 pet in main cycle
    ├── settings.*          — §6.11 settings list
    ├── reset.*             — reset submenu
    ├── system_menu.*       — Hold-A system menu
    ├── dock_clock.*        — §6.12 dock clock
    └── demo.*              — §6.13 demo mode scene runner
```

## Integration — when V1 is ready

`ui_core.h` exposes three entry points the V1 monolith (or a V2-era rewrite) hooks into:

```cpp
ui_v2::core::init(pimoroni::PicoGraphics* gfx, uint32_t initial_seed, uint32_t now_ms);
ui_v2::core::tick_40ms(const ui_v2::BuddyInputs& in, ui_v2::BuddyOutputs& out, uint32_t now_ms);
ui_v2::core::on_button_event(ui_v2::Button, ui_v2::ButtonEdge, uint32_t now_ms);
```

The V1 monolith remains authoritative for BLE, pairing, protocol parsing, persistence, and the `BuddyState` struct. V2 reads a `BuddyInputs` snapshot the monolith fills once per tick; V2 writes button-driven intents into `BuddyOutputs` that the monolith then acts on (send approve/deny, mark dirty for persist, etc.).

The integration recipe is in `docs/v2-ui-integration.md`.

## Testing path

Because V2 is isolated, you can test it without flashing:

1. Build the V1 target (`claude_buddy_pico`) as you do today — unaffected.
2. Build the `claude_buddy_pico_v2_smoke` target that renders V2 screens against a synthetic input bus. This lets you iterate on layouts on real hardware without any BLE or protocol state.
3. When a screen looks right on hardware, flip the `BUDDY_UI_V2=ON` CMake flag to route the production firmware through V2.

Rollback is a one-line CMake flip.

## Honesty notes

- V2 is **untested firmware**. It compiles against the same Pimoroni API V1 uses and follows the same patterns, but neither I (Claude) nor the repo CI has flashed it to a Pico. Bench work will surface issues. The code is structured so those issues are local — a bad `eye::render` function won't break BLE.
- The plan document is the source of truth. If this tree deviates from the plan, the plan wins; file an issue against the code.
- The V1 monolith is not dead code. Most of what V2 needs (BLE handlers, protocol parsing, persistence layout, demo scenario table) stays in V1. V2 is a UI-layer replacement, not a rewrite of the whole firmware.
