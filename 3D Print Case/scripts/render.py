"""Render 2x2 four-view previews of STL files for visual review.

Views: front, side, back-iso, top-iso. Used as the feedback loop in Claude's
3D design iteration - Claude reads the output PNG, self-reviews against the
printability checklist, and decides whether to iterate before asking the user.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
import trimesh
from mpl_toolkits.mplot3d.art3d import Poly3DCollection


# Claude Code mascot theme orange for the preview color.
THEME_ORANGE = "#d97a4a"
SHADOW_EDGE = "#5a3420"


def _render_view(mesh: trimesh.Trimesh, elev: float, azim: float, ax, title: str) -> None:
    verts = mesh.vertices[mesh.faces]
    pc = Poly3DCollection(verts, alpha=0.95, linewidth=0.15, edgecolor=SHADOW_EDGE)
    pc.set_facecolor(THEME_ORANGE)
    ax.add_collection3d(pc)

    b = mesh.bounds
    span = max(b[1] - b[0]) * 0.6
    c = (b[0] + b[1]) / 2
    ax.set_xlim(c[0] - span, c[0] + span)
    ax.set_ylim(c[1] - span, c[1] + span)
    ax.set_zlim(c[2] - span, c[2] + span)
    ax.set_box_aspect([1, 1, 1])
    ax.view_init(elev=elev, azim=azim)
    ax.set_title(title, fontsize=10)
    ax.set_axis_off()


def four_view(stl_path: Path, out_png: Path, heading: str | None = None) -> Path:
    mesh = trimesh.load(stl_path)
    if isinstance(mesh, trimesh.Scene):
        mesh = trimesh.util.concatenate(list(mesh.geometry.values()))
    mesh.apply_translation(-mesh.centroid)

    fig = plt.figure(figsize=(10, 10), dpi=120)

    # Views assume CAD convention: X=width, Y=depth (+Y=back), Z=height (up).
    # Front: camera on -Y axis looking toward +Y, seeing XZ plane = the mascot face.
    _render_view(mesh, 0, -90, fig.add_subplot(2, 2, 1, projection="3d"), "Front (mascot face)")
    _render_view(mesh, 0, 0, fig.add_subplot(2, 2, 2, projection="3d"), "Side profile")
    _render_view(mesh, 0, 90, fig.add_subplot(2, 2, 3, projection="3d"), "Back (hatch side)")
    _render_view(mesh, 25, -60, fig.add_subplot(2, 2, 4, projection="3d"), "Iso")

    b = mesh.bounds
    ext = b[1] - b[0]
    if heading is None:
        heading = stl_path.name
    fig.suptitle(
        f"{heading}   {ext[0]:.1f} x {ext[1]:.1f} x {ext[2]:.1f} mm   {len(mesh.faces)} tris",
        fontsize=12,
    )
    fig.tight_layout()
    out_png.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(out_png, bbox_inches="tight")
    plt.close(fig)
    return out_png


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("stl", nargs="?")
    parser.add_argument("--all", action="store_true", help="render every stl in ../stl")
    args = parser.parse_args()

    here = Path(__file__).resolve().parent
    stl_dir = here.parent / "stl"
    render_dir = here.parent / "renders"

    if args.all or args.stl is None:
        for stl in sorted(stl_dir.glob("*.stl")):
            out = render_dir / f"{stl.stem}.png"
            four_view(stl, out)
            print(f"rendered {stl.name} -> {out.relative_to(here.parent)}")
    else:
        stl = Path(args.stl)
        out = render_dir / f"{stl.stem}.png"
        four_view(stl, out)
        print(f"rendered {stl.name} -> {out.relative_to(here.parent)}")


if __name__ == "__main__":
    main()
