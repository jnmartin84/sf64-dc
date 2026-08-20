#!/usr/bin/env python3
"""PNG -> N64 IA8 .inc.c converter (splat-compatible output).

Emits one u8 literal per pixel, row-major, 16 per line — the same format as the
extracted src/assets/*.ia8.inc.c files. IA8 layout: intensity in the high
nibble, alpha in the low nibble (what import_texture_ia8 in gfx_retro_dc.c
expects). Intensity is taken from the pixel's luminance (a grayscale or white
glyph PNG converts as-is); both channels truncate 8 bits -> 4.

Usage:
    python3 tools/png2ia8.py input.png > output.ia8.inc.c
"""
import sys
from PIL import Image


def main():
    if len(sys.argv) != 2:
        sys.stderr.write(__doc__)
        return 1
    img = Image.open(sys.argv[1]).convert("RGBA")
    w, h = img.size
    gray = img.convert("L").load()   # luminance for intensity
    px = img.load()                  # alpha from the original
    vals = []
    for y in range(h):
        for x in range(w):
            vals.append(((gray[x, y] >> 4) << 4) | (px[x, y][3] >> 4))
    out = sys.stdout
    for i in range(0, len(vals), 16):
        out.write("".join("0x%02x, " % v for v in vals[i:i + 16]).rstrip(" ") + " \n")
    sys.stderr.write("%s: %dx%d, %d texels\n" % (sys.argv[1], w, h, len(vals)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
