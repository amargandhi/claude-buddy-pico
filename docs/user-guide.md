# User Guide

This guide explains the on-device UI for the main firmware target: `claude_buddy_pico`.

## Quick Start

After the Pico boots, the UI is split into three areas:

- left column: Buddy name, owner, animated pet, current state, and a short status banner
- right panel: the current screen content
- footer: the active button hints

The main screens cycle like this:

1. `Home`
2. `Pet`
3. `Info`

Tap `A` to move to the next main screen.

## Button Map

- `A`: next main screen, approve prompt, advance selection in overlays
- `B`: next page, next transcript chunk, deny prompt, or confirm selection in overlays
- `X`: next pet normally, raw prompt details during approvals
- `Y`: home / back
- `Hold A`: open or close the main menu
- `Hold X`: trigger the one-shot `dizzy` easter egg
- `Hold Y`: nap / wake
- `LiPo SHIM button`: hardware-only power control, never app input

The footer always shows the active controls for the current context.

## Home Screen

The `Home` screen is the main live status view.

The left column shows:

- the large Buddy device name
- the smaller owner label
- the animated pet
- the current Buddy state and selected pet
- a short banner such as `Buddy connected`, `Claude is working`, or `Approval pending`
- the summary counters `T / R / W` for total, running, and waiting

The right panel shows:

- connection chip: `secure`, `pairing`, or `idle`
- power chip: battery percent plus `usb` or `bat`
- `sessions`
- `running`
- `waiting`
- `today`
- transcript or recent message content

If more transcript lines exist than fit on screen, the panel shows `B more X/Y`. Press `B` to page through transcript chunks. When new lines arrive from Claude, the transcript pager resets to the newest chunk automatically.

If transcript display is turned off in settings, the `Home` screen still shows the current message and latest ack line.

## Pet Screen

The `Pet` screen has two pages. Press `B` to switch between them.

Page 1 shows live pet stats:

- `mood`
- `fed`
- `energy`
- `level`
- `approved`
- `denied`
- `napped`
- `tokens`
- `today`

Page 2 explains what drives those stats:

- fast approvals improve mood
- more tokens increase feed and level progress
- naps refill energy
- reminder text for the main controls

## Info Screen

The `Info` screen has seven pages. Press `B` to move through them.

The pages are:

1. `About`
2. `Buttons`
3. `States`
4. `Claude`
5. `Device`
6. `Bluetooth`
7. `Build`

These pages act as the built-in on-device reference manual.

## Prompt Overlay

When Claude asks for a permission decision, a centered `APPROVAL` overlay appears over the normal UI.

The overlay shows:

- how long the prompt has been waiting
- the tool name when available
- either a short human-readable prompt hint or the raw JSON payload

Prompt controls:

- `A`: approve once
- `B`: deny
- `X`: toggle raw details
- `Y`: return the underlying display mode to `Home`

If a decision has already been sent, the prompt overlay shows `sent...` until the next heartbeat clears or replaces that prompt.

## Main Menu

Open the main menu with `Hold A`.

Menu items:

- `settings`: opens the settings overlay
- `screen off`: turns the screen off until the next button wake
- `help`: jumps to the `Info -> Buttons` page
- `about`: jumps to the `Info -> About` page
- `demo: on/off`: toggles demo mode
- `close`: closes the menu

Overlay navigation is consistent:

- `A`: move selection
- `B`: choose
- `Y`: back

## Settings

Open `Settings` from the main menu.

Settings items:

- `bright: 0/4` to `4/4`
- `led: on/off`
- `transcript: on/off`
- `demo: on/off`
- `dock clock: on/off`
- `pet: <species>`
- `reset`
- `back`

What each setting does:

- `bright`: cycles through five backlight steps. `4/4` is the brightest normal setting.
- `led`: enables or disables the RGB status LED.
- `transcript`: shows or hides wrapped transcript lines on the `Home` screen.
- `demo`: runs a local fake heartbeat sequence for showing the device without Claude Desktop driving it.
- `dock clock`: allows the dock clock view to appear when the Buddy is idle on USB power and has already received a time sync.
- `pet`: cycles through the built-in pets, or the currently installed custom pack if one exists.
- `reset`: opens the reset overlay.
- `back`: closes settings.

All of these settings are persisted in flash.

## Reset Overlay

The reset overlay contains:

- `delete char`
- `factory reset`
- `back`

Reset actions use a two-step confirm flow:

1. first `B` press arms the selected action for a short window
2. second `B` press while still armed performs it

What each reset action does:

- `delete char`: removes the installed custom character pack
- `factory reset`: clears device settings and stats, resets the active pet selection, clears stored identity fields, and drops the current bond when connected
- `back`: exits reset

## Dock Clock

The dock clock is a Pico-specific idle screen.

It only appears when all of these are true:

- the display mode is `Home`
- `dock clock` is enabled in settings
- USB power is present
- a time sync has already been received
- no prompt or overlay is active
- demo mode is off
- the Buddy is linked and idle

When the dock clock is showing:

- the right panel displays `HH:MM`
- the date appears underneath
- the owner name or `USB dock mode` is shown below the clock
- `B` is not used for transcript paging on this screen

## Nap, Screen Off, And Wake

`Hold Y` toggles nap mode.

While napping:

- the pet state becomes `sleep`
- the banner changes to `Buddy is napping`
- the backlight drops to a low glow
- any normal button press wakes the Buddy

`screen off` from the main menu is different from nap:

- it turns the backlight fully off
- the next button press wakes the screen
- it does not count as nap time

The firmware also dims automatically after inactivity and can turn the screen fully off after a longer idle period while on battery.

## Names And Identity

There are two user-visible identity fields:

- large device name: the Buddy `name`
- smaller owner line: the Buddy `owner`

How they are set:

- the Claude Desktop Hardware Buddy `Name` field changes the large on-device name
- the Buddy `owner` value sent by Claude Desktop or the bridge changes the smaller owner line

So another builder will see their own chosen values, not the names used during bring-up on this machine.

## LED Meanings

The display RGB LED is intentionally driven below full power so it is visible without being harsh in a dark room.

Current LED map:

- `off`: asleep or not linked
- `green`: linked and idle
- `blue`: Claude is running work
- `amber`: approval pending
- `magenta`: pairing or character install
- `red`: low battery

If `led` is turned off in settings, the LED stays off regardless of Buddy state.

## Character Packs

This Pico port supports:

- built-in ASCII pets compiled into firmware
- one installed pushed character pack stored in flash

When a character pack is being installed:

- the right panel changes to an install progress view
- the left banner shows `Installing character pack`
- the LED uses the pairing/install color

The current renderer supports pushed `text` manifests on-device. The wire protocol for `char_begin`, `file`, `chunk`, `file_end`, and `char_end` is implemented, but GIF-pack rendering from the original ESP32 sample is still future work.

For a ready-to-send sample folder, use [../examples/character-packs/sunset_blob](../examples/character-packs/sunset_blob).

## Status Messages You Will Commonly See

Examples of normal status text:

- `Waiting for Claude Desktop`
- `Connected, pairing in progress`
- `Buddy connected`
- `Claude is working`
- `Claude needs attention`
- `Approval pending`
- `Installing character pack`
- `Demo mode`
- `Buddy is napping`

These messages come from the firmware state machine and are meant to be the fastest way to understand what the device is doing.

## Everyday Usage Flow

For a normal day-to-day flow:

1. Power the Pico and let it reconnect to Claude Desktop.
2. Stay on `Home` for live activity and transcript.
3. Use `A` to move to `Pet` or `Info`.
4. Use `B` to page transcript or move through screen sub-pages.
5. Use `X` to swap pets.
6. Use `Hold A` for settings or demo mode.
7. Use `Hold Y` when you want the Buddy to nap.
8. Respond to approvals with `A` or `B` when prompts appear.

## Related Docs

- [README.md](../README.md)
- [software-setup.md](software-setup.md)
- [folder-push.md](folder-push.md)
- [protocol-map.md](protocol-map.md)
- [feature-matrix.md](feature-matrix.md)
