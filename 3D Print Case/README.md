# Claude Buddy Pico — 3D Printed Case

Parametric case design for the Pico Buddy hardware stack. The outer silhouette is the Claude Code mascot; the display is the mascot's face.

## Hardware inside this case

- Raspberry Pi Pico 2 W with headers
- Pimoroni Pico Display Pack 2.8 (PIM715)
- Pimoroni LiPo SHIM for Pico (PIM557)
- Pimoroni 2000mAh LiPo (BAT0005)

## Case design decisions

- Desk buddy form factor: stands upright with a slight backward tilt, like a picture frame.
- Display IS the face: the firmware's pet renderer draws eyes and expressions on the screen. The case provides the mascot body silhouette around the display window.
- Two-part split: front shell plus rear hatch held by M2 screws. The stack slides in from the back; battery and USB stay serviceable.
- Print material: Bambu Lab ABS on the P1S, theme color matched to the Claude Code mascot orange.

## Why trimesh instead of CadQuery

The public reference for Claude-authored 3D parts ([flowful-ai/cad-skill](https://github.com/flowful-ai/cad-skill)) uses CadQuery. This project uses `shapely + trimesh + manifold3d` instead because:

- CadQuery's sketch solver hard-requires `nlopt`, which needs a C++/CMake build step that the Cowork sandbox cannot complete.
- For an enclosure whose complexity is "rounded-silhouette extrusion with cutouts and mount bosses", `shapely` profiles plus `trimesh` booleans cover every geometry operation we need.
- The methodology from the article is preserved: parametric Python script, STL output, 4-view preview render that Claude self-reviews before handing off.

What we lose: native fillets on arbitrary 3D edges, and sketch-constraint solving. Neither affects this case — rounded corners come from 2D profile buffering, and chamfers come from offset operations.

## Folder layout

- `scripts/buddy_case.py` — main parametric model (one module, top-to-bottom readable)
- `scripts/render.py` — 4-view preview renderer (front, side, back, iso)
- `scripts/build.sh` — one-shot: build STLs, render previews
- `stl/` — output STL files (front shell, rear hatch, assembly)
- `renders/` — preview PNGs (committed so diffs are visible in PRs)
- `reference/` — source mascot image and hardware reference files
- `Sample display base.stl` — earlier reference sleeve for the Pico Display Pack 2.8, used to sanity-check outer envelope dimensions

## Iteration loop

1. Edit parameters at the top of `scripts/buddy_case.py`.
2. Run `bash scripts/build.sh` to regenerate STLs and 4-view renders.
3. Review the PNGs in `renders/`.
4. Once the render looks right, slice the STLs in Bambu Studio for the P1S, print in ABS, and bring measurements back for the next pass.

## Printability targets

- Minimum wall thickness 1.6 mm (1.2 mm absolute floor for FDM)
- No unsupported overhangs above 45 degrees
- Chamfers, not fillets, on downward-facing bottom edges
- Hole-to-shaft clearance 0.3 mm for M2 screws
- Bridge spans kept under 20 mm

## Print orientation guidance (finalized after first print test)

- Front shell: print face-down on the bed. The mascot face is the most visible surface, and face-down gives a clean, line-free front.
- Rear hatch: print face-down on the bed. The visible outer face of the hatch wants the same quality.
- Both parts print without supports if chamfers on downward-facing internal edges stay at 45 degrees.
