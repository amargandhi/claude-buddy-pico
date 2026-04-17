# Folder Push

The Pico port supports one installed pushed character pack at a time.

Current renderer scope:

- supported on-device format: `text` manifest packs
- supported wire protocol: `char_begin`, `file`, `chunk`, `file_end`, `char_end`
- not supported yet: upstream GIF-pack rendering

## First Test

Use the sample pack in [../examples/character-packs/sunset_blob](../examples/character-packs/sunset_blob).

In Claude Desktop:

1. Open `Developer -> Open Hardware Buddy...`
2. Connect to `Claude Pico`
3. Drag the `sunset_blob` folder into the folder drop area
4. Click `Send to Device`

Expected behavior:

- the Pico shows an install progress panel
- the left banner changes to `Installing character pack`
- the LED switches to the pairing/install color
- after `char_end`, the pack becomes the active custom species

## Manifest Shape

The current Pico parser expects a `manifest.json` file with:

- `mode`: optional, but if present it must be `text`
- `name`: pack display name
- `colors`: optional hex colors using `#RRGGBB`
- `states`: object keyed by Buddy state name

State names:

- `sleep`
- `idle`
- `busy`
- `attention`
- `celebrate`
- `dizzy`
- `heart`

Each state can include:

- `delay`: frame delay in milliseconds
- `frames`: array of up to 8 short text frames

Current practical limits:

- frame strings should stay under about 24 characters
- the manifest must fit inside the device transfer buffer
- only one pushed custom pack is stored at a time

## Remove Or Replace A Pack

To remove the current custom pack on-device:

1. `Hold A`
2. `settings`
3. `reset`
4. `delete char`

Sending a new pack replaces the previous custom pack.
