# Claude Buddy Pico — Project Plan

Last updated: 2026-04-17

This plan exists to prevent scope drift, keep v1 shippable, and make the project land the way you actually want it to land.

## 1. What this project really is (incentive-honest framing)

The public framing is "fork of the Anthropic Hardware Buddy reference, reimplemented on Pico 2 W, with a 3D-printable Claude mascot case." The real incentive structure is different and worth naming, because every downstream decision follows from it:

- **Primary audience:** Anthropic staff and the Claude Desktop / Claude Code team. You want them to notice.
- **Secondary audience:** maker community on X. You want them to replicate, so the build count grows and the post keeps earning views after day one.
- **Tertiary audience:** your future self. A clean repo and a real writeup are assets that compound.

Show me the incentive and I will tell you the outcome. Two consequences of naming the primary audience honestly:

1. **Craft and technical honesty matter more than hype.** Anthropic engineers can smell a polished demo that hides a hack. They will also recognize a builder who did the unglamorous parts — encrypted GATT, protocol schema correctness, real bring-up pain documented in prose. Lean into the failure-mode journal entries; those are what a Claude Desktop engineer will actually screenshot and send to a colleague.
2. **Brand boundaries have to be respected, hard.** A "mini Claude Code mascot" is a fan artifact, not an official Anthropic product. The case, packaging, repo README, and blog all need to make that status unambiguous. Any ambiguity here turns a warm story into a legal email.

Anti-goals, stated explicitly so the plan can be checked against them:

- Not trying to look like an official Anthropic product.
- Not trying to maximize Claude API coolness per tweet at the expense of shipping v1.
- Not trying to port the upstream ESP32 codebase verbatim — that's already been ruled out and should stay ruled out.
- Not trying to build a business. This is a side project and a portfolio piece.

## 2. V1 — ship the port, cleanly, with a case

V1 is the non-negotiable shipping line. Nothing from v2 lands on main until V1 is tagged, posted, and accepting PRs.

### 2.1 V1 exit criteria (hard gate)

V1 is done when all of these are true, not when most of them are:

- `claude_buddy_pico.uf2` flashes cleanly on a fresh Pico 2 W from a **release page**, not a build command.
- A beginner who has never touched a Pico can follow the build guide end to end and get a working buddy, or can tell you exactly where they got stuck.
- The 3D case prints on a stock Bambu / Prusa / Ender-class FDM printer in PLA or PETG without modification, with documented print settings.
- The repo has a single canonical firmware target. The abandoned scaffold files are either deleted or moved to `src/_legacy/` with a one-line README that explains why they're there.
- The upstream repo's license has been read, honored in the fork's `LICENSE` and `NOTICE`, and attribution to Anthropic is visible in the README.
- The blog post is drafted and reviewed.
- The X thread is drafted, with photos and at least one GIF, and a hook post is ready.
- At least three separate people outside you have built or flashed the device. Friends count. This is a reality check before public launch.

### 2.2 Repo hygiene before the repo goes public

This is the single most impactful pre-launch task relative to effort. The current repo has architectural drift that will cost you reader trust within the first 60 seconds of a GitHub visit.

- **Decide the monolith question.** `claude_buddy_pico.cpp` is 1,884 lines. You can either (a) ship it as-is with a `// TODO(v2): split by responsibility` note and a paragraph in the README explaining why you chose not to refactor pre-launch, or (b) do a targeted split into `ble/`, `ui/`, `state/`, `buddy/`. Option (a) is honest and faster. Option (b) is more impressive to an engineer reading the code, but is real risk because every refactor breaks some working behavior you can't easily re-verify. **Recommendation: (a) for V1, with an explicit comment. The honesty is the taste signal.**
- **Prune or quarantine the abandoned scaffold.** `main.cpp`, `ui.cpp`, `ui.hpp`, `protocol.cpp`, `protocol.hpp`, `ble_nus.cpp`, `ble_nus.hpp`, `settings.cpp`, `settings.hpp`, `battery.cpp`, `battery.hpp`, `app_state.hpp`. Pick one of: delete, or move to `src/_legacy/` with a 3-line README. Not touching them at all is the worst option because it signals "I'm not paying attention."
- **Normalize the device name.** `"Claude Pico"` in the GATT and advertisement vs `"Claude"` in `BuddyState::device_name`. Pick one. `"Claude Pico"` is better — distinguishes it from official products and reads correctly in Claude Desktop's device picker.
- **Verify encryption enforcement at the GATT layer, not just at connect time.** Confirm BTstack actually rejects unencrypted writes to the NUS RX characteristic given the current `.gatt` file. If not, add the stronger flag combination and document it.
- **Add a `LICENSE`, a `NOTICE`, and a `CODE_OF_CONDUCT.md`.** Check upstream's license; most Anthropic public repos are MIT or Apache-2.0. Match obligations exactly.
- **Write a README that does the first 30 seconds of the story well.** Lead image should be the assembled device with the case, on-screen. Then a two-sentence pitch. Then a BOM link, a flash-the-UF2 shortcut, and a "build from source" section. Everything else goes under collapsible sections.

### 2.3 Binary release strategy

This is the highest-leverage design decision for adoption. The toolchain is a wall. If a reader has to install `arm-none-eabi-gcc` to try the device, you've lost 90% of them.

- Produce signed, versioned `.uf2` artifacts on every tagged release: `claude_buddy_pico.uf2`, `claude_buddy_ble_smoke.uf2`, `claude_buddy_pico_smoke.uf2`.
- Drag-and-drop install instructions in the README, with an embedded GIF of the BOOTSEL + drag-drop workflow.
- Consider a GitHub Actions workflow that builds the UF2s on every push to `main`. This also forces you to keep the build reproducible on a clean environment — which is currently not proven.

### 2.4 Current firmware gaps worth closing in V1

These are the bring-up journal's loose ends:

- **Secure pairing end-to-end.** Journal flagged it, GATT file attempts it, next pass in `claude_buddy_pico.cpp` was planned around it. Validate with real Claude Desktop that the link reports as encrypted and bonding persists across a power cycle.
- **deviceStatus schema drift guard.** Claude Desktop's schema will change under you. Write a small Python or JS script in `tools/` that reads your firmware's `deviceStatus` JSON over BLE and diffs it against a reference. Ship the reference. Readers who get the "No response" bug in three months have a one-command diagnostic.
- **Forget-bond flow on-device.** Settings menu already has it. Make sure it actually clears the BTstack flash bank and that re-pairing works immediately afterwards.
- **Battery low threshold and behavior.** Decide what happens at 15%, 5%, and below. Screen dim, LED red, reduce advertising interval, etc. This is easy to hand-wave and will make the product feel unfinished if you don't close it.

## 3. V2 — evolved software, parallel track with hard gates

You asked for v2 planned in parallel. Fine. The rule that protects v1 is: **no v2 code lands on `main` before the v1 git tag is cut and the blog post is published.** Work on v2 in a `v2/` branch or a separate worktree. The day v1 ships, you merge the first v2 increment and restart the cycle.

Pick one headline feature for v2, not three. Side projects die from scope, not from lack of ideas.

Ranked by "impressive to Anthropic / shareable to maker audience / feasible in evenings and weekends":

1. **On-device character pack loading from Claude Desktop over BLE (folder push / GIF characters).** This is the upstream feature you deliberately deferred, and closing the loop is a crisp story. Cute, visible, demos beautifully on video. Bounded in scope because the protocol is already specified. Recommendation: this is v2.
2. **Persona / buddy personality system.** Each buddy already has seven states. You could add per-buddy voice lines that render on the display when Claude sends permission prompts ("the cat thinks about your bash command"). Charming but requires writing content, not just code.
3. **Wi-Fi OTA firmware update.** Technically interesting, genuinely useful for iteration, but a security minefield. Only worth it if you want to learn it. Not recommended as v2 headline because it's all plumbing and no visible payoff.
4. **On-device MCP client experiment.** The Pico is too weak for real LLM work, but you could imagine the buddy surfacing MCP tool calls and tool results visually. Interesting because it reframes the device from "status display" to "ambient interface for MCP." Very Anthropic-flavored. Hardest to scope and easiest to never finish. Keep as a v3 daydream.
5. **Persistent stats and settings in flash.** Upstream uses NVS; you don't persist today. Small, boring, necessary eventually. Roll into v2 as a sub-task, not as the headline.

Recommendation: **v2 headline = folder push / character pack loading, with persisted stats as a bundled sub-task.** The story is "closed the last gap between this port and the official reference, plus loaded custom buddies over BLE." That writes itself.

## 4. The 3D case — design track

You have nothing yet, so this is the biggest and riskiest scope item. Estimate honestly: a from-scratch organic case with good fit, good print-ability, and real visual charm is roughly 20–40 focused hours across three or four iterations. Less if you're fast with CAD; more if you want the mascot to look genuinely good in photos.

### 4.1 Concept

The brief, written so you can check choices against it later:

- Instantly recognizable as a Claude Code mascot without infringing brand.
- Fits around the Pico 2 W + Display Pack 2.8 + LiPo SHIM + 2000mAh LiPo in the assembly order already proven in the bring-up journal.
- Sits on a desk at an angle that shows the display to a seated user.
- Printable in two pieces max, ideally with no supports, on a common FDM printer.
- Reads as friendly and small in photos. Photogenic is a real design constraint here because the X thread depends on it.

Brand-respect rules:

- Do not use Anthropic's wordmark or logo on the case.
- Do not use the literal Claude avatar. Reference, don't copy.
- Make "fan-built, not official" clear on the case in a discreet way — for example, a small tag on the underside reading "unofficial community build."
- Run the final look past Anthropic's public brand guidelines before publishing, and consider emailing someone on the Claude team before launch if you know anyone there. The cost of that email is near zero. The cost of a takedown request after you've shipped is the story turning sour.

### 4.2 Phases

1. **Paper / foam sketches.** Two evenings. Get to a silhouette you like. Don't touch CAD yet.
2. **Mechanical-fit skeleton in CAD.** Import or model the PCB, display, and LiPo as blocks with real dimensions. Confirm the case can physically enclose them with clearance. This is the boring part that saves print iterations.
3. **Aesthetic pass in CAD.** Layer the mascot form over the skeleton. Keep constraints visible: antenna keep-out, display window, button access, USB-C port access, BOOTSEL button access, LiPo SHIM power button access, airflow gaps.
4. **Print iteration 1.** Fit test only. Draft mode is fine. Expect the first print not to fit.
5. **Print iteration 2.** Fit and function. Buttons should be pressable. Display window should not crop pixels. USB-C should plug in without strain on the case.
6. **Print iteration 3 (final).** Aesthetic polish. Final material choice. Photography-ready.
7. **Release packaging.** STL + STEP + print profile notes + sliced preview images + a "known-good print settings" block in the README.

### 4.3 CAD tool recommendation

- **Fusion 360 (personal, free):** best all-around. Parametric, plays well with imported PCB dimensions, exports clean STLs. Recommended unless you already know another tool better.
- **OnShape (free tier):** browser-based, good for parametric, but files are public on free tier — may be fine since you want to release the model anyway. Legitimate alternative.
- **Blender:** only if the mascot form is primarily organic and you want sculpt tools. Risk: poor parametric control around PCB cutouts leads to iteration hell.

### 4.4 Mechanical constraints list (fill in exact numbers during the skeleton phase)

- Pico 2 W antenna keep-out (top of board, no metal, minimal plastic thickness within the keep-out).
- LiPo pouch clearance; no point load on the pouch.
- Display window cutout aligned to the visible raster, not to the board edge.
- Button plungers or cutouts for A/B/X/Y.
- USB-C port alignment with enough room for thick cables.
- Pico BOOTSEL button reachable through a small hole or with a removable top.
- LiPo SHIM power button reachable without disassembly.
- Ventilation if the case is enclosed — CYW43 can warm up under sustained BLE activity.
- Mounting bosses for M2 screws or snap-fit tabs. Prefer screws for iteration; snap-fits for the final version.

### 4.5 Material recommendation

- PLA for the first three prints. Cheap, prints clean, good for iteration.
- PETG for the final version. Higher heat tolerance next to the LiPo pouch and the CYW43. Slightly harder to print cleanly but worth it.
- Avoid ABS. Fumes, warp, not worth it for this project.

### 4.6 Release format

- `hardware/case/stl/` — ready-to-print STL files, one per part.
- `hardware/case/step/` — parametric STEP file for people who want to modify.
- `hardware/case/print-settings.md` — layer height, infill, support strategy, material, orientation, estimated print time, tested printers.
- `hardware/case/photos/` — reference photos of a known-good print.

## 5. Build guide

Layered structure: quick-start for makers, expanded appendix for beginners. Same document, collapsible sections.

### 5.1 Section order

1. **What you're building and what it does.** One paragraph. One photo.
2. **Bill of materials with direct links.** Pimoroni, Pi Hut, Adafruit, Amazon where appropriate. Include total cost.
3. **Tools required.** Soldering iron, solder, flush cutters, small Phillips, 3D printer or access to one.
4. **Print the case.** Print settings, estimated print time, known-good printers. Link to STL.
5. **Assemble the hardware.** The bring-up journal already nailed this — polish it. Photos at every step. Call out the LiPo SHIM back-to-back method, the antenna keep-out, the order-of-operations for soldering.
6. **Flash the firmware.** UF2 drag-drop. Screenshots. The BOOTSEL-with-battery-attached gotcha goes here.
7. **Pair with Claude Desktop.** Screenshots of the Hardware Buddy window. Feature-gating note, because the menu may be hidden.
8. **First impressions and what each button does.** Mirror the `docs/protocol-map.md` button map.
9. **Troubleshooting.** Take straight from the bring-up journal — the five scenarios there are gold. Add one or two more as beta testers find new failure modes.
10. **Beginner appendix.** Soldering basics. What a LiPo is and how to not set it on fire. What BLE pairing actually is. What to do if your Pico won't show up.

### 5.2 Photography plan

You have photos already. Audit them against the section list. Identify gaps. Shoot the missing photos deliberately, with consistent lighting and background. A $30 cheap light tent does more for the blog post than another firmware feature.

Required shots:

- Fully assembled device, hero shot, three angles.
- Each assembly step, clean background.
- Display close-ups showing each persona state.
- Reaction GIFs: permission prompt arriving, approve, deny, celebrate.
- One "scale" photo with a common object (coin, pen) so readers know how big it is.

### 5.3 Safety notes

- LiPo handling. Don't puncture, don't short, don't leave charging unattended.
- Soldering iron basics. Ventilation.
- First power-up over USB only, no battery. This is already in the journal; emphasize it.

This section is not optional. If a reader hurts themselves or damages hardware because the guide didn't warn them, the story changes direction fast.

## 6. Content production

Sequence matters. The repo is the canonical home. Everything else points at it.

### 6.1 Repo (canonical)

- README with hero image, BOM, flash instructions, link to blog, link to X thread, link to case STLs.
- Tagged release with UF2 binaries.
- Clean commit history from the point of public launch forward. Pre-launch you can squash or rewrite. Post-launch, no force pushes on main.
- Issues and discussions enabled. Seed two or three issues as "good first issue" placeholders (e.g., "add buddy species X," "support alternate LCD").
- A `CONTRIBUTING.md` that sets expectations: this is a side project, responses may be slow, PRs welcome but may not be merged.

### 6.2 Long-form blog post

Structure, based directly on your bring-up journal but pitched for a reader who isn't inside your head:

1. Hook: a photo and one sentence about what the device does.
2. Why build a Pico version when an ESP32 reference exists.
3. The exact parts and why they're enough.
4. The mechanical trap with the LiPo SHIM and Display Pack.
5. Hardware smoke test working, first display output.
6. The macOS toolchain detour (this is shorter than you think but has to be accurate).
7. The Claude Desktop feature-gating detour. **This is the single most publishable section.** An Anthropic engineer will screenshot this and it will get attention internally. Write it carefully: factual, no snark, ends with "the app contained the feature, it was gated for rollout, here's what it means for anyone replicating."
8. The BLE advertisement overflow bug.
9. The deviceStatus schema mismatch and how you figured it out.
10. The secure pairing work.
11. On-device UI decisions.
12. Designing a case from nothing.
13. What's next (the v2 teaser — one paragraph only).
14. How to build your own. Link to repo.

Target length: 2,500–4,000 words. Any longer and readers bail. Any shorter and it feels thin for this much engineering depth.

### 6.3 X thread

Thread structure, 8–12 posts:

- Post 1: hero image + one sentence hook. No link. The goal of post 1 is to stop the scroll.
- Posts 2–4: the build. One crisp photo per post. One line of copy.
- Post 5: the Claude Desktop menu hidden moment. This is the "did you know" beat that makes people reshare.
- Posts 6–8: the BLE bring-up bugs. One per post. The "look at this specific thing I found" rhythm.
- Post 9: the on-device UI reveal, ideally a GIF.
- Post 10: the case reveal. Ideally a GIF rotating around it.
- Post 11: a link to the repo and a link to the blog. Clean call to action.
- Post 12: optional teaser for v2.

Rules:

- One image or GIF per post. Silent threads die.
- Don't tag Anthropic accounts. Let them find it organically. Tagging reads as asking for attention; not tagging and being found reads as craft.
- Post the thread the same day as the repo release and the blog post. Don't stagger. Attention consolidates.

### 6.4 Video GIFs

Shortlist, each 2–6 seconds, loopable:

- Screen persona cycling through states.
- Permission prompt arriving, A-button approval, celebrate animation.
- Side-by-side: Claude Desktop tool-call prompt + buddy reacting.
- Plugging in battery, screen waking up.
- Case reveal rotation on a turntable or in a 3D viewer.

Capture with your phone at 60fps, convert to GIF with a known-good ffmpeg recipe. Commit the ffmpeg command into `tools/gifs.sh` so the recipe is part of the repo.

## 7. Launch plan

### 7.1 Pre-launch checklist (every item must be true)

- V1 exit criteria all green.
- Three outside builders have flashed or replicated without your direct help.
- Repo is public-ready: README, LICENSE, NOTICE, CONTRIBUTING, CODE_OF_CONDUCT.
- UF2 release artifacts attached to the git tag.
- Blog post drafted, reviewed, link tested.
- X thread drafted with all assets ready in a single folder.
- Case STL and STEP files uploaded. Print settings documented. Reference print photographed.
- Brand-respect pass complete. Nothing on the case or in the repo claims official status.
- Optional but recommended: a quiet heads-up to one or two Anthropic contacts if you have them. Not for permission. For courtesy and because it makes the story go better.

### 7.2 Launch day sequence

1. Morning: push the git tag. UF2 artifacts attach via Actions or manual upload.
2. Morning: publish the blog post.
3. Late morning or early afternoon (X prime time for your target audience): post the thread.
4. Do not post thread replies or updates for the first four hours. Let the thread breathe.
5. Respond to replies genuinely and in prose. No marketing-speak.
6. Capture any replication photos people post for a future "builds in the wild" README section.

### 7.3 Post-launch

- Triage issues daily for the first week, weekly after.
- Merge real PRs. Decline scope-creep PRs with kindness and a reason.
- Collect feedback in a `docs/feedback.md` that seeds v2 planning.

## 8. Risk register

Each risk is paired with its incentive analysis — why someone on the other side of it might push back.

**Brand / IP risk.** The mascot blurs into official-looking Anthropic brand. *Incentive:* Anthropic's legal team is obligated to protect the mark even when intent is benign. *Mitigation:* explicit "unofficial" labeling, no wordmark, distinct visual identity, courtesy email if possible.

**Safety risk.** Reader hurts themselves on the iron or torches a LiPo. *Incentive:* people skim. Warnings in the middle of a doc are not read. *Mitigation:* safety notes at the top of the assembly section, not in an appendix.

**Replication failure.** Reader bricks a Pico, blames you in public. *Incentive:* a public failure story gets engagement even when the project is fine. *Mitigation:* robust troubleshooting section, recovery instructions that assume the worst.

**License risk from upstream fork.** Not honoring upstream obligations. *Incentive:* Anthropic is reasonable but has to be consistent. *Mitigation:* read the license today, match it exactly, keep attribution visible.

**Scope creep into v2.** V2 ideas crowd out v1 shipping. *Incentive:* every new idea feels more exciting than finishing. *Mitigation:* the hard gate rule above. V2 code lives on a branch until v1 is tagged.

**Toolchain decay.** Six months from now, readers can't build. *Incentive:* upstream tools move on. *Mitigation:* pinned versions in `scripts/`, CI workflow that builds on a clean image, UF2 release artifacts so most readers never touch the toolchain.

**Host-side protocol drift.** Claude Desktop changes the deviceStatus schema. *Incentive:* the desktop app will evolve; the fork won't always follow. *Mitigation:* the diagnostic script in `tools/` and a pinned version note in the README.

## 9. Rough timeline

Stated as effort buckets, not calendar weeks, because side projects don't respect calendars.

- **V1 firmware final polish:** 1–2 focused weekends. Close the secure-pairing and schema-drift items. Prune dead scaffold. Decide the monolith question.
- **Case design:** 3–5 focused weekends. Expect three print iterations.
- **Build guide writing + photography:** 1–2 weekends. Most of the content is lift from the bring-up journal.
- **Blog post writing:** 1 weekend.
- **X thread + GIFs:** 1 weekend, mostly waiting on prints and firmware.
- **Launch week:** 1 week for triage and responding to early feedback.

Realistic: six to ten weekends from today to public launch, if no major blockers. Budget double that honestly. Side-project estimates are wrong in the same direction every time.

## 10. First five things to do on Monday

In order. Don't skip.

1. Read the upstream license and update `LICENSE` and `NOTICE` in this repo to match.
2. Decide the monolith question. Add the TODO comment or start the refactor branch. Don't leave it ambiguous.
3. Delete or quarantine the dead scaffold files.
4. Stand up a GitHub Actions workflow that builds all three UF2 targets on push.
5. Start paper sketches for the case. Don't open CAD yet.
