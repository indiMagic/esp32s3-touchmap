#!/usr/bin/env python3
"""Convert sim/out/*.ppm to PNG, and build a contact sheet of the maneuver arrows."""
import glob
import os
import sys

from PIL import Image

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out")
for ppm in sorted(glob.glob(os.path.join(out, "*.ppm"))):
    Image.open(ppm).save(ppm[:-4] + ".png")

arrows = sorted(glob.glob(os.path.join(out, "arrow_*.ppm")))
if arrows:
    # Crop each banner's 56x56 arrow area and tile them 7 per row.
    tiles = [Image.open(a).crop((4, 4, 68, 60)) for a in arrows]
    cols = 7
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * 72, rows * 64), (16, 24, 44))
    for i, t in enumerate(tiles):
        sheet.paste(t, ((i % cols) * 72 + 4, (i // cols) * 64 + 4))
    sheet = sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST)
    sheet.save(os.path.join(out, "arrows_sheet.png"))
print("png written to", out)
