# Smoke Test Plan

Use MicroPython first for the shortest hardware bring-up loop, then switch to the C++ firmware for the full BLE device.

## Firmware to Flash

- Use the RP2350-specific Pimoroni build for `rpi-pico2_w`.
- Do not use the older RP2040-only Pimoroni MicroPython builds for this board.

## Smoke Test Checklist

1. Draw text and a color fill on the Pico Display Pack 2.8.
2. Confirm all four front buttons read correctly.
3. Run a separate RGB LED test before relying on any LED pin mapping.
4. Verify battery voltage through `GPIO29 / ADC3`.
5. Verify USB presence through CYW43 `WL GPIO 2`.

## Battery Notes

- Sample VSYS periodically, not constantly.
- Treat the older Pico and Pico W `GP24` examples as historical reference only.
- Do the first boot over USB before connecting the battery.
