#!/usr/bin/env python3
"""PNG -> N64 RGBA5551 .inc.c converter (splat-compatible output).

Emits one u16 literal per pixel, row-major, 8 per line — the same format as the
extracted src/assets/*.rgba16.inc.c files. The value layout is N64 RGBA5551
(R in bits 15-11, G 10-6, B 5-1, A in bit 0), which is exactly what
import_texture_rgba16 in gfx_retro_dc.c expects to read as a native u16;
byte order inside the u16 is the compiler's problem, so no swapping here.

Usage:
    python3 tools/png2rgba16.py input.png > output.rgba16.inc.c

Alpha: a source pixel with alpha >= 128 is opaque (A=1), else transparent (A=0).
"""
import sys
from PIL import Image


def main():
    if len(sys.argv) != 2:
        sys.stderr.write(__doc__)
        return 1
    img = Image.open(sys.argv[1]).convert("RGBA")
    w, h = img.size
    px = img.load()
    vals = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            vals.append(((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (1 if a >= 128 else 0))
    out = sys.stdout
    for i in range(0, len(vals), 8):
        out.write("".join("0x%04x, " % v for v in vals[i:i + 8]).rstrip(" ") + " \n")
    sys.stderr.write("%s: %dx%d, %d texels\n" % (sys.argv[1], w, h, len(vals)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
