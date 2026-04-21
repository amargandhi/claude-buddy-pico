# Limitations And Future Work

## Current Limitations

- The pushed character-pack renderer currently supports `text` manifests on-device. Upstream-style GIF pack rendering still needs a Pico-native decode and asset pipeline.
- The battery percentage is estimated from `VSYS` voltage. There is no direct current-sense path on this build, so the number is a voltage-curve approximation rather than true coulomb counting.
- Reflashing is documented through `BOOTSEL` and drag-drop UF2. A `picotool`-driven reboot/load path would make the in-firmware reflash flow smoother but has not been wired up yet.
- The case is not included in this release. V1 is firmware-only; the 3D-printable enclosure and full hardware build guide are tracked as the next phase.

## Good Next Steps

- Add a Pico-friendly GIF asset pipeline for pushed character packs.
- Wire up a `picotool`-based reboot-to-BOOTSEL path so the `BOOTSEL` button becomes a fallback rather than the primary flash workflow.
- Capture polished final screenshots and a short demo GIF from the finished main firmware.
- Design and publish a 3D-printable enclosure alongside a full hardware build guide.
