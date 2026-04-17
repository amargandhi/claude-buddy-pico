"""Combined review render for v3: the single-piece mascot case shown from
multiple angles next to the sample STL it wraps. Used as the handoff image
for design review with the user.
"""
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import trimesh
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

THEME_ORANGE = "#d97a4a"
REFERENCE_GRAY = "#777777"
SHADOW_EDGE = "#5a3420"

HERE = Path(__file__).resolve().parent
CASE_DIR = HERE.parent


def _place(ax, mesh, color, elev, azim, title):
    verts = mesh.vertices[mesh.faces]
    pc = Poly3DCollection(verts, alpha=0.95, linewidth=0.15, edgecolor=SHADOW_EDGE)
    pc.set_facecolor(color)
    ax.add_collection3d(pc)
    b = mesh.bounds
    span = max(b[1] - b[0]) * 0.6
    c = (b[0] + b[1]) / 2
    ax.set_xlim(c[0] - span, c[0] + span)
    ax.set_ylim(c[1] - span, c[1] + span)
    ax.set_zlim(c[2] - span, c[2] + span)
    ax.set_box_aspect(tuple(float(v) for v in (b[1] - b[0])))
    ax.view_init(elev=elev, azim=azim)
    ax.set_title(title, fontsize=10)
    ax.set_axis_off()


def load_centered(path):
    m = trimesh.load(path)
    if isinstance(m, trimesh.Scene):
        m = trimesh.util.concatenate(list(m.geometry.values()))
    m.apply_translation(-m.centroid)
    return m


def main():
    case = load_centered(CASE_DIR / "stl" / "buddy_case.stl")
    sample = load_centered(CASE_DIR / "Sample display base.stl")

    fig = plt.figure(figsize=(14, 10), dpi=120)

    # Views assume the exported CAD convention X=width, Y=depth, Z=up.
    _place(fig.add_subplot(2, 3, 1, projection="3d"), case, THEME_ORANGE,
           0, -90, "Case - Front (mascot face)")
    _place(fig.add_subplot(2, 3, 2, projection="3d"), case, THEME_ORANGE,
           0, 0, "Case - Side")
    _place(fig.add_subplot(2, 3, 3, projection="3d"), case, THEME_ORANGE,
           25, -60, "Case - Iso")
    _place(fig.add_subplot(2, 3, 4, projection="3d"), case, THEME_ORANGE,
           90, -90, "Case - Top (shows tilt)")
    _place(fig.add_subplot(2, 3, 5, projection="3d"), case, THEME_ORANGE,
           0, 90, "Case - Back")
    _place(fig.add_subplot(2, 3, 6, projection="3d"), sample, REFERENCE_GRAY,
           25, -60, "Sample display base (reference)")

    cbb = case.bounds
    ce = cbb[1] - cbb[0]
    sbb = sample.bounds
    se = sbb[1] - sbb[0]
    fig.suptitle(
        "Claude Buddy Pico - Case v3 (review): mascot shell wraps the "
        "existing display base STL as internal frame\n"
        f"Case envelope: {ce[0]:.1f} x {ce[1]:.1f} x {ce[2]:.1f} mm   |   "
        f"Embedded sample: {se[0]:.1f} x {se[1]:.1f} x {se[2]:.1f} mm",
        fontsize=12,
    )
    fig.tight_layout()
    out = CASE_DIR / "renders" / "v3_review.png"
    plt.savefig(out, bbox_inches="tight")
    print("saved ->", out)


if __name__ == "__main__":
    main()
