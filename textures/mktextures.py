#!/usr/bin/env python3
"""
Build the cabinet textures from a photo of the author's Olympia 405 W:

  python3 textures/mktextures.py photo.png

Crops three veneer areas (no brand lettering), evens out the lighting
(divides by a strongly blurred copy), stretches contrast and colour a
little, upscales 2x and writes 24-bit BMPs
that roehre loads with SDL_LoadBMP. Coordinates refer to the 1600x1200
original photo.
"""
import sys, os
import numpy as np
from PIL import Image, ImageFilter, ImageEnhance

REGIONS = {
    # name: (x0, y0, x1, y1)
    "frame.bmp": (700, 342, 770, 510),   # light front frame, vertical grain
    "front.bmp": (305, 333, 668, 371),   # dark strip above the cloth, horizontal grain
    "panel.bmp": (372, 696, 494, 774),   # figured lower panel, left of the lettering
}

def even_light(img, radius):
    a = np.asarray(img, dtype=np.float64)
    blur = np.asarray(img.filter(ImageFilter.GaussianBlur(radius)), dtype=np.float64)
    mean = a.reshape(-1, 3).mean(axis=0)
    out = a / np.maximum(blur, 1) * mean
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8))

def main():
    src = Image.open(sys.argv[1]).convert("RGB")
    here = os.path.dirname(os.path.realpath(__file__))
    for name, box in REGIONS.items():
        tile = src.crop(box)
        tile = even_light(tile, max(tile.size) / 6)
        # the photo was taken under dim room light: stretch the contrast a
        # little and bring back some of the warmth of the varnished wood
        tile = ImageEnhance.Contrast(tile).enhance(1.4)
        tile = ImageEnhance.Color(tile).enhance(1.2)
        tile = tile.resize((tile.width * 2, tile.height * 2), Image.LANCZOS)
        tile = tile.filter(ImageFilter.UnsharpMask(radius=2, percent=60, threshold=2))
        tile.save(os.path.join(here, name))
        print(name, tile.size)

if __name__ == "__main__":
    main()
