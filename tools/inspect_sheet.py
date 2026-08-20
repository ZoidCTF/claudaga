"""Re-derive the sprite coordinates in src/atlas.c straight from the sheet.

The atlas is hand-written C rather than generated, so this exists to check it:
it finds every block of art on the sheet and prints where each one sits. Run it
after replacing assets/galaga_sheet.png to see whether the layout still holds.

    python tools/inspect_sheet.py

Requires Pillow. Palette index 0 is the grey gutter between blocks and index 1
is the black that stands in for transparency inside a cell.
"""

import sys
from PIL import Image

SHEET = "assets/galaga_sheet.png"
GUTTER = 0          # palette index of the grey background between blocks
TRANSPARENT = 1     # palette index of the black treated as see-through


def blocks(px, w, h):
    """Every connected run of non-gutter pixels, as (x, y, w, h)."""
    seen = [[False] * w for _ in range(h)]
    out = []
    for y0 in range(h):
        for x0 in range(w):
            if px[x0, y0] == GUTTER or seen[y0][x0]:
                continue
            stack = [(x0, y0)]
            seen[y0][x0] = True
            minx = maxx = x0
            miny = maxy = y0
            while stack:
                x, y = stack.pop()
                minx, maxx = min(minx, x), max(maxx, x)
                miny, maxy = min(miny, y), max(maxy, y)
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx] \
                            and px[nx, ny] != GUTTER:
                        seen[ny][nx] = True
                        stack.append((nx, ny))
            out.append((minx, miny, maxx - minx + 1, maxy - miny + 1))
    return sorted(out, key=lambda b: (b[1], b[0]))


def main():
    im = Image.open(SHEET)
    if im.mode != "P":
        sys.exit("expected a palette PNG, got mode %s" % im.mode)
    w, h = im.size
    px = im.load()
    print("%s  %dx%d" % (SHEET, w, h))

    # The 16x16 art sits on a 1px border with an 18px pitch. Anything matching
    # that is reported as a grid cell so it can be compared with GX/GY in
    # src/atlas.c; everything else is printed raw.
    for (x, y, bw, bh) in blocks(px, w, h):
        if bw < 4 or bh < 4:
            continue    # credit text and palette swatches, not sprites
        note = ""
        if bw == 16 and bh == 16 and (x - 1) % 18 == 0 and (y - 1) % 18 == 0:
            note = "  grid col %d row %d" % ((x - 1) // 18, (y - 1) // 18)
        print("  x=%3d y=%3d w=%3d h=%3d%s" % (x, y, bw, bh, note))


if __name__ == "__main__":
    main()
