#!/usr/bin/env python3
# title.py -- draws Texas Hold'em's title screen for the TFR/911H and
# writes it as title.h:
#
#   python3 title.py > title.h
#   python3 title.py -a        the screen as ANSI, to look at in a terminal
#
# "TEXAS" and "HOLD'EM" are big letters, pixels two to a character cell
# (half blocks), each row of them a shade lighter at the top, with a
# shadow.  Under them a hand is fanned out, a royal flush, between two
# stacks of chips.  title.c draws all this through screen.c's copy of the
# screen, so the felt (whichever colour it is) shows where nothing is
# drawn, and the welcome screen after it, which keeps the letters, costs
# nothing to send but what changes.
#
# title.h holds three lists of cells: TITLE_SHADOW, the letters' shadow
# alone, which title.c sends first (so that a row of it, and then a row
# of the letters, each need one colour change, not one at every edge of
# a letter); TITLE_LETTERS, the letters with their shadow; and TITLE_FAN.
# Each is a C string of these, the character codes those of glyphs.h:
#
#   1 x y     the next cell is at column x - 1, row y - 1
#   2 c       the foreground is colour c (xterm's 256)
#   3 c       the background is colour c
#   5         the background is the one there already (the felt)
#   4 n ch    n of ch
#   ch        a character (ASCII, or a code from 0x80), in those colours
#
# No byte is a NUL, but the one that ends the string.
#
# @license gpl v. 3, see fujinet/LICENSE for details.

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# ── The characters ─────────────────────────────────────────────────
GLYPH = {}
for line in open(os.path.join(HERE, "glyphs.h"), encoding="utf-8"):
    m = re.match(r"#define G_\w+\s+0x([0-9A-F]{2})\s+// (.)", line)
    if m:
        GLYPH[m.group(2)] = int(m.group(1), 16)


def code(ch):
    if " " <= ch <= "~":
        return ord(ch)
    return GLYPH[ch]


# ── Letters ────────────────────────────────────────────────────────
# Eight pixels high, with strokes two wide.
FONT = {
    "T": """
########
########
...##...
...##...
...##...
...##...
...##...
...##...""",
    "E": """
########
########
##......
######..
######..
##......
########
########""",
    "X": """
##....##
###..###
.######.
..####..
..####..
.######.
###..###
##....##""",
    "A": """
..####..
.######.
###..###
##....##
########
########
##....##
##....##""",
    "S": """
.#######
########
##......
#######.
.#######
......##
########
#######.""",
    "H": """
##....##
##....##
##....##
########
########
##....##
##....##
##....##""",
    "O": """
.######.
########
##....##
##....##
##....##
##....##
########
.######.""",
    "L": """
##......
##......
##......
##......
##......
##......
########
########""",
    "D": """
######..
#######.
##...###
##....##
##....##
##...###
#######.
######..""",
    "'": """
.##
.##
##.
...
...
...
...
...""",
    "M": """
##......##
###....###
####..####
##.####.##
##..##..##
##......##
##......##
##......##""",
}

GAP = 2                 # pixels between letters
SHADOW = 233            # under and to the right of the letters

# A colour a row of cells, lighter at the top: gold for TEXAS, then
# white for HOLD'EM.
WORDS = [("TEXAS", 2, [229, 221, 214, 208]),
         ("HOLD'EM", 7, [231, 255, 252, 249])]


def word_pixels(word):
    glyphs = [FONT[c].strip("\n").split("\n") for c in word]
    width = sum(len(g[0]) for g in glyphs) + GAP * (len(glyphs) - 1)
    rows = [[False] * width for _ in range(8)]
    x = 0
    for g in glyphs:
        for y, line in enumerate(g):
            for i, p in enumerate(line):
                if p == "#":
                    rows[y][x + i] = True
        x += len(g[0]) + GAP
    return rows, width


def letters(shadow_only=False):
    """The cells of the two words: with their shadow, or the shadow alone."""
    cells = {}
    for word, row, shades in WORDS:
        px, width = word_pixels(word)
        x0 = (80 - width) // 2
        # The pixels, a colour each: the letters, and their shadow a pixel
        # down and to the right, where there is no letter.
        letter = {}
        for y in range(8):
            for x in range(width):
                if px[y][x]:
                    letter[(x, y)] = shades[y // 2]
        pix = {} if shadow_only else dict(letter)
        for (x, y) in letter:
            if (x + 1, y + 1) not in letter:
                pix[(x + 1, y + 1)] = SHADOW
        for cy in range(5):
            for x in range(width + 1):
                top = pix.get((x, cy * 2))
                bot = pix.get((x, cy * 2 + 1))
                if top is None and bot is None:
                    continue
                cells[(x0 + x, row + cy)] = half(top, bot)
    return cells


def half(top, bot):
    """A cell of two pixels: (character, foreground, background)."""
    if top is not None and top == bot:
        return ("█", top, None)
    if bot is None:
        return ("▀", top, None)
    if top is None:
        return ("▄", bot, None)
    return ("▀", top, bot)


# ── The fan ────────────────────────────────────────────────────────
WHITE, RED = 231, 160


def card(cells, x, y, rank):
    """A card 7 by 5, the ace to ten of hearts, its border red too."""
    rows = ["╭─────╮",
            "│%-5s│" % (rank + "♥"),
            "│  ♥  │",
            "│%5s│" % ("♥" + rank),
            "╰─────╯"]
    for dy, line in enumerate(rows):
        for dx, ch in enumerate(line):
            cells[(x + dx, y + dy)] = (ch, RED, WHITE)


# Chips, a stack of them, a row each: its colour, with the white marks
# on its edge.
CHIPS_L = [160, 25, 160, 238, 160]
CHIPS_R = [238, 25, 238, 160, 25]


def stack(cells, x, y, colours):
    for i, c in enumerate(colours):
        for dx in range(7):
            cells[(x + dx, y + i)] = ("│", WHITE, c) if dx in (1, 5) else (" ", None, c)


def fan():
    cells = {}
    stack(cells, 15, 14, CHIPS_L)
    stack(cells, 58, 14, CHIPS_R)
    ranks = ["10", "J", "Q", "K", "A"]
    lift = [2, 1, 0, 1, 2]
    x0 = (80 - (4 * 4 + 7)) // 2
    for i, r in enumerate(ranks):
        card(cells, x0 + 4 * i, 12 + lift[i], r)
    return cells


# ── Output ─────────────────────────────────────────────────────────
def encode(cells):
    out = []
    cx = cy = None
    fg = bg = "?"
    items = sorted(cells.items(), key=lambda kv: (kv[0][1], kv[0][0]))
    i = 0
    while i < len(items):
        (x, y), (ch, f, b) = items[i]
        n = 1
        while (i + n < len(items) and items[i + n][1] == (ch, f, b)
               and items[i + n][0] == (x + n, y)):
            n += 1
        if (cx, cy) != (x, y):
            out += [1, x + 1, y + 1]
        if ch != " " and f != fg:
            out += [2, f]
            fg = f
        if b != bg:
            out += [5] if b is None else [3, b]
            bg = b
        if n >= 4:
            out += [4, n, code(ch)]
        else:
            n = 1
            out.append(code(ch))
        cx, cy = x + n, y
        i += n
    assert 0 not in out
    return out


def c_string(data, hexed=False):
    """data as a C string literal, and whether it ends in a hex escape.
    CMOC joins "\x81" "10" into "\x8110" before it reads the escapes, so
    a hex digit after a hex escape is written as an escape itself."""
    out = []
    for byte in data:
        c = chr(byte)
        if byte in (0x22, 0x5C):
            out.append("\\" + c)
            hexed = False
        elif 0x20 <= byte < 0x7F and not (hexed and c in "0123456789abcdefABCDEF"):
            out.append(c)
            hexed = False
        else:
            out.append("\\x%02X" % byte)
            hexed = True
    return '"' + "".join(out) + '"', hexed


def c_macro(name, data):
    lines, hexed = [], False
    for i in range(0, len(data), 24):
        s, hexed = c_string(data[i:i + 24], hexed)
        lines.append("    " + s)
    return "#define %s \\\n%s\n" % (name, " \\\n".join(lines))


def ansi(cells, felt=28):
    s = "\033[0m\033[48;5;%dm\033[2J" % felt
    for (x, y), (ch, f, b) in sorted(cells.items(), key=lambda kv: (kv[0][1], kv[0][0])):
        s += "\033[%d;%dH\033[38;5;%dm\033[48;5;%dm%s" % (y + 1, x + 1, f or 0, b or felt, ch)
    return s + "\033[0m\033[24;1H\n"


def main():
    let, shade, fn = letters(), letters(True), fan()
    if sys.argv[1:] == ["-a"]:
        both = dict(let)
        both.update(fn)
        sys.stdout.write(ansi(both))
        return
    a, sh, b = encode(let), encode(shade), encode(fn)
    print("// title.h -- made by title.py; do not edit.")
    print("//")
    print("// @license gpl v. 3, see fujinet/LICENSE for details.\n")
    print("// The letters' shadow alone, %d cells in %d bytes: sent first, it" % (len(shade), len(sh)))
    print("// needs a colour change a row, not one at every edge of a letter.")
    print(c_macro("TITLE_SHADOW", sh))
    print("// The big letters, %d cells in %d bytes" % (len(let), len(a)))
    print(c_macro("TITLE_LETTERS", a))
    print("// The cards and chips, %d cells in %d bytes" % (len(fn), len(b)))
    print(c_macro("TITLE_FAN", b))


if __name__ == "__main__":
    main()
