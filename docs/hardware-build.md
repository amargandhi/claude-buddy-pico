# Hardware Build

## Parts

Exact prototype parts:

- `Raspberry Pi Pico 2 W` with headers
- `Pimoroni Pico Display Pack 2.8` (`PIM715`)
- `Pimoroni LiPo SHIM for Pico` (`PIM557`)
- `Pimoroni 2000mAh LiPo` (`BAT0005`)
- micro-USB data cable
- soldering iron, solder, and ideally a multimeter

## Assembly Order

1. Dry-fit the `Pico 2 W` into the `Display Pack 2.8` with no battery attached.
2. Remove the display and mount the `LiPo SHIM` to the back of the Pico.
3. Prefer the back-to-back SHIM method first. The display's underside connector makes the sandwich stack tight with short header pins.
4. Inspect the SHIM solder joints closely before applying power.
5. Refit the Pico into the display board.
6. Power the board from USB only for the first bring-up.
7. Connect the battery only after the USB-only power-up succeeds.

## Power Pins To Double-Check

Before first power, inspect around:

- `VBUS`
- `VSYS`
- `3V3_EN`
- `3V3`

On this build the hardware risk was mechanical fit and accidental solder bridges, not logic-level compatibility.

## Battery And Antenna Notes

- Keep the LiPo away from the Pico 2 W antenna end.
- Avoid metal hardware near the antenna keep-out area.
- Keep sharp case edges and screw posts away from the battery pouch.
- Treat the LiPo SHIM button as hardware-only power control, not an app button.

## BOOTSEL Recovery That Actually Worked

With the battery connected, the Pico often stayed powered in a way that made normal `BOOTSEL` recovery unreliable.

The most reliable fallback reflash workflow for this exact prototype was:

1. Disconnect the battery from the LiPo SHIM.
2. Hold the Pico `BOOTSEL` button.
3. Plug USB into the Mac.
4. Wait for the `RP2350` volume to appear.
5. Flash the new UF2.
6. Reconnect the battery after the board is running again if needed.

There was also a battery-connected path that worked on this build:

1. Leave the battery connected.
2. Plug USB into the Mac first.
3. Hold the Pico `BOOTSEL` button.
4. While holding `BOOTSEL`, press and hold the LiPo SHIM side button near the USB end.
5. Wait for the `RP2350` volume to appear.

So the practical guidance is:

- use the battery-disconnect method as the safest fallback
- if you want to recover without unplugging the battery, try `USB first -> hold BOOTSEL -> hold SHIM side button`

## First Power-Up Checklist

- nothing gets hot
- display lights cleanly
- the Pico enumerates over USB
- button presses register
- RGB LED responds

If any of those fail, stop before connecting the battery.
