"""Parametric case for the Claude Buddy Pico - v3.

v3 changes (after user feedback "why are you not just building on the existing
design stl file? your design has many issues such as floating buttons and
wierd angles on flat surfaces"):

  - The existing ``Sample display base.stl`` is loaded and embedded (boolean
    union) as the literal internal frame. It already carries the cradle
    geometry, standoffs, and button bosses that fit the real PCB.
  - The outer mascot silhouette is grown to proper mascot proportions so the
    display cutout can comfortably clear the Display Pack 2.8 active area
    (58 x 43.5 mm) with margin.
  - The silhouette now has both LEGS and ARMS (small stub protrusions at the
    torso sides) so the silhouette actually reads as a Claude mascot and not
    as a picture frame.
  - Speculative button / USB / PCB-mount cutouts are removed; those live in
    the sample STL already. The user cuts the USB pass-through manually
    against the real PCB.

Coordinate conventions
----------------------
During construction the 2D silhouette lives in the shapely XY plane
(+X width, +Y mascot-up) and extrusion runs along +Z (depth front to back,
+Z = mascot face / display side).

At export time the mesh is rotated so:
  world +X = mascot width (left/right when looking at the face)
  world +Y = mascot depth (into the desk - back of case)
  world +Z = mascot height (up)

This matches CAD / slicer convention and the render.py view assumptions.

Run this directly or via scripts/build.sh. Outputs go to stl/.
"""

from __future__ import annotations

import math
from pathlib import Path

import numpy as np
import trimesh
from shapely.geometry import Polygon
from shapely.ops import unary_union


# ===========================================================================
# Parameters (mm unless noted) - tune these, then rebuild
# ===========================================================================

# --- outer mascot silhouette (build XY plane) ---------------------------
# The display cutout must clear the 58 x 43.5 mm active area of the
# Display Pack 2.8 with a few mm of margin on each side, so BODY_W / TORSO_H
# are sized to leave a comfortable mascot "frame" around that window.
BODY_W = 104.0            # overall torso width (arms stick out past this)
TORSO_H = 96.0            # torso vertical extent
CORNER_R = 14.0           # outer corner radius on the torso

# legs - two small rounded rectangles at the bottom edge, separated by a
# gap so they read as distinct feet.
LEG_W = 22.0              # width of each leg
LEG_H = 12.0              # height of the leg region (below torso bottom)
LEG_GAP = 14.0            # horizontal gap between the inner edges of the legs
LEG_R = 3.0               # leg corner radius

# arms - small stub protrusions extending from the sides of the torso at
# mid-height. The Claude Code mascot has tiny rounded "hand" nubs; we keep
# them deliberately small so the body still reads as the dominant shape.
ARM_W = 10.0              # how far each arm sticks out past the torso edge
ARM_H = 22.0              # arm vertical extent
ARM_R = 4.0               # arm corner radius
ARM_CY_OFFSET = -6.0      # arm vertical center relative to torso center (<0 = lower)

# --- body depth ---------------------------------------------------------
# Set equal to the sample STL's Z extent so the embedded frame sits flush
# front-to-back inside the mascot shell.
BODY_D = 23.0

# --- embedded sample STL placement --------------------------------------
# The sample STL (``Sample display base.stl``) is centered in X and sits in
# the upper torso (above mascot center) so its display opening reads as
# the "face". STL_Y_OFFSET shifts the STL upward in the build frame.
SAMPLE_STL_NAME = "Sample display base.stl"
STL_Y_OFFSET = 10.0

# --- front-face display cutout ------------------------------------------
# The sample STL already has a through-hole; but the outer mascot shell
# extrudes solid through that region, so we must cut a matching window
# in the shell. Size is active area plus tolerance, plus a little extra so
# it never clips the panel bezel.
DISPLAY_W = 60.0
DISPLAY_H = 46.0
DISPLAY_CX = 0.0          # centered horizontally
# centered vertically on the STL's own centroid, so the hole lines up with
# the STL's internal cradle opening.

# --- backward tilt ------------------------------------------------------
# The base is cut at this angle (about the front-bottom edge) so the model
# leans back slightly when placed on a desk. 0 disables.
TILT_DEG = 6.0

# --- derived ------------------------------------------------------------
BODY_H = TORSO_H + LEG_H
TORSO_CY = BODY_H / 2 - TORSO_H / 2   # torso center in build-space Y


# ===========================================================================
# Paths
# ===========================================================================

HERE = Path(__file__).resolve().parent
CASE_DIR = HERE.parent
STL_DIR = CASE_DIR / "stl"
SAMPLE_STL = CASE_DIR / SAMPLE_STL_NAME
STL_DIR.mkdir(parents=True, exist_ok=True)


# ===========================================================================
# Geometry helpers
# ===========================================================================

def rounded_rect(w: float, h: float, r: float,
                 cx: float = 0.0, cy: float = 0.0,
                 n: int = 24) -> Polygon:
    """Axis-aligned rounded rectangle centered on (cx, cy)."""
    r = max(0.01, min(r, min(w, h) / 2 - 0.01))
    x0, x1 = cx - w / 2, cx + w / 2
    y0, y1 = cy - h / 2, cy + h / 2
    centers = [(x1 - r, y0 + r), (x1 - r, y1 - r),
               (x0 + r, y1 - r), (x0 + r, y0 + r)]
    arcs = [(-math.pi / 2, 0.0),
            (0.0, math.pi / 2),
            (math.pi / 2, math.pi),
            (math.pi, 3 * math.pi / 2)]
    pts = []
    for (ccx, ccy), (a0, a1) in zip(centers, arcs):
        for i in range(n + 1):
            t = a0 + (a1 - a0) * (i / n)
            pts.append((ccx + r * math.cos(t), ccy + r * math.sin(t)))
    return Polygon(pts)


def mascot_silhouette() -> Polygon:
    """Union of torso + legs + arms, returned in the build XY plane."""
    torso = rounded_rect(BODY_W, TORSO_H, CORNER_R, cy=TORSO_CY)

    pieces: list[Polygon] = [torso]

    # legs (overlap torso bottom by 1 mm for clean union at join)
    leg_cy = -BODY_H / 2 + LEG_H / 2 + 1.0
    leg_total_h = LEG_H + 2.0
    for sign in (-1, 1):
        leg_cx = sign * (LEG_GAP / 2 + LEG_W / 2)
        pieces.append(rounded_rect(LEG_W, leg_total_h, LEG_R,
                                   cx=leg_cx, cy=leg_cy))

    # arms (overlap torso side edge by 1 mm for clean union at join)
    arm_cy = TORSO_CY + ARM_CY_OFFSET
    arm_total_w = ARM_W + 2.0
    for sign in (-1, 1):
        arm_cx = sign * (BODY_W / 2 + ARM_W / 2 - 1.0)
        pieces.append(rounded_rect(arm_total_w, ARM_H, ARM_R,
                                   cx=arm_cx, cy=arm_cy))

    return unary_union(pieces)


def extrude(poly: Polygon, depth: float) -> trimesh.Trimesh:
    """Extrude a shapely polygon along +Z, then center the extrusion on Z=0."""
    m = trimesh.creation.extrude_polygon(poly, depth)
    m.apply_translation([0.0, 0.0, -depth / 2])
    return m


def union(a: trimesh.Trimesh, b: trimesh.Trimesh) -> trimesh.Trimesh:
    return trimesh.boolean.union([a, b], engine="manifold")


def diff(a: trimesh.Trimesh, b: trimesh.Trimesh) -> trimesh.Trimesh:
    return trimesh.boolean.difference([a, b], engine="manifold")


def load_sample_stl() -> trimesh.Trimesh:
    """Load the reference cradle STL and place it in build space.

    The sample is centered in XY, then translated so its original Z=0 face
    sits at build Z = -BODY_D/2 (the back of the mascot), and lifted in Y
    by STL_Y_OFFSET so the display opening reads as the mascot's face.
    """
    m = trimesh.load(SAMPLE_STL)
    if isinstance(m, trimesh.Scene):
        m = trimesh.util.concatenate(list(m.geometry.values()))
    b = m.bounds
    m.apply_translation([
        -(b[0][0] + b[1][0]) / 2,
        -(b[0][1] + b[1][1]) / 2,
        0.0,
    ])
    # STL Z currently 0..BODY_D; shift so it spans -BODY_D/2..+BODY_D/2
    m.apply_translation([0.0, 0.0, -BODY_D / 2])
    # Lift upward so cradle sits in upper torso
    m.apply_translation([0.0, STL_Y_OFFSET, 0.0])
    return m


# ===========================================================================
# Build
# ===========================================================================

def build_case() -> trimesh.Trimesh:
    """Build the single-piece mascot case with embedded sample STL."""
    # 1. outer shell: mascot silhouette extruded through full body depth
    shell = extrude(mascot_silhouette(), BODY_D)

    # 2. cut display window through the shell (sized for Display Pack 2.8
    #    active area + tolerance). The hole extends past both faces to avoid
    #    coplanar booleans with the STL's own faces.
    hole = trimesh.creation.box(
        extents=(DISPLAY_W, DISPLAY_H, BODY_D + 6.0)
    )
    hole.apply_translation([DISPLAY_CX, STL_Y_OFFSET, 0.0])
    shell = diff(shell, hole)

    # 3. union with the embedded sample STL (preserves its internal cradle)
    stl = load_sample_stl()
    combined = union(shell, stl)

    # 4. backward tilt: cut the base with a plane rotated about the
    #    front-bottom edge so the underside becomes an angled wedge that
    #    leans the mascot back TILT_DEG degrees when set on a desk.
    if TILT_DEG > 0.0:
        tilt = math.radians(TILT_DEG)
        cutter = trimesh.creation.box(extents=(BODY_W * 4, 80.0, BODY_D * 4))
        # park the cutter entirely below the body
        cutter.apply_translation([0.0, -BODY_H / 2 - 40.0, 0.0])
        # rotate about the hinge point (front-bottom edge of the body):
        # positive tilt raises the back of the cutter so it shaves a wedge
        # off the back-bottom, angling the underside toward the front
        hinge = [0.0, -BODY_H / 2, BODY_D / 2]
        R = trimesh.transformations.rotation_matrix(tilt, [1, 0, 0], point=hinge)
        cutter.apply_transform(R)
        combined = diff(combined, cutter)

    return combined


def to_export_frame(m: trimesh.Trimesh) -> trimesh.Trimesh:
    """Rotate the build-frame mesh into CAD / slicer convention.

    Build frame: Y=up, Z=mascot-forward. Export frame: Z=up, Y=depth (back).
    That is a +90deg rotation about the X axis.
    """
    R = trimesh.transformations.rotation_matrix(math.pi / 2, [1, 0, 0])
    m.apply_transform(R)
    return m


# ===========================================================================
# Entrypoint
# ===========================================================================

def main() -> None:
    print("building v3 mascot case (wrapping sample display base)...")
    part = build_case()
    part = to_export_frame(part)

    out = STL_DIR / "buddy_case.stl"
    part.export(out)

    ext = part.bounds[1] - part.bounds[0]
    print(f"wrote {out.relative_to(CASE_DIR)}")
    print(f"  bounds (X,Y,Z) = {ext[0]:.1f} x {ext[1]:.1f} x {ext[2]:.1f} mm")
    print(f"  triangles      = {len(part.faces)}")
    print(f"  watertight     = {part.is_watertight}")


if __name__ == "__main__":
    main()
