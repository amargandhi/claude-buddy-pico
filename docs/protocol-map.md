# Protocol And UI Map

## Final Button Map

- `A`: next main screen, approve prompt, advance selection in overlays
- `B`: next page or transcript chunk, deny prompt, choose in overlays
- `X`: cycle pet normally, toggle raw prompt details during approvals
- `Y`: home / back
- `Hold A`: open or close the main menu
- `Hold X`: one-shot `dizzy`
- `Hold Y`: nap / wake
- `LiPo SHIM button`: hardware-only power

## Screen Model

The Pico port keeps the upstream Buddy model, but adapts it to the larger landscape display:

- left column: pet + state banner
- right column: `Home`, `Pet`, or `Info`
- centered overlays: prompt, menu, settings, reset

## Buddy State Mapping

- `sleep`: disconnected, stale heartbeat, manual nap, or screen-off state
- `idle`: linked and calm
- `busy`: `running > 0`
- `attention`: `waiting > 0` or prompt pending
- `celebrate`: completion / level-up one-shot
- `dizzy`: manual easter egg via `Hold X`
- `heart`: quick approval one-shot

## LED Map

- `off`: asleep / not linked
- `green`: linked and calm
- `blue`: Claude is running work
- `amber`: approval pending
- `magenta`: pairing or character install
- `red`: battery low

The current firmware intentionally drives the LED below full power so it is visible without overwhelming the display in a dim room.

## Protocol Commands Implemented

Required Buddy commands:

- `status`
- `name`
- `owner`
- `species`
- `unpair`

Permission replies:

- `once`
- `deny`

Character transfer flow:

- `char_begin`
- `file`
- `chunk`
- `file_end`
- `char_end`

## Heartbeat Behavior

Heartbeat snapshots remain the main source of truth for:

- session counts
- message text
- transcript entries
- prompt presence
- tokens / tokens today
- recently completed state

`evt:"turn"` is treated as optional decoration only, not critical UI state.

## Character Packs

The Pico port supports:

- built-in ASCII pets compiled into firmware
- one installed pushed character pack stored in flash

Current renderer scope:

- installed `text` character manifests are supported on-device
- the wire protocol for pushed packs is implemented
- GIF-pack rendering from the upstream ESP32 project is not yet implemented here

## Dock Clock

The dock clock is a Pico-specific adaptation of the upstream idle clock idea:

- fixed landscape only
- only appears on USB power
- only appears when a bridge time sync has been received
- only appears when the Buddy is idle and no overlay is active
