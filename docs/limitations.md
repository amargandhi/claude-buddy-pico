# Limitations And Future Work

## Current Limitations

- The pushed character-pack renderer currently supports `text` manifests on-device. Upstream-style GIF pack rendering still needs a Pico-native decode and asset pipeline.
- The battery percentage is estimated from `VSYS` voltage. There is no direct current-sense path on this build, so the number is a voltage-curve approximation rather than true coulomb counting.
- Reflashing is documented through `BOOTSEL` and drag-drop UF2. A `picotool`-driven reboot/load path would make the in-firmware reflash flow smoother but has not been wired up yet.
- The case and build guide are included, but the enclosure is still v1. The obvious next fixes are a better integrated battery retainer, a deeper USB cutout, and button stems that need less break-in.

## Good Next Steps

- Add a Pico-friendly GIF asset pipeline for pushed character packs.
- Wire up a `picotool`-based reboot-to-BOOTSEL path so the `BOOTSEL` button becomes a fallback rather than the primary flash workflow.
- Tighten the v2 case revision: battery retainer, USB cutout, and button-stem tolerance.
- Add a simpler first-time-builder path that focuses on the minimum USB-powered build before the portable stack.
