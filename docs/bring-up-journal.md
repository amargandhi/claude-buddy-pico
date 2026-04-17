# Claude Buddy Pico Bring-Up Journal

This document is the long-form build log for the first real Pico 2 W Hardware Buddy prototype. It is written to be useful in two ways:

1. As a repo-side reference for anyone repeating the build.
2. As raw material for a later blog post about the project.

The work captured here happened on April 17, 2026.

## Project Goal

Build a Claude Hardware Buddy around:

- Raspberry Pi Pico 2 W
- Pimoroni Pico Display Pack 2.8
- Pimoroni LiPo SHIM for Pico
- 2000mAh single-cell LiPo

The end goal was a battery-capable handheld companion that can connect to Claude Desktop through Anthropic's Hardware Buddy flow as a Pico-first adaptation of the upstream project rather than a literal M5Stick clone.

## Hardware Chosen

The prototype hardware stack was:

- `Pico 2 W with headers`
- `Pico Display Pack 2.8 (PIM715)`
- `LiPo SHIM for Pico (PIM557)`
- `2000mAh LiPo (BAT0005)`

The main risk was mechanical fit, not electrical compatibility. The Display Pack works with Pico 2 W boards, and the LiPo SHIM works electrically with the Pico, but the underside connector on the display makes the common sandwich approach awkward when the header pins are short.

## Assembly Approach That Worked

The practical assembly order was:

1. Dry-fit the Pico in the Display Pack with no battery attached.
2. Remove the display.
3. Solder the LiPo SHIM to the back of the Pico using the back-to-back method.
4. Inspect all solder joints carefully.
5. Refit the Pico to the display.
6. Power the board from USB only first.
7. Connect the battery only after USB bring-up is clean.

The important mechanical conclusion was that back-to-back mounting was the right first choice for this specific parts set.

## First Hardware Bring-Up

The initial firmware goal was intentionally small:

- confirm the screen draws correctly
- confirm the A/B/X/Y buttons read correctly
- confirm the RGB LED responds
- confirm USB power is detected

That smoke build succeeded. The display, buttons, and LED were all alive on the assembled hardware.

## Battery and BOOTSEL Lesson

One of the earliest real hardware lessons was that the LiPo changes the boot workflow.

With the battery attached, unplugging and replugging USB was not enough to reliably force the board back into BOOTSEL mode. The reliable recovery path on this exact setup was:

1. Disconnect the battery from the LiPo SHIM.
2. Hold the Pico `BOOTSEL` button.
3. Plug USB back into the Mac.
4. Wait for the `RP2350` mass-storage volume to appear.

The attempted shortcut of holding `BOOTSEL` and using the SHIM power button did not work reliably in practice on this build, even though it looked plausible from the product notes.

That is worth calling out prominently for anyone else using the same SHIM plus battery combination.

## macOS Toolchain Setup

The firmware toolchain on macOS needed one extra note:

- Homebrew's `arm-none-eabi-gcc` formula was not enough in this environment because `nosys.specs` was missing.
- The workaround was to extract a local Arm GNU embedded toolchain into the repo and configure the project to prefer it automatically.

That left the build reproducible inside the workspace without depending on a system-wide manual install.

## Claude Desktop Surprise

The biggest host-side surprise had nothing to do with the Pico hardware at all.

Claude Desktop `1.3109.0` on April 16, 2026 still contained the Hardware Buddy code and strings, but the `Developer -> Open Hardware Buddy...` menu item was hidden by runtime feature gating on this machine.

That meant the project hit an application rollout problem before it hit a firmware problem.

### What we observed

- the public `claude-desktop-buddy` repo still documented the Hardware Buddy flow
- the installed Claude app bundle still contained the Buddy code
- the local UI did not initially expose the Buddy menu

### What finally worked

A local feature override and patched app investigation eventually caused the main Claude install to expose the Hardware Buddy window again. The important lesson for future write-up is:

- if the repo says Hardware Buddy exists but the menu is missing, the app may still contain the feature and simply be hiding it through rollout state

This was a host application availability problem, not a Pico problem.

## BLE Discovery Bug

The first true firmware bug that blocked discovery was in the BLE advertising payload.

The firmware originally packed:

- flags
- complete local name
- the full 128-bit Nordic UART Service UUID

into the primary advertisement packet.

That total exceeded the 31-byte limit for legacy BLE advertising, which meant the Pico could claim it was advertising while Claude Desktop could not discover it reliably.

### Fix

The fix was:

- keep `Claude Pico` in the main advertisement
- move the NUS UUID into the scan response

After that change, Claude Desktop could discover and select the device.

## Device Status Schema Bug

After BLE connection started working, Claude still showed `No response`.

That turned out to be a protocol-shape problem, not a transport problem.

Claude Desktop was validating the firmware's `deviceStatus` response and rejecting it because the firmware was missing fields that the current desktop build expects.

### Required fields we had to satisfy

- `name`
- `sec`
- `bat.pct`
- `bat.mV`
- `bat.mA`
- `bat.usb`
- `sys.up`
- `sys.heap`
- `stats.appr`
- `stats.deny`
- `stats.vel`
- `stats.nap`
- `stats.lvl`

Once those fields were included, Claude stopped showing `No response` and began rendering real device information in the Hardware Buddy window.

## Secure Pairing Problem

The next blocker was more subtle.

Claude Desktop could connect to the Pico, but it still reported:

- `Connection is unencrypted`

The Pico screen also stayed in a passkey/pairing state instead of moving cleanly into a secure, bonded steady state.

### Why this mattered

The BLE transport was functional, but the final v1 goal is a bonded secure link, not just an open NUS pipe.

### Firmware direction chosen

The next firmware pass was designed to improve this in two ways:

1. Define the NUS RX/TX characteristics with encryption requirements in the GATT database instead of using the default unsecured imported definition.
2. Proactively request the target security level on connect instead of waiting for later traffic to trigger security negotiation.

This became the basis of the next firmware target in `src/claude_buddy_pico.cpp`.

## UX Direction for the Device

The project started with a pure smoke-test screen and then grew toward a real on-device UI.

The v1 UI direction became:

- `Home`: connection, message, session counts, power state
- `Activity`: most recent traffic and live state
- `Prompt`: on-device approve/deny workflow
- `Stats`: approvals, denials, reconnects, uptime
- `Settings`: device name, owner, security notes, recovery hints

Button mapping stayed aligned with the earlier design:

- `A`: next page or approve
- `B`: previous page or deny
- `X`: detail toggle
- `Y`: home
- `Hold A`: settings

### What actually shipped

The firmware that landed in `claude_buddy_pico` simplified the screen set away from five top-level screens. The shipped layout is `Home`, `Pet`, and `Info` as the main screens, with `Prompt` handled as a centered overlay on top of whichever main screen is active, and `Settings` and `Reset` reached through a menu overlay opened with `Hold A`. The button semantics above stayed close to the plan, with some Pico-specific additions such as `Hold X` for the `dizzy` easter egg and `Hold Y` for nap. The canonical on-device UI reference is [user-guide.md](user-guide.md), not this section.

## Recovery and Troubleshooting Notes Worth Publishing

These are the highest-value troubleshooting items to keep in the eventual public write-up:

### 1. If BOOTSEL stops working after the LiPo is connected

Disconnect the battery first. Then use `BOOTSEL` over USB.

### 2. If Claude Desktop cannot see the device

Check the BLE advertisement size. A name plus a 128-bit UUID can easily overflow a legacy advertisement packet.

### 3. If Claude says `Connected` but `No response`

Inspect the exact JSON schema of the `status` reply and include every field the current desktop build validates.

### 4. If Hardware Buddy is missing from the Claude menu

Do not assume the feature is gone. The app may still contain the code but hide it through feature gating.

### 5. If the Pico screen shows pairing/passkey forever

Treat that as a security negotiation issue, not a discovery issue.

## Repo Layout After This Phase

By the end of this phase the repo had three useful firmware tiers:

- `claude_buddy_pico_smoke`: basic hardware smoke test
- `claude_buddy_ble_smoke`: BLE discovery and protocol smoke test
- `claude_buddy_pico`: the next, more complete firmware pass with stronger security and a fuller UI

That split is useful for future contributors because it gives them a ladder:

1. Prove the hardware.
2. Prove BLE discovery.
3. Prove protocol and UI behavior.

## Suggested Blog Structure

If this becomes a public blog post later, a strong structure would be:

1. Why build a Pico Hardware Buddy instead of copying the ESP32 sample
2. The exact hardware stack and why it is enough
3. The mechanical trap with the LiPo SHIM and Display Pack
4. The smoke-test phase
5. The Claude Desktop feature-gating detour
6. The BLE discovery bug caused by a too-large advertisement packet
7. The protocol validation bug that caused `No response`
8. The secure pairing work needed for a proper v1
9. The final enclosure and polish work still left

## Current Status

At this point the Pico port had proven:

- assembled hardware works
- USB bring-up works
- display/buttons/LED work
- Claude Desktop can discover and pair with the Pico
- `status` payloads validate in the Hardware Buddy window
- the main firmware now provides pet/info/home screens, prompt approval, demo mode, dock clock, persisted settings/stats, and one installed pushed character pack

The biggest remaining firmware gap for a future pass is full GIF character rendering parity with the upstream ESP32 build.
