# Physical Build — Claude Pico Desktop Buddy

Source assets for the build guide (GitHub + MakerWorld).

- [`BUILD_GUIDE.md`](BUILD_GUIDE.md) — the full step-by-step build.
- [`CAD/`](CAD/) — the 6 printable parts, in both `.step` (parametric) and `.stl` (slice-ready).
- [`Photos/`](Photos/) — **originals** (PNG, 4032×3024 and 8064×6048, ~560 MB total). Keep these for editing / archival.
- [`Photos_web/`](Photos_web/) — **web-optimised JPEGs** (2048px wide, ~23 MB total). These are the ones `BUILD_GUIDE.md` references and what you'd publish to GitHub / MakerWorld.

### What to commit

`Photos/` is ~560 MB. GitHub's per-file web-upload limit is 25 MB, and the `01_01` and `03_07` originals are both over it. Recommended:

```
# .gitignore
Physical Build/Photos/
```

Commit `CAD/`, `Photos_web/`, `BUILD_GUIDE.md`, and `INDEX.md`. Keep `Photos/` locally, or push it to a release asset / Git LFS if you want the originals in the repo.

---

## Bill of materials

| Qty | Part | Notes |
|----:|------|-------|
| 1 | Raspberry Pi Pico 2 W | Main MCU, BLE radio |
| 1 | Pimoroni Display Pack 2.8" | 320×240 IPS LCD + 4 buttons + RGB LED |
| 1 | Pimoroni Display Pack Shim | Break-out so Pico headers stay accessible |
| 1 | LiPo battery | 2000 mAh, 3.7 V (PKCELL LP803860 used in reference build) |
| 1 | USB-micro → USB-C cable | Generic, for Pico USB power/flashing |
| 2 | M3 × 12 screw | Hold rear panel to main body |
| 1 | 3D-printed set (below) | Print all 6 parts |

### 3D-printed parts (see `CAD/`)

| File stem | Qty | Description |
|-----------|----:|-------------|
| `Claude_Pico_Case_Main_Body` | 1 | Front shell — display window, button slots |
| `Claude_Pico_Case_Rear_With_Tail` | 1 | Rear panel, integrates Clawd tail |
| `Claude_Pico_Battery_Clip` | 1 | Retains battery on the rear panel (glue with Gorilla Super Glue Gel) |
| `Claude_Pico_Power_Button` | 1 | Actuator for Pico on-board button |
| `Claude_Pico_Left_Side_Button` | 1 | Actuator for display `A` / `B` button column |
| `Claude_Pico_Right_Side_Button` | 1 | Actuator for display `X` / `Y` button column |

---

## Photo map

Filenames are `<stage>_<step>_<short_description>.png`. They sort naturally, so a directory listing already reads as the build sequence.

### `01_hardware/` — what you need before soldering or pressing Go

| # | Shot |
|---|------|
| 01 | Origin — Raspberry Pi Store (Cambridge) |
| 02 | Bare Pico 2 W, top |
| 03 | Pico + display shim stack, back |
| 04 | Pico + display shim stack, side (header pins) |
| 05 | Pico + display shim stack, underside with battery JST |
| 06 | Parts laid out — display, battery, Pico — view A |
| 07 | Parts laid out — view B |
| 08 | Parts laid out — view C |
| 09 | Parts laid out — Pico underside / shim seated |

### `02_smoke_test/` — hardware bringup before case assembly

| # | Shot |
|---|------|
| 01 | `CLAUDE BUDDY PICO SMOKE TEST` running on display |
| 02 | Smoke test — USB + battery, full system shot |
| 03 | Smoke test — display close-up, battery only |
| 04 | BLE smoke test — `ADVERTISING`, waiting for Claude |
| 05 | BLE smoke test — advertising, alt angle |
| 06 | Claude connected — ASCII buddy on screen, `BUSY SNAIL` state |

### `03_assembly/` — putting the buddy into its 3D-printed case

| # | Shot |
|---|------|
| 01 | Display + battery laid out on mat, assembly prep |
| 02 | Display face-down with Pico + battery placed |
| 03 | Pico + display stack, side profile |
| 04 | Pico stack, edge-on view (header seating check) |
| 05 | Battery clip + Gorilla Super Glue Gel |
| 06 | All 3D-printed parts laid out (main body, rear with tail, battery clip, 3 buttons) |
| 07 | Hero shot — every part + 2× M3×12 screws + battery + Pico stack |
| 08 | Main body, empty, front view (screen cavity) |
| 09 | Main body, empty, angle view |
| 10 | Main body, empty, showing side button slots |
| 11 | Display seated in main body, battery lead exiting |
| 12 | Pico seated, USB-micro port visible through case |
| 13 | Case open, side profile, power button in slot |
| 14 | USB-micro → USB-C adapter test fit |
| 15 | Battery clipped to rear panel (inside of rear-with-tail) |
| 16 | Rear panel + 2× M3×12 screws, ready to close |
| 17 | Fully assembled — rear/tail view |
| 18 | Fully assembled — side profile |
| 19 | Powered on — `ADVENTURE MODE`, pet screen |
| 20 | Powered on — `CLAUDE LINKED` home screen |
| 21 | Powered on — info / stats screen |

---

## Naming conventions

- **Photos:** `<stage>_<step>_<short_description>.png`
  - Two-digit stage + step so a `ls` or GitHub directory view sorts correctly.
  - Stages are `01_hardware` → `02_smoke_test` → `03_assembly`; step numbers restart within each stage.
- **CAD:** `Claude_Pico_<Part_Name>.{step,stl}` — underscores only, no spaces, URL-safe for GitHub / MakerWorld.

## Posting to MakerWorld / GitHub

- The `CAD/` folder is ready to upload directly to MakerWorld — each `.stl` is the printable file, each `.step` is the parametric source for anyone who wants to modify.
- The `Photos/` folder is ready to reference in a `BUILD_GUIDE.md` (or the repo README) via relative links, e.g. `![](Physical%20Build/Photos/03_assembly/03_07_all_parts_hero.png)`.
- A good "cover" shot for both GitHub and MakerWorld is `Photos/03_assembly/03_07_all_parts_hero.png`.

## Original EXIF timestamps

All photos keep their original EXIF `DateTimeOriginal`; nothing was recompressed. Chronology:

- `01_hardware/` and `02_smoke_test/`: captured 2026-04-17
- `03_assembly/`: captured 2026-04-20

## Source filename map (if you ever need to trace back)

| New name | Original |
|---|---|
| 01_hardware/01_01_origin_raspberry_pi_store.png | IMG_2817 2.png |
| 01_hardware/01_02_pico_2w_top.png | IMG_2823.png |
| 01_hardware/01_03_pico_shim_stack_back.png | IMG_2821.png |
| 01_hardware/01_04_pico_shim_stack_side.png | IMG_2822.png |
| 01_hardware/01_05_pico_shim_stack_underside.png | IMG_2819.png |
| 01_hardware/01_06_parts_layout_a.png | IMG_2824.png |
| 01_hardware/01_07_parts_layout_b.png | IMG_2825.png |
| 01_hardware/01_08_parts_layout_c.png | IMG_2826.png |
| 01_hardware/01_09_parts_layout_pico_underside.png | IMG_2827.png |
| 02_smoke_test/02_01_display_smoke_test_running.png | IMG_2828 2.png |
| 02_smoke_test/02_02_display_smoke_test_battery_usb.png | IMG_2829.png |
| 02_smoke_test/02_03_display_smoke_test_closeup.png | IMG_2830.png |
| 02_smoke_test/02_04_ble_smoke_test_advertising.png | IMG_2831.png |
| 02_smoke_test/02_05_ble_smoke_test_advertising_alt.png | IMG_2832 2.png |
| 02_smoke_test/02_06_ble_claude_connected_ascii.png | IMG_2833.png |
| 03_assembly/03_01_display_battery_prep.png | IMG_2861.png |
| 03_assembly/03_02_display_pico_battery_facedown.png | IMG_2862.png |
| 03_assembly/03_03_pico_display_stack_side.png | IMG_2863.png |
| 03_assembly/03_04_pico_stack_edge_view.png | IMG_2864.png |
| 03_assembly/03_05_battery_clip_and_glue.png | IMG_2865.png |
| 03_assembly/03_06_printed_parts_layout.png | IMG_2866.png |
| 03_assembly/03_07_all_parts_hero.png | IMG_2867.png |
| 03_assembly/03_08_main_body_empty_front.png | IMG_2868.png |
| 03_assembly/03_09_main_body_empty_angle.png | IMG_2869.png |
| 03_assembly/03_10_main_body_empty_button_slots.png | IMG_2870.png |
| 03_assembly/03_11_display_seated_in_main_body.png | IMG_2871.png |
| 03_assembly/03_12_pico_seated_usb_port.png | IMG_2872.png |
| 03_assembly/03_13_case_open_power_button.png | IMG_2873.png |
| 03_assembly/03_14_usb_c_adapter_fit_check.png | IMG_2874.png |
| 03_assembly/03_15_battery_clipped_to_rear_panel.png | IMG_2875.png |
| 03_assembly/03_16_rear_panel_with_m3_screws.png | IMG_2876.png |
| 03_assembly/03_17_assembled_rear_tail.png | IMG_2877.png |
| 03_assembly/03_18_assembled_side_profile.png | IMG_2878.png |
| 03_assembly/03_19_assembled_powered_on_pet_screen.png | IMG_2879.png |
| 03_assembly/03_20_assembled_claude_linked.png | IMG_2880.png |
| 03_assembly/03_21_assembled_claude_info_screen.png | IMG_2881.png |
