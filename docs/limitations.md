# Limitations And Future Work

## Current Limitations

- The pushed character-pack renderer currently supports `text` manifests on-device. Upstream-style GIF pack rendering still needs a Pico-native decode and asset pipeline.
- This workspace is not yet linked to the intended public `-pico` GitHub fork from this machine because the local `gh` token is invalid.
- USB reflash from a running firmware image is still less reliable than the battery-disconnect + `BOOTSEL` path on this hardware stack.
- The Battery percentage is estimated from `VSYS` voltage. There is no direct current-sense path on this build.

## Good Next Steps

- Add a Pico-friendly GIF asset pipeline for pushed character packs.
- Make the `picotool` USB reboot/load path reliable enough to replace most manual `BOOTSEL` flashing.
- Capture polished final screenshots and a short demo GIF from the finished main firmware.
- Reconcile this workspace into the existing public `-pico` fork so the published repo preserves fork lineage cleanly.
