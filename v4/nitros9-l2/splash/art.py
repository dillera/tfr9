#!/usr/bin/env python3
# art.py -- draws the picture at the top of splash's screen and writes it
# as art.h:  python3 art.py > art.h   (art.py -a writes the ANSI itself,
# to look at in a terminal).
#
# The picture is pixels, two to a character cell, one above the other:
# half blocks in UTF-8, with xterm's 256 colours.  "TFR" is big, in the
# warm colours of the manual's cover, with a brown shadow; the band of
# five stripes from the cover runs from under it, turns down, and turns
# again to run under "FujiNet".  splash writes the rest of the screen
# into the spaces this leaves (see its LAYOUT).
#
# @license gpl v. 3, see fujinet/LICENSE for details.

import math
import sys

W = 80                  # columns
H = 34                  # pixel rows: 17 character rows

# ── Letters ────────────────────────────────────────────────────────
# 14 pixels high, condensed and heavy, after Futura Condensed ExtraBold.
BIG = {
    "T": """
#########
#########
#########
..#####..
..#####..
..#####..
..#####..
..#####..
..#####..
..#####..
..#####..
..#####..
..#####..
..#####..""",
    "F": """
########
########
########
#####...
#####...
#######.
#######.
#######.
#####...
#####...
#####...
#####...
#####...
#####...""",
    "R": """
########..
#########.
##########
#####..###
#####..###
#####..###
##########
#########.
########..
#####.###.
#####.####
#####..###
#####..###
#####..###""",
}

# 10 pixels high: capitals of 8, and 2 below the line for the j.
MED = {
    "F": """
######
######
##....
#####.
#####.
##....
##....
##....
......
......""",
    "u": """
......
......
##..##
##..##
##..##
##..##
######
.#####
......
......""",
    "j": """
..##
....
..##
..##
..##
..##
..##
..##
..##
###.""",
    "i": """
##
..
##
##
##
##
##
##
..
..""",
    "N": """
##....##
###...##
####..##
##.##.##
##..####
##...###
##....##
##....##
........
........""",
    "e": """
......
......
.####.
##..##
######
##....
##...#
.#####
......
......""",
    "t": """
.##.
.##.
####
.##.
.##.
.##.
.##.
..##
....
....""",
}

# ── Colours (xterm's 256) ──────────────────────────────────────────
STRIPES = [94, 166, 172, 214, 222]  # the cover's brown, rust, orange, gold, yellow
TFR_RAMP = [229, 229, 222, 222, 221, 221, 214, 214, 208, 208, 172, 166, 166, 130]
SHADOW = 94
FUJI_RAMP = [123, 87, 81, 45, 45, 39, 33, 33, 27, 27]

# ── Where things go (in pixels) ────────────────────────────────────
TFR_X, TFR_Y = 2, 2
BAND_Y1 = 19            # the band's centre line under TFR,
BAND_XA = 24            # where it starts to turn down,
BAND_R = 6              # the radius of its turns,
BAND_Y2 = BAND_Y1 + 2 * BAND_R  # and its centre line under FujiNet
FUJI_Y = BAND_Y2 - 12


def glyph(font, ch):
    return font[ch].strip("\n").split("\n")


def width(font, s, gap=1):
    return sum(len(glyph(font, c)[0]) for c in s) + gap * (len(s) - 1)


class Canvas:
    def __init__(self):
        self.px = [[None] * W for _ in range(H)]

    def put(self, x, y, c, under=False):
        if 0 <= x < W and 0 <= y < H and not (under and self.px[y][x] is not None):
            self.px[y][x] = c

    def text(self, font, s, x, y, ramp, under=False):
        for ch in s:
            g = glyph(font, ch)
            for r, row in enumerate(g):
                for k, v in enumerate(row):
                    if v == "#":
                        self.put(x + k, y + r, ramp[r] if isinstance(ramp, list) else ramp, under)
            x += len(g[0]) + 1


def centre_line():
    """The band's centre line, as points with unit tangents: east along
    BAND_Y1, a quarter turn down, south, a quarter turn east, and east
    along BAND_Y2, as on the cover."""
    pts, step = [], 0.05
    xa, r, y1, y2 = BAND_XA, BAND_R, BAND_Y1, BAND_Y2
    x = -3.0
    while x < xa:
        pts.append((x, y1, 1.0, 0.0))
        x += step
    n = int(r * math.pi / 2 / step) + 1
    for i in range(n + 1):                  # about (xa, y1 + r)
        a = -math.pi / 2 + math.pi / 2 * i / n
        pts.append((xa + r * math.cos(a), y1 + r + r * math.sin(a), -math.sin(a), math.cos(a)))
    y = y1 + r
    while y < y2 - r:
        pts.append((xa + r, y, 0.0, 1.0))
        y += step
    for i in range(n + 1):                  # about (xa + 2r, y2 - r)
        a = math.pi - math.pi / 2 * i / n
        pts.append((xa + 2 * r + r * math.cos(a), y2 - r + r * math.sin(a), math.sin(a), -math.cos(a)))
    x = xa + 2 * r
    while x < W + 3:
        pts.append((x, y2, 1.0, 0.0))
        x += step
    return pts


def band(cv):
    """Each pixel within two and a half of the centre line takes the stripe
    of its distance to the left of it (brown is leftmost)."""
    n = len(STRIPES)
    pts = centre_line()
    for y in range(H):
        for x in range(W):
            px, py = x + 0.5, y + 0.5
            near = [p for p in pts if abs(p[0] - px) <= n and abs(p[1] - py) <= n]
            if not near:
                continue
            qx, qy, tx, ty = min(near, key=lambda p: (p[0] - px) ** 2 + (p[1] - py) ** 2)
            if math.hypot(qx - px, qy - py) > n / 2 + 0.01:
                continue
            left = ty * (px - qx) - tx * (py - qy)
            i = int(math.floor(n / 2 - left))
            if 0 <= i < n:
                cv.put(x, y, STRIPES[i])


def picture():
    cv = Canvas()
    cv.text(BIG, "TFR", TFR_X, TFR_Y, TFR_RAMP)
    cv.text(BIG, "TFR", TFR_X + 1, TFR_Y + 1, SHADOW, under=True)
    cv.text(MED, "FujiNet", W - 2 - width(MED, "FujiNet"), FUJI_Y, FUJI_RAMP)
    band(cv)
    return cv


# ── As characters ──────────────────────────────────────────────────
UPPER, LOWER, FULL = "▀", "▄", "█"


def ansi(cv):
    """Rows of characters and colour changes.  The screen has just been
    cleared, so a run of blanks is a cursor move."""
    rows = []
    for y in range(0, H, 2):
        fg = bg = None
        out = []

        def sgr(f, b):
            nonlocal fg, bg
            s = []
            if f != fg:
                s.append("38;5;%d" % f if f is not None else "39")
            if b != bg:
                s.append("48;5;%d" % b if b is not None else "49")
            fg, bg = f, b
            return "\033[" + ";".join(s) + "m" if s else ""

        top, bot = cv.px[y], cv.px[y + 1]
        x, last = 0, max([k for k in range(W) if top[k] or bot[k]] + [-1])
        while x <= last:
            t, b = top[x], bot[x]
            if t is None and b is None:
                run = 1
                while x + run <= last and top[x + run] is None and bot[x + run] is None:
                    run += 1
                if run >= 4:
                    out.append("\033[%dC" % run)
                else:
                    out.append(sgr(fg, None) + " " * run)
                x += run
                continue
            if t == b:
                out.append(sgr(t, bg) + FULL)
            elif b is None:
                out.append(sgr(t, None) + UPPER)
            elif t is None:
                out.append(sgr(b, None) + LOWER)
            elif (fg, bg) == (b, t):
                out.append(LOWER)
            else:
                out.append(sgr(t, b) + UPPER)
            x += 1
        if fg is not None or bg is not None:
            out.append("\033[0m")
        rows.append("".join(out))
    return rows


def c_string(s):
    """s as a C string literal, in hex escapes where it must be (CMOC takes
    no octal above \\177), split where a hex digit follows one."""
    out, hexed = [], False
    for byte in s.encode("utf-8"):
        c = chr(byte)
        if byte == 0x22 or byte == 0x5C:
            out.append("\\" + c)
        elif 0x20 <= byte < 0x7F:
            out.append(('" "' if hexed and c in "0123456789abcdefABCDEF" else "") + c)
        else:
            out.append("\\x%02X" % byte)
            hexed = True
            continue
        hexed = False
    return '"' + "".join(out) + '"'


def main():
    rows = ansi(picture())
    head = "\033[0m\033[H\033[2J"
    if sys.argv[1:] == ["-a"]:
        sys.stdout.write(head + "\r\n".join(rows) + "\r\n")
        return
    body = [head + rows[0]] + ["\r\n" + r for r in rows[1:]]
    # A macro, used once, rather than an array: CMOC would keep a copy of
    # the literal as well as the array.
    print("// art.h -- made by art.py; do not edit.\n")
    print("#define ART_ROWS %d\n" % len(rows))
    print("// The screen cleared, then the picture, ART_LEN bytes, leaving the")
    print("// cursor at the end of its last row.")
    print("#define ART_LEN %d" % len("".join(body).encode()))
    print("#define ART \\")
    print(" \\\n".join("    " + c_string(line) for line in body))


if __name__ == "__main__":
    main()
