# Claude Pico Desktop Buddy — Build Guide

A small BLE buddy for [Claude Desktop](https://claude.ai/download). Raspberry Pi Pico 2 W, Pimoroni Display Pack 2.8, optional battery, 3D-printed case in the shape of the Claude Code mascot. Pairs over Bluetooth Low Energy using Anthropic's Hardware Buddy protocol.

![Assembled Claude Pico Buddy](Photos_web/03_assembly/03_19_assembled_powered_on_pet_screen.jpg)

The CAD is in `.step` so it can be edited and remixed.

- Repo: [../README.md](../README.md)
- Firmware notes: [../docs/hardware-build.md](../docs/hardware-build.md), [../docs/software-setup.md](../docs/software-setup.md)
- Printable files (`.stl` and `.step`): [CAD/](CAD/)
- Photos used in this guide: [Photos_web/](Photos_web/)

---

## Why this exists

Felix Rieseberg's Hardware Buddy looked like a good side project when I'd run out of Claude usage and was waiting for the next 5-hour block to start again. Cambridge helps here: Raspberry Pi has a shop I can wander into at lunch, so getting the parts was easy. The catch was that Felix's original build was for different hardware, which meant I then had to port the whole thing to the Pico 2 W.

This ended up being a relatively low-cost build. The Display Pack 2.8 felt like the best screen for the size and money. E-ink was tempting. A bigger panel was tempting too. This one felt like the right compromise for something that can sit on top of a Mac mini, or be picked up and taken to the kitchen while the agents are coding and you're doing something else around the house.

The case is designed around the portable stack, but the same firmware also runs as a simpler USB-powered build with just the Pico 2 W and Display Pack.

---

## Two builds, same firmware

The main thing you need is a **Pico 2 W and a Display Pack**. That's the minimum build. Plug it into USB, flash the firmware, pair with Claude Desktop, and leave it on your desk. If you only want a tethered buddy, stop there.

The portable build adds the **LiPo SHIM and a battery** so you can carry it around. The case is designed around the portable build — the rear panel has a battery cavity and the screw posts assume the SHIM stack height — but a tethered minimum build will run the same firmware identically.

Everything below describes the portable build. If you're doing the minimum build, skip the SHIM solder step and the battery steps; the rest is the same.

---

## 1. What you need

### Electronics

| Qty | Part | Source | Notes |
|----:|------|--------|-------|
| 1 | Raspberry Pi Pico 2 W with headers | [Pimoroni / Pi Hut / official](https://shop.pimoroni.com/) | Soldered headers, not bare pads. |
| 1 | Pimoroni Pico Display Pack 2.8 | `PIM715` | 320×240 IPS, four buttons, RGB LED. |
| 1 | Pimoroni LiPo SHIM for Pico | `PIM557` | Portable build only. |
| 1 | LiPo battery, 2000 mAh, 3.7 V, JST-PH | PKCELL LP803860 or similar | Portable build only. Any 3.7 V LiPo with a JST-PH plug in roughly that footprint will fit the cavity. |
| 1 | USB-micro cable, **or** USB-C cable + USB-micro-to-USB-C adapter | generic | The case really wants a thin micro plug or a compact adapter — see note below. |

**About the USB connector.** The Pico is USB-micro. The case works with a thin USB-micro cable. If your cable has a chunky overmoulded plug, use a small USB-micro-to-USB-C adapter instead. The left-side cutout is larger specifically so that adapter can fit. I don't know the exact adapter brand I used, but Amazon is full of ones in roughly the same size and most of them should fit.

### 3D-printed parts (see [CAD/](CAD/))

Six parts. Both `.stl` (slice-ready) and `.step` (parametric — open in Fusion / FreeCAD / Onshape and change anything you like).

| File stem | Qty | What it is |
|-----------|----:|------------|
| `Claude_Pico_Case_Main_Body` | 1 | Front shell. Screen window, side button slots, power-button pocket. |
| `Claude_Pico_Case_Rear_With_Tail` | 1 | Rear panel with the Claude Code mascot tail. |
| `Claude_Pico_Battery_Clip` | 1 | Holds the battery against the inside of the rear panel. Glued. |
| `Claude_Pico_Power_Button` | 1 | Actuator for the LiPo SHIM's power button. |
| `Claude_Pico_Left_Side_Button` | 1 | Actuator for the display's `A` / `B` column. |
| `Claude_Pico_Right_Side_Button` | 1 | Actuator for the display's `X` / `Y` column. |

**Why three separate side buttons.** The display PCB's flex connector eats about 5 mm on one side, so the screen sits off-centre relative to the display PCB. An uncentered display would have driven me mad, so I centered the display in the case and let the button geometry absorb the offset instead. The buttons have enough tolerance, but they usually smooth out after a few repeated clicks.

### Fasteners and consumables

| Qty | Part | Notes |
|----:|------|-------|
| 2 | M3 × 12 mm self-tapping screw | Rear panel into main body. |
| — | Super glue | Bonds the battery clip to the rear panel. |

### Tools

Soldering iron, thin solder, flux. Small Phillips screwdriver. Multimeter is useful for the pre-power continuity check. Tweezers. A Mac with Homebrew for the toolchain (Linux works too — the scripts are portable).

### Software

[Claude Desktop](https://claude.ai/download) with Developer Mode enabled.

![All parts laid out](Photos_web/03_assembly/03_07_all_parts_hero.jpg)

---

## 2. Print the case

Six parts, all small. I printed mine in **ABS on a Bambu P1S with default settings** because that was the only orange filament I had around. PLA should also be fine; the case has no real thermal duty.

Orientation:

- **Main body** — screen opening facing up. Supports under the side-button overhangs only.
- **Rear panel with tail** — flat back on the bed, tail facing up. No supports.
- **Battery clip** — flat side down. No supports.
- **Three buttons** — cap up, stem pointing into the build plate. No supports. Batch all three on the same plate.

The `.step` files are there so you can edit the actual CAD and improve it. This was a side project over a weekend, not a finished enclosure program. The obvious places to start are the button stems, the USB cutout, and a better battery retainer.

![3D-printed parts](Photos_web/03_assembly/03_06_printed_parts_layout.jpg)

---

## 3. Build the Pico stack

Three boards: Pico 2 W → LiPo SHIM → Display Pack.

### 3.1 Dry-fit before soldering

With nothing soldered and no battery attached, push the Pico onto the Display Pack's female headers. It should seat with no force on the glass. If it doesn't sit flat, find what's obstructing it before going further.

### 3.2 Solder the LiPo SHIM

Pull the Pico off the display. Place the LiPo SHIM on the back of the Pico (opposite the RF can). The SHIM's silkscreen aligns with a subset of the Pico header pins. The JST connector ends up pointing away from the USB port.

Solder one pin at a time from the Pico side. Inspect under magnification for bridges when done — `VBUS`, `VSYS`, `3V3_EN`, `3V3` are the ones that turn a working build into a dead one. A multimeter in continuity mode takes a minute and saves hours.

![Pico + SHIM, back](Photos_web/01_hardware/01_03_pico_shim_stack_back.jpg)

![Pico + SHIM, underside with battery JST](Photos_web/01_hardware/01_05_pico_shim_stack_underside.jpg)

### 3.3 Refit the Pico into the display

Press the Pico-plus-SHIM stack back into the Display Pack. The display silkscreen reads upside-down when the USB port points up — that's the right orientation for this case.

---

## 4. Flash the firmware

Build it from the repo:

```bash
scripts/bootstrap_macos.sh      # one-time
scripts/clone_deps.sh           # one-time
scripts/configure_firmware.sh
cmake --build build/pico
```

This produces three UF2s. The full one is `build/pico/claude_buddy_pico.uf2` — that's the one to flash. The other two (`claude_buddy_pico_smoke.uf2` and `claude_buddy_ble_smoke.uf2`) are there for troubleshooting. Skip them unless something breaks.

Flash:

1. Disconnect the battery.
2. Hold the SHIM's power button (or `BOOTSEL` on a minimum build).
3. Plug USB into your Mac.
4. Wait for the `RP2350` volume to mount.
5. `cp build/pico/claude_buddy_pico.uf2 /Volumes/RP2350/`

Toolchain detail in [../docs/software-setup.md](../docs/software-setup.md).

### Pair with Claude Desktop

1. Open Claude Desktop, enable Developer Mode.
2. `Developer → Open Hardware Buddy…`
3. `Connect`, pick `Claude Pico`, complete the passkey flow.

When pairing succeeds the Pico shows the linked state and reacts to Claude Desktop activity.

![Claude connected, ASCII buddy on screen](Photos_web/02_smoke_test/02_06_ble_claude_connected_ascii.jpg)

Before you assemble the case, leave the firmware running off battery for 15 minutes and confirm nothing gets warm. Thermal problems are much harder to diagnose once the case is closed.

---

## 5. Assemble the case

Boot the firmware into the Pico/display board first. Then do the assembly in this order.

### 1. Place the four side buttons into the main body

From the inside, push each printed button through its slot until the cap protrudes about 1 mm. They're a friction fit. Two on the left for `A` / `B`, two on the right for `X` / `Y`.

![Empty main body, button slots visible](Photos_web/03_assembly/03_10_main_body_empty_button_slots.jpg)

### 2. Slide the power button into its slot, all the way

The power button is the printed actuator for the LiPo SHIM's side button. Slide it in fully so it bottoms out.

![Case open, power button in slot](Photos_web/03_assembly/03_13_case_open_power_button.jpg)

### 3. Angle the assembled Pico/display board into the case

Tilt the stack in display-first, then press down. It should snap fit. The display glass should land flat against the screen window.

![Display seated in main body](Photos_web/03_assembly/03_11_display_seated_in_main_body.jpg)

### 4. Push the USB connector through the left-hand cutout

Push the USB-C adapter (or the micro plug) through the cutout on the left. I usually seat the Pico/display stack nearly all the way down first, then adjust from there. If the connector doesn't line up cleanly, push the Pico in a little more or pull it back up a touch. The fit is loose enough that this is easy.

![Pico seated, USB port visible](Photos_web/03_assembly/03_12_pico_seated_usb_port.jpg)

![USB-C adapter fit check](Photos_web/03_assembly/03_14_usb_c_adapter_fit_check.jpg)

### 5. Slide the battery into the rear case

The battery clip is glued to the inside of the rear panel (see note below). Slide the battery into the clip. Route the JST lead toward the SHIM end so the wire doesn't sit across the RF can.

![Battery clipped to rear panel](Photos_web/03_assembly/03_15_battery_clipped_to_rear_panel.jpg)

**About the battery clip.** I was getting tired, so I just glued the clips to the back of the rear case with super glue. It works. An integrated retainer would be better.

### 6. Two M3 × 12 screws

Bring the rear panel up to the main body, tail pointing down. The screws go through the rear flanges of the main body into the screw posts on the rear panel. Hand-tight only — overtightening will split the printed posts. If a screw feels gritty going in, back out and re-start; you're cross-threading.

![Rear panel + screws ready](Photos_web/03_assembly/03_16_rear_panel_with_m3_screws.jpg)

Done.

![Assembled — rear with tail](Photos_web/03_assembly/03_17_assembled_rear_tail.jpg)

![Assembled — side profile](Photos_web/03_assembly/03_18_assembled_side_profile.jpg)

---

## 6. First power-on from the case

Plug in USB, or press the SHIM power button if the battery is charged.

![Powered on — Claude home](Photos_web/03_assembly/03_20_assembled_claude_linked.jpg)

`A` / `B` step through screens, `X` / `Y` jump to Pet and Home. `Hold A` opens settings, `Hold Y` puts the buddy to sleep. Full reference: [../README.md#button-map](../README.md).

---

## 7. Troubleshooting

If the build came up clean on first power-on, you're done. The rest of this section is for when it doesn't.

### The smoke-test ladder

The firmware ships as three UF2s in increasing complexity. If the full firmware misbehaves, walk down the ladder until you find the first one that works — that tells you which layer is broken.

- `claude_buddy_pico_smoke.uf2` — just the hardware. Display, buttons, VSYS, USB sense, uptime.
- `claude_buddy_ble_smoke.uf2` — adds the BLE radio and protocol.
- `claude_buddy_pico.uf2` — the full firmware.

Flash the same way as the main firmware. The hardware smoke test should show `CLAUDE BUDDY PICO SMOKE TEST`, a non-zero `VSYS` reading, `USB SENSE: ON`, an uptime counter, and per-button `HIT` indicators when you press `A` / `B` / `X` / `Y`.

![Hardware smoke test running](Photos_web/02_smoke_test/02_01_display_smoke_test_running.jpg)

The BLE smoke test should show `ADVERTISING` and a name field of `Claude Pico`.

![BLE smoke test, advertising](Photos_web/02_smoke_test/02_04_ble_smoke_test_advertising.jpg)

### Specific failures

- **`BOOTSEL` recovery is flaky with the battery attached.** Known LiPo SHIM behaviour, not a firmware bug. Either disconnect the battery before reflashing, or use the battery-connected sequence: plug USB, hold `BOOTSEL`, also hold the SHIM side button near the USB end, wait for the `RP2350` volume.
- **Display shows garbage after flashing.** Re-flash the hardware smoke test. If that's also broken, the headers between Pico and display aren't making clean contact. Re-seat the stack.
- **Pairs but never gets past `ADVERTISING`.** Developer Mode off in Claude Desktop, or you're on a Claude Desktop build that doesn't expose Hardware Buddy yet. Not a firmware problem.
- **Battery feels warm.** Disconnect immediately. Inspect SHIM solder for bridges around `VBUS` / `VSYS` / `3V3`. Don't reconnect until you've found the bridge.
- **Buttons sticky after assembly.** Case walls shaving the printed stems. Open up, sand stems with 400-grit, reinstall.

---

## 8. What to do next

- **Build the minimum version first** if you're unsure — Pico 2 W + Display Pack + USB cable. That tells you whether the firmware works on your hardware before you commit to the SHIM solder.
- **Open the `.step` files** in Fusion / FreeCAD / Onshape if you want to improve the case. The USB cutout, battery retainer, and button stems are the obvious next passes.
- **Post your build.** Photos and fit notes from different prints, batteries, and cable setups help the next builder.

---

## Credits and licence

- Upstream BLE Hardware Buddy protocol: [anthropics/claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy). The original project is Felix Rieseberg's.
- Pico firmware port, case design, build notes: this repo.
- Mascot: Claude Code mascot (Anthropic).
- Licence: see [../LICENSE](../LICENSE).
