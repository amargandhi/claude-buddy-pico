# Internal Advanced Firmware Capability Review

This document is an internal engineering review of the exact current `Pico 2 W + Pico Display Pack 2.8 + LiPo SHIM + 2000mAh LiPo` build. Its purpose is to compare the current firmware against the canonical upstream `anthropics/claude-desktop-buddy` surface and identify the highest-leverage firmware work that still fits the current hardware stack.

It is intentionally not written as a public launch document. It is a source-backed planning artifact for future firmware decisions.

## 1. Reference Baseline And Scope

### Baseline

For this review, the canonical upstream reference is:

- upstream repo: [anthropics/claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy)
- upstream protocol contract: [REFERENCE.md](https://github.com/anthropics/claude-desktop-buddy/blob/main/REFERENCE.md)
- upstream example firmware behavior: [README.md](https://github.com/anthropics/claude-desktop-buddy/blob/main/README.md)

This resolves the earlier "Felix original repo" ambiguity in favor of the Anthropic upstream, because this fork explicitly cites that repo as the reference throughout the workspace.

### Scope

This review is limited to the current shipped hardware stack only:

- `Raspberry Pi Pico 2 W` with headers
- `Pimoroni Pico Display Pack 2.8` (`PIM715`)
- `Pimoroni LiPo SHIM for Pico` (`PIM557`)
- `Pimoroni 2000mAh LiPo` (`BAT0005`)

Out of scope for primary recommendations:

- external sensors or alternate displays
- different battery boards
- speaker or IMU add-ons
- redesigning the protocol surface on the desktop side

Unused on-board interfaces are still discussed when they materially affect future firmware choices.

### Evidence Base

Primary local implementation sources:

- [src/claude_buddy_pico.cpp](../src/claude_buddy_pico.cpp)
- [docs/feature-matrix.md](./feature-matrix.md)
- [docs/protocol-map.md](./protocol-map.md)

Supporting local sources:

- [README.md](../README.md)
- [docs/hardware-build.md](./hardware-build.md)
- [docs/software-setup.md](./software-setup.md)
- [docs/user-guide.md](./user-guide.md)
- [docs/limitations.md](./limitations.md)
- [third_party/pimoroni-pico/libraries/pico_display_28/pico_display_28.hpp](../third_party/pimoroni-pico/libraries/pico_display_28/pico_display_28.hpp)
- [third_party/pimoroni-pico/common/pimoroni_common.hpp](../third_party/pimoroni-pico/common/pimoroni_common.hpp)
- [third_party/pico-sdk/src/boards/include/boards/pico2_w.h](../third_party/pico-sdk/src/boards/include/boards/pico2_w.h)

## 2. Verified Hardware And Firmware Profile

### Exact BOM

- `Raspberry Pi Pico 2 W` with headers
- `Pimoroni Pico Display Pack 2.8` (`PIM715`)
- `Pimoroni LiPo SHIM for Pico` (`PIM557`)
- `Pimoroni 2000mAh LiPo` (`BAT0005`)
- micro-USB data cable

### Board And Display Profile

`Raspberry Pi Pico 2 W`

- RP2350-based microcontroller board
- dual-core Arm Cortex-M33 or dual-core Hazard3 RISC-V, up to `150MHz`
- `520KB` SRAM
- local implementation baseline: `4MB` flash, because the checked-in `pico2_w` board header sets `PICO_FLASH_SIZE_BYTES` to `4 * 1024 * 1024`
- on-board `2.4GHz 802.11n` Wi-Fi and `Bluetooth 5.2`
- USB `1.1` device/host support
- supported input voltage `1.8V-5.5V`
- physical board size `21mm x 51mm`

`Pico Display Pack 2.8`

- `2.8"` IPS LCD
- `320x240` landscape-capable panel
- `ST7789V` display controller
- quoted luminance `250 cd/m2`
- active area `57.5mm x 43.2mm`
- four front-facing buttons
- one RGB LED
- one `Qw/ST` connector
- one `SP/CE` connector

### Physical And Mechanical Constraints

- the current build has no IMU and no speaker
- the LiPo SHIM side button is treated as hardware-only power control, not an application input
- the Pico 2 W antenna end needs clearance from the battery and from nearby metal
- this exact stack has a known recovery constraint: with the battery attached, `BOOTSEL` entry is less reliable; the documented safe path is battery disconnect first, then USB `BOOTSEL`
- the display connector and LiPo SHIM stack-up make the assembly mechanically tighter than the electrical schematic suggests

### Real Pin And Resource Map Used By Current Firmware

Display pack buttons:

- `GP12`: A
- `GP13`: B
- `GP14`: X
- `GP15`: Y

Display path:

- `GP17`: display chip select
- `GP18`: display SPI clock
- `GP19`: display MOSI
- `GP16`: display data/command
- `GP20`: display backlight PWM

Display pack RGB LED:

- `GP26`: red
- `GP27`: green
- `GP28`: blue

Wireless and board-reserved functions:

- `GPIO29 / ADC3`: `VSYS / 3` measurement path
- `CYW43 WL GPIO 2`: USB presence sensing
- wireless subsystem shares lines with `VSYS` measurement, so analog voltage reads are constrained by radio traffic timing

Latent but currently unused board/display interfaces:

- `Qw/ST` I2C path is physically present on the display pack
- Pico 2 W Wi-Fi is present in hardware but unused by the main firmware

### Current Runtime Profile

From the current firmware:

- UI tick: `40ms`
- battery sample interval: `2000ms`
- prompt keepalive / stale prompt window: `30000ms`
- auto-dim after inactivity: `20000ms`
- auto screen-off after inactivity on battery: `30000ms`
- hold threshold for long-press actions: `800ms`
- fixed passkey in display role: `123456`
- five backlight levels: `48, 88, 128, 176, 220`
- level progression: `50,000` tokens per level

### Current Persistence Model

The current firmware persists state in one flash sector at the end of flash, separate from the BTstack flash bank. It stores:

- device name
- owner
- brightness level
- LED enabled
- transcript enabled
- demo mode
- dock clock enabled
- approvals
- denials
- nap seconds
- total tokens
- level
- response velocity ring buffer
- selected ASCII species
- one installed custom text pack

Persistence details:

- custom magic and versioned blob
- CRC-protected
- debounced writes with a `2000ms` schedule window
- current storage blob version: `3`

### Current Rendering And Character-Pack Envelope

Display rendering:

- current framebuffer format is `RGB332`
- effective full-screen framebuffer size is about `76.8KB` for `320x240`
- this is a deliberate RAM tradeoff against `RGB565`

Custom pack model:

- only one custom pack can be installed at a time
- transfer flow accepts `char_begin`, `file`, `chunk`, `file_end`, `char_end`
- current parser only accepts a text-manifest pack
- manifest capture buffer is `4096` bytes
- `PackState` supports up to `8` text frames per state, each frame string up to `24` characters
- the pack becomes part of the persistent blob after a successful install

### Current Battery And Status Telemetry Model

Battery model:

- battery percentage is estimated from `VSYS` voltage only
- there is no real current-sense path in this build
- current code reports `bat.mA = 0` in the `status` response
- low battery is derived from voltage thresholding, not coulomb counting

Current `status` shape:

- `name`
- `sec`
- `bat: { pct, mV, mA, usb }`
- `sys: { up, heap }`
- `stats: { appr, deny, vel, nap, lvl }`

Implementation detail:

- `heap` is currently hard-coded to `0`
- `mA` is currently hard-coded to `0`
- `sec` reflects current link encryption state

### Source Contradictions That Matter

| Topic | Contradiction | Working Baseline For This Repo | Why |
| --- | --- | --- | --- |
| Pico 2 W flash size | Raspberry Pi has public material that currently disagrees; one Pico 2 W datasheet section says `2MB`, while the product brief, Pico 2 family docs, and the checked-in `pico2_w.h` board header say `4MB` | Use `4MB` | The repo builds against the local SDK board header, and persistence offsets are computed from `PICO_FLASH_SIZE_BYTES` in that header |
| Pico 2 W thermal limit | Raspberry Pi product pages/product brief list `-20C to +85C`, while the Pico 2 W datasheet gives `-20C to +70C` including self-heating | Use `+70C` as the conservative board-level limit | This is the safer engineering assumption for a wireless enclosed device |

## 3. Claude Surface Vs Current Pico Implementation

### Comparison Matrix

| Upstream feature | Upstream contract | Current Pico behavior | Hardware fit | Verdict |
| --- | --- | --- | --- | --- |
| Availability gate | Bridge exists only when Claude desktop apps expose Hardware Buddy in developer mode | Pico firmware is compatible once the host exposes the bridge; bring-up notes already show host-side feature gating can hide the menu independently of the firmware | Neutral host dependency, not a device constraint | `implemented` |
| Discovery and advertising | Advertise a name starting with `Claude` over BLE NUS so the picker can filter to the device | Advertises as `Claude Pico`, uses the expected NUS UUIDs, and is discoverable from the Hardware Buddy window | Strong fit | `implemented` |
| Transport | UTF-8 JSON, newline-delimited, over Nordic UART Service | Current firmware accumulates line-based JSON and handles the expected command/heartbeat model | Strong fit | `implemented` |
| Secure pairing and bonding | Upstream recommends LE Secure Connections bonding, encrypted characteristics, DisplayOnly passkey, and `sec` reporting | Current firmware enables secure-connections-only mode, fixed display-role passkey, pairing status UI, and secure state reporting; reconnect security state is surfaced in the UI | Strong fit | `implemented` |
| Heartbeat snapshot | Upstream sends `total`, `running`, `waiting`, `msg`, `entries`, `tokens`, `tokens_today`, and optional `prompt` | Current firmware consumes the heartbeat as its main source of live state, counters, transcript lines, prompt state, and token counts | Strong fit | `implemented` |
| Transcript and one-line message handling | Upstream provides both a short `msg` and recent `entries` for display | Pico uses `msg` plus wrapped transcript entries, with paging and a transcript toggle in settings | Strong fit, improved by larger screen | `implemented` |
| Permission prompts and decisions | `prompt.id` must be echoed back as `permission once` or `permission deny` | Prompt overlay is central to the UI; A/B send upstream-compatible decisions and X toggles raw prompt details | Strong fit | `implemented` |
| Turn events | Upstream can send `evt:"turn"` with raw SDK content array, capped around `4KB` | Current firmware only acknowledges turn events as optional decoration and reduces them to a transient banner | Usable, but currently underexploited | `partial` |
| Time sync | Upstream can send `[epoch, offset]` time sync on connect | Current firmware stores local epoch plus sync time and uses it for the dock clock | Good fit | `implemented` |
| Identity commands (`name`, `owner`) | Upstream can set large device name and owner name | Current firmware persists both and shows them in the layout | Strong fit | `implemented` |
| Status polling | Upstream polls `status` for the Hardware Buddy panel | Current firmware returns the expected envelope with encryption flag, battery, uptime, and stats, but omits real heap/current telemetry | Strong fit with some stubbed metrics | `implemented` |
| Unpair | Upstream expects `unpair` to clear stored bonds and allow fresh pairing | Current firmware handles `unpair`, clears secure/paired state, and drops bond data when connected | Strong fit | `implemented` |
| Folder push transport | Upstream folder push is content-agnostic under about `1.8MB` total and streams regular files sequentially | Current firmware implements the transport acknowledgements and sequential receive model, but only captures a text manifest payload and ignores general file use | Transport fits; current product scope is intentionally narrower | `partial` |
| GIF pets | Upstream supports streamed GIF character packs for the portrait ESP32 device | Current Pico firmware does not decode or render GIF packs; it only installs text-based custom packs | Possible in principle, but not yet aligned with current implementation | `not implemented` |
| Built-in pet roster and seven states | Upstream ships 18 ASCII pets and seven states: sleep, idle, busy, attention, celebrate, dizzy, heart | Pico keeps the seven-state model and built-in ASCII species count, adapted to landscape layout | Strong fit | `implemented` |
| Hardware controls and gestures | Upstream uses A/B, hold A, power button, shake, and face-down gestures | Pico remaps missing hardware: hold A menu, hold X dizzy, hold Y nap, SHIM button reserved as hardware power only | Good fit after adaptation, not layout-parity | `implemented` |
| Screen dim/off behavior | Upstream auto powers off after inactivity and wakes on button input | Pico auto-dims, can fully screen-off on battery, supports explicit screen-off, and wakes on button input | Strong fit | `implemented` |
| Idle behavior | Upstream idle behavior centers on the pet staying present and reacting to session urgency | Pico keeps the same idle/busy/attention model and adds a USB-only dock clock adaptation for idle linked state | Better than upstream on this screen size | `implemented` |

### Local Deviations And Extensions

- `species` is handled by the Pico firmware as a local extension and persistence feature; it is not part of the upstream protocol contract this review treats as canonical.
- the UI is intentionally landscape-first, not a clone of the upstream portrait M5Stick layout.
- upstream motion and power gestures are replaced with deterministic button holds because this build has no IMU and the SHIM button is reserved as hardware power.
- `evt:"turn"` is currently treated as optional decoration instead of a primary UI data source.
- folder push is implemented as a transport, but the installed payload format is a text-manifest pack rather than upstream GIF assets.
- `status` compatibility is maintained, but some fields are placeholders today: `bat.mA = 0`, `sys.heap = 0`.

## 4. Current-Hardware-Only Opportunity Map

### Claude-Driven Opportunities

| Class | Opportunity | What it would use | Why it matters |
| --- | --- | --- | --- |
| `Verified now` | Promote the current prompt overlay into the core device identity | `prompt.id`, `prompt.tool`, `prompt.hint`, secure state, button approvals | The device is already strongest when acting as a low-latency approval surface; leaning into this improves utility without adding hardware |
| `Enabled by current hardware, not yet built` | Turn-event tape instead of banner-only decoration | `evt:"turn"` raw content array, current right-panel real estate | The upstream desktop already emits a richer event surface than the current firmware uses; the larger screen can show a short-lived event digest without sacrificing the buddy |
| `Enabled by current hardware, not yet built` | Tool-specific prompt micro-UX | `prompt.tool`, `prompt.hint`, current overlay and color system | Different tools imply different risk. The device can make approvals faster and safer by rendering tool-aware hints instead of a generic approval panel |
| `Enabled by current hardware, not yet built` | Transcript triage instead of raw rolling lines | `msg`, `entries`, `running`, `waiting`, `tokens_today` | The screen is large enough to separate "current status", "prompt risk", and "recent activity"; today it still behaves more like a compact transcript viewer |
| `Enabled by current hardware, not yet built` | Better status-poll diagnostics page | existing `status` envelope, persisted stats, connection/security state | The device already tracks useful approval latency and nap stats. A stronger diagnostics page would help firmware debugging and long-term usage insight |
| `Technically possible but poor fit` | Use folder push for richer non-text assets without changing UI model first | existing `char_begin/file/chunk/file_end/char_end` | The transport can carry more than the current parser accepts, but shipping a richer asset format before fixing the renderer and storage model would create complexity without clear user value |
| `Blocked by current hardware or current protocol surface` | Deep per-turn semantic browsing on-device | upstream turn events are capped and heartbeat is intentionally summary-level | The desktop surface does not stream enough structured context for a serious on-device session browser; the device should stay summary-first |

### Hardware-Only Power-Ups

| Class | Opportunity | What it would use | Why it matters |
| --- | --- | --- | --- |
| `Verified now` | Dock clock as a separate desk-object mode | USB detection, time sync, idle detection, larger landscape panel | This is already one of the most native uses of the current build and should be treated as a real mode, not a small side feature |
| `Enabled by current hardware, not yet built` | Trust-state-first UI | secure pairing state, passkey display, LED, dock/home chips | This build can show whether the link is paired, merely connected, or unencrypted more clearly than upstream because it has the screen area to do so |
| `Enabled by current hardware, not yet built` | Battery-aware display and radio behavior | VSYS readings, USB sense, idle timers, brightness control | Even without current sensing, the firmware can make better decisions about dimming, screen-off, and degraded modes under low voltage |
| `Enabled by current hardware, not yet built` | Wi-Fi-backed convenience features with no new hardware | Pico 2 W radio, existing flash and BLE control path | The board has unused Wi-Fi capacity. Small support features such as time fallback or update helpers are possible without changing the BOM |
| `Enabled by current hardware, not yet built` | Self-diagnostics and maintenance mode | status page, BLE state, battery voltage, flash-backed settings, BOOTSEL recovery notes | This build already has sharp failure modes. Putting recovery and health checks on-device would shorten debugging loops |
| `Technically possible but poor fit` | OTA update system as a headline feature | Pico 2 W Wi-Fi, flash, boot flow | Possible, but the security and recovery burden is high relative to the immediate value of this device |
| `Blocked by current hardware or current protocol surface` | Accurate charge/discharge telemetry | there is no current-sense path in the current BOM | Voltage-only percentage can be serviceable, but it will never behave like a real fuel gauge on this hardware |
| `Blocked by current hardware or current protocol surface` | Audio or haptic notification language | no speaker, no haptic motor in current hardware | Any serious audio or tactile UX is blocked by the current BOM |

## 5. Unique Hardware Insights For Maximum UI And Utility

> `Verified now`  
> Use landscape concurrency, not upstream modal parity. The Pico screen is wide enough to keep the buddy alive while also showing operational state. Do not collapse the interface back into a one-panel modal device just to mimic the M5Stick reference.

> `Verified now`  
> Treat `RGB332` as an intentional visual language, not a compromise to hide. It favors bold chips, hard contrast, and restrained palettes. A better UI direction for this panel is crisp information hierarchy, not faux-rich gradients that will look cheap in 8-bit color.

> `Verified now`  
> Use the LCD and LED as two separate channels. The screen should communicate meaning; the LED should communicate urgency. If both channels say the same thing, one of them is wasted.

> `Enabled by current hardware, not yet built`  
> Make trust state visually unavoidable. Pairing, encrypted-but-not-bonded, secure reconnect, and forgotten-bond states deserve persistent chips or banners. For a permission device, "am I on a trusted link?" is not secondary metadata.

> `Verified now`  
> Prefer deterministic button flows over depth. This hardware is good at one-step decisions and short cycling flows. It is bad at nested interfaces that ask the user to remember where they are.

> `Enabled by current hardware, not yet built`  
> Push the dock personality harder. USB present + time sync + no prompt + idle linked state is a distinct use case. It can become a stable ambient desk object mode rather than a fallback clock panel.

> `Verified now`  
> Never present the battery estimate as precision instrumentation. On this build, battery is a comfort signal, not a measurement instrument. Phrase and color it accordingly.

> `Enabled by current hardware, not yet built`  
> The display pack quietly consumes all three exposed ADC-capable pins for the RGB LED. That means analog expansion is much worse than the board headline specs suggest. Future firmware should assume digital/I2C-first enhancements unless the LED traces are sacrificed.

## 6. Priority Roadmap

### P0: Highest-Leverage Firmware Work On Current Hardware

| Item | Class | Why it matters | What Claude or hardware primitive it uses | Main implementation risk | Go / no-go |
| --- | --- | --- | --- | --- | --- |
| Turn-event digest UI | `Enabled by current hardware, not yet built` | It closes the biggest current gap between available Claude surface and visible device value without changing the BOM | `evt:"turn"` plus existing right-panel layout | Event payloads are capped and heterogeneous; the UI must summarize without assuming full message fidelity | `Go` |
| Tool-aware approval overlay | `Enabled by current hardware, not yet built` | This improves the device at its most valuable job: fast, safer permission decisions | `prompt.tool`, `prompt.hint`, secure state, LED channel | Overfitting prompts to tool heuristics could mislead if the hint is sparse or misleading | `Go` |
| Stronger diagnostics/info pages | `Enabled by current hardware, not yet built` | Internal iteration is still expensive; on-device visibility into pairing, storage, battery, and state transitions reduces debugging cost | current `status` payload, secure state, persisted stats, BOOTSEL guidance | Risk is mostly UI clutter, not hardware feasibility | `Go` |
| Make dock mode first-class | `Enabled by current hardware, not yet built` | The current hardware is unusually well-suited to a useful idle desk mode | USB detect, time sync, idle/running/waiting state, landscape panel | Risk is time spent polishing an idle mode instead of core Claude interactions | `Go` |

### P1: Strong Improvements That Still Fit The Current BOM

| Item | Class | Why it matters | What Claude or hardware primitive it uses | Main implementation risk | Go / no-go |
| --- | --- | --- | --- | --- | --- |
| Text-pack format expansion | `Enabled by current hardware, not yet built` | Current folder push is more constrained than the transport surface; a richer text-pack schema could unlock more personality without full GIF complexity | existing folder-push transport and manifest parser | Scope creep toward a second asset pipeline | `Go, but keep it text-first` |
| Battery-aware degraded modes | `Enabled by current hardware, not yet built` | Better low-power behavior will improve real-world usability more than cosmetic changes | VSYS estimate, USB detect, dim/off timers, LED | Voltage-only estimates are noisy and can cause annoying mode thrash if thresholds are naive | `Go` |
| Wi-Fi support features with tight scope | `Enabled by current hardware, not yet built` | The current hardware contains unused radio value | Pico 2 W Wi-Fi, flash, current settings model | Network features can quickly become a security and UX rabbit hole | `Go only for tightly bounded helpers such as time fallback or controlled maintenance flows` |

### P2: Ambitious Or Risky Work That Is Still Technically Possible

| Item | Class | Why it matters | What Claude or hardware primitive it uses | Main implementation risk | Go / no-go |
| --- | --- | --- | --- | --- | --- |
| Upstream-style GIF pack parity | `Technically possible but poor fit` | It is the most visible parity gap with the upstream example | folder push transport, flash, display renderer | Decoder complexity, memory pressure, flash budgeting, asset tooling, and visual quality tradeoffs on this display stack | `Conditional go only after the text-pack path and event UI are strong` |
| OTA-style firmware updates | `Technically possible but poor fit` | It could improve iteration speed and remote maintenance | Pico 2 W Wi-Fi, flash, boot path | Security, recovery, signing, and brick-risk far outweigh the short-term product value | `No-go as a near-term headline` |
| Full on-device session browser | `Blocked by current hardware or current protocol surface` | It sounds impressive but overreaches the current data surface | heartbeat snapshots and capped turn events | The protocol does not provide enough structured detail for a high-quality browser; the device would become a misleading thin client | `No-go` |

## Bottom Line

The current build is already a strong fit for three things:

- fast permission decisions
- ambient operational awareness
- high-character desk-object behavior during idle and linked states

The highest-value next steps are not new hardware. They are better use of what the current hardware and upstream Claude surface already provide:

- elevate turn events beyond banner-only handling
- make the approval experience tool-aware and trust-aware
- use the landscape layout to separate urgency, summary, and personality instead of imitating upstream modal behavior

The main traps to avoid are equally clear:

- do not sell battery telemetry as precision
- do not chase OTA before the core device utility is stronger
- do not force GIF parity before the current text-pack and event surfaces are doing enough real work
- do not let the reference ESP32 firmware dictate a UI shape that this screen no longer requires
