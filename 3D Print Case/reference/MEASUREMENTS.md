# Reference dimensions and mascot ratios

## Hardware envelope (from Sample display base.stl)

- Outer sleeve: 79.8 x 55.2 x 23.0 mm
- Display Pack 2.8 active area: approx 58 x 43.5 mm (2.8" IPS, 320x240 landscape)
- Pico footprint: 51 x 21 mm
- Pico + Display Pack stack depth with LiPo SHIM: approx 18-20 mm
- LiPo (BAT0005) 2000mAh: approx 60 x 36 x 6.5 mm

## Mascot silhouette ratios (v0 working assumption)

Source: pixel art master the user provided in-chat. Claude Code mascot is known to be a rounded rectangle body with two rectangular eyes near the top, small leg notches at the bottom.

- Body aspect ratio (W:H): 1 : 1.06 (barely taller than wide)
- Corner radius / body width: 0.20
- Eye row position from top: approx 0.30 (upper third)
- Display placement (face replaces eyes on our build): centered horizontally, offset upward by ~8% of body height

Because the display IS the face on this build, the eyes on the pixel master are not modelled as geometry - the firmware's pet renderer draws them on screen.

## Printability constants

- Wall thickness: 2.0 mm (1.6 mm minimum)
- Hole-to-shaft clearance: 0.3 mm
- Chamfer bottom-facing overhangs: 45 degrees
- Nominal layer height: 0.2 mm
- ABS shrink compensation: built into the P1S profile; no manual offset in the model

## Print material

- Bambu Lab ABS, theme color: "Terracotta" / "Orange" (match Claude Code mascot hue).
- Nozzle: 0.4 mm.
- Chamber: enclosed (P1S).
