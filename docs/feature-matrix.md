# Feature Matrix

## Original Buddy To Pico Mapping

| Upstream Buddy Feature | Pico Hardware Equivalent | Status | Notes |
| --- | --- | --- | --- |
| M5Stick portrait pet screen | 320x240 landscape pet column + side rail | Adapted | Pico uses a left pet column and right content panel |
| `A` next / approve | `A` next / approve | Kept | Same role as upstream |
| `B` page / deny | `B` page / deny | Kept | Transcript paging is chunked and labeled on-screen |
| power button menu semantics | `Y` home/back and `Hold A` menu | Adapted | SHIM button remains hardware-only |
| shake to dizzy | `Hold X` dizzy | Adapted | No IMU on Pico build |
| face-down nap | `Hold Y` nap / wake | Adapted | Explicit control instead of IMU gesture |
| speaker / beep cues | none | Removed | No speaker on this hardware |
| RGB LED | Display Pack RGB LED | Kept | Used for pairing, busy, prompt, low battery |
| idle charging clock | fixed landscape dock clock | Adapted | Only when USB + time sync + idle |
| ASCII species built into firmware | ASCII species built into firmware | Kept | 18 built-in pets remain available |
| single installed pushed character | single installed pushed character | Kept | Stored in flash on Pico |
| GIF character rendering | text-manifest character rendering | Partial | Transfer wire protocol is present; Pico GIF renderer is future work |
| `status` reply | `status` reply | Kept | Validated against Claude Desktop during bring-up |
| `name`, `owner`, `unpair`, `species` | same | Kept | Persisted in Pico flash |
| `char_begin/file/chunk/file_end/char_end` | same | Kept | Used for installing one pushed text pack |

## Settings On Pico

Persisted settings buckets:

- brightness
- LED enabled
- transcript enabled
- demo mode
- dock clock enabled
- selected pet / character
- device name
- owner

Persisted stats buckets:

- approvals
- denials
- response velocity ring
- nap time
- level
- cumulative tokens

## Known Differences Worth Calling Out Publicly

- The Pico build is intentionally Pico-first, not a pixel clone of the M5Stick UI.
- The screen is used for clearer hierarchy instead of strict upstream layout parity.
- The LiPo SHIM button is never treated as a normal app button.
- The current pushed-pack renderer supports `text` manifests. Upstream-style GIF packs still need a Pico-native renderer pass.
