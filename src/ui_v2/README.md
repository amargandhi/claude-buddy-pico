# V2 UI module

The V2 UI engine for Claude Buddy Pico. Renders the face, screens, transitions, and approval flow against the 320×240 Display Pack. Selected at build time with `BUDDY_UI_V2=1` (on by default in `CMakeLists.txt`).

## Layout

```
src/ui_v2/
├── README.md               — this file
├── palette.h               — RGB332-safe colour palette
├── geometry.h              — layout constants and case-derived eye positions
├── gfx.h / gfx.cpp         — text / chip / rounded-rect helpers over PicoGraphics
├── eye.h / eye.cpp         — Eye parameter struct and renderer
├── expression.h / .cpp     — 18 expression parameter sets
├── face.h / face.cpp       — face engine: tween, blink cadence, saccades
├── transitions.h / .cpp    — merge_wipe, chip_pulse, wipe_down, fade
├── risk_tier.h / .cpp      — tool-name → SAFE/NEUTRAL/CAUTION/DANGER classifier
├── permissions_log.h / .cpp — 64-entry ring buffer of approval decisions
├── ui_state.h              — read-only state snapshot the screens consume
├── ui_core.h / ui_core.cpp — dispatcher: picks screen, runs face, commits frame
└── screens/
    ├── screen.h            — base Screen interface
    ├── boot.*              — boot sequence
    ├── sync.*              — pairing screen with large passkey
    ├── home.*              — face-dominant home
    ├── approval.*          — tool-aware approval overlay
    ├── stats.*             — paginated stats
    ├── usage.*             — tokens-today / projected EOD
    ├── permissions.*       — decision history
    ├── info.*              — 7-page reference
    ├── pet.*               — pet in main cycle
    ├── settings.*          — settings list
    ├── reset.*             — reset submenu
    ├── system_menu.*       — Hold-A system menu
    ├── dock_clock.*        — dock clock
    └── demo.*              — demo mode scene runner
```

## Integration surface

`ui_core.h` exposes three entry points:

```cpp
ui_v2::core::init(pimoroni::PicoGraphics* gfx, uint32_t initial_seed, uint32_t now_ms);
ui_v2::core::tick_40ms(const ui_v2::BuddyInputs& in, ui_v2::BuddyOutputs& out, uint32_t now_ms);
ui_v2::core::on_button_event(ui_v2::Button, ui_v2::ButtonEdge, uint32_t now_ms);
```

`src/claude_buddy_pico.cpp` remains authoritative for BLE, pairing, protocol parsing, persistence, and the `BuddyState` struct. It fills a read-only `BuddyInputs` snapshot once per tick and acts on the `BuddyOutputs` V2 writes back (approve/deny, persist-dirty, LED, backlight).

## Build targets

- `claude_buddy_pico` — main firmware; compiled with `BUDDY_UI_V2=1` by default. Build with `-DBUDDY_UI_V2=OFF` for the V1 fallback UI.
- `claude_buddy_pico_v2_smoke` — V2 screens against a synthetic input bus. Flashable without BLE or protocol state; useful for iterating on layouts on real hardware.
