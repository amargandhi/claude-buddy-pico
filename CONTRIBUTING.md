# Contributing

Thanks for your interest. A few notes so nobody is surprised.

## What this project is

An unofficial community fork of Anthropic's Hardware Buddy reference, retargeted to the Raspberry Pi Pico 2 W plus Pimoroni Pico Display Pack 2.8 and LiPo SHIM. The compatibility target is the upstream Hardware Buddy BLE protocol. It is not intended to be a line-for-line clone of the upstream ESP32 firmware.

## Response times

Issues and PRs are reviewed in batches as time allows. Please do not expect same-day responses. If you open a PR and do not hear back for a few weeks, a polite ping is welcome.

## What kinds of changes are welcome

- Bug fixes against real hardware.
- Documentation fixes, especially build-guide errata from people who actually did the build.
- Additional built-in buddy species with clean, self-contained animation tables.
- Pico-native improvements that do not break upstream protocol compatibility.
- Small quality-of-life additions that are self-contained.

## What is likely to be declined

- Large rewrites that do not come with a written motivation.
- Changes that break upstream protocol compatibility without a strong reason.
- New features that require hardware this repo does not target.
- Tooling churn (reformatters, linters, CI frameworks) that does not serve a concrete need.

## Before opening a PR

1. Build the affected firmware target locally and flash it to real hardware when feasible.
2. Include a one-line summary of what you changed and why.
3. If the change touches the on-device UI, include a photo.
4. If the change touches the BLE protocol or the `deviceStatus` JSON, include a Claude Desktop Hardware Buddy screenshot showing the new behavior.

## Code of conduct

Be kind. Assume good faith. Disagree with ideas, not people.
