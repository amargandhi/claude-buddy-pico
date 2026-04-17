# Software Setup

## Build Tooling On macOS

Install the local toolchain and helper apps:

```bash
scripts/bootstrap_macos.sh
```

That setup expects:

- `cmake`
- `ninja`
- `picotool`
- an Arm embedded GCC toolchain

If Homebrew's default embedded compiler is missing `nosys.specs` on your machine, use the workspace-local fallback:

```bash
scripts/extract_local_toolchain.sh
```

## Clone Vendor Dependencies

```bash
scripts/clone_deps.sh
```

Expected directories:

- `third_party/pico-sdk`
- `third_party/pico-examples`
- `third_party/pimoroni-pico`
- `third_party/pimoroni-pico-rp2350`

## Configure And Build

```bash
scripts/configure_firmware.sh
cmake --build build/pico
```

Main outputs:

- `build/pico/claude_buddy_pico.uf2`
- `build/pico/claude_buddy_pico_smoke.uf2`
- `build/pico/claude_buddy_ble_smoke.uf2`

## Flashing

The safest fallback workflow on this prototype was:

1. Disconnect the battery.
2. Hold `BOOTSEL`.
3. Plug USB into the Mac.
4. Wait for `RP2350` to mount.
5. Copy the UF2.

There was also a battery-connected method that worked on this hardware:

1. Leave the battery connected.
2. Plug USB into the Mac first.
3. Hold `BOOTSEL`.
4. While holding `BOOTSEL`, press and hold the LiPo SHIM side button near the USB end.
5. Wait for `RP2350` to mount.

Example:

```bash
cp build/pico/claude_buddy_pico.uf2 /Volumes/RP2350/
```

## Firmware Targets

### `claude_buddy_pico_smoke`

Use this first when you only want to verify:

- screen draw
- A/B/X/Y buttons
- RGB LED
- USB / VSYS readings

### `claude_buddy_ble_smoke`

Use this when debugging:

- BLE discovery
- passkey pairing
- Claude Desktop connection
- Buddy `status` payload shape

### `claude_buddy_pico`

Use this for the real Pico Buddy firmware:

- Buddy UI
- secure pairing flow
- approvals
- demo mode
- dock clock
- flash-backed settings/stats
- single installed pushed character pack

## Claude Desktop Pairing

1. Turn on Developer Mode in Claude Desktop.
2. Open `Developer -> Open Hardware Buddy...`
3. Click `Connect`
4. Select `Claude Pico`
5. Complete the passkey flow if prompted

The Claude Desktop `Name` field changes the large device name shown on the Pico. The smaller owner label comes from the Buddy `owner` value that Claude Desktop or the bridge sends.

On-device settings are available with `Hold A`.

For the full on-device UI walkthrough, see [user-guide.md](user-guide.md).

If the Hardware Buddy menu is hidden on a Claude Desktop build, verify the app feature exposure first before assuming the Pico firmware is at fault.

## Hardware-Specific Notes

- Final firmware targets `pico2_w`.
- Battery sampling uses `GPIO29 / ADC3` for `VSYS`.
- USB presence uses CYW43 `WL GPIO 2`.
- The display RGB LED pin mapping in docs vs code has been inconsistent historically, so test it on-device rather than trusting old snippets.
