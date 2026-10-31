#!/usr/bin/env python3
# title.py -- the title's logo for the TFR/911H, from the Atari's art
# (src/atari/title4.atascii), and writes it as title.h:
#
#   python3 title.py ../atari/title4.atascii > title.h
#   python3 title.py -a ../atari/title4.atascii    the logo as ANSI, to look at
#
# The art draws block letters as outlines: box-drawing lines round strokes
# of spaces.  As in 80 columns on MS-DOS, each column is doubled: a line
# with an arm to the right goes on with a line, and the hyphen's half
# blocks with half blocks.  The outlines are drawn in neon, a colour a
# row, cyan at the top to pink at the foot.  The strokes, the spaces that
# a flood from the edge does not reach, are where display.c puts the TV
# static, a run of them at a time: title.h lists the runs.
#
# @license gpl v. 3, see fujinet/LICENSE for details.

import sys

ATASCII = {0x20: " ", 0x7C: "│", 0x12: "─", 0x01: "├", 0x04: "┤",
           0x11: "┌", 0x05: "┐", 0x1A: "└", 0x03: "┘", 0x17: "┬",
           0x18: "┴", 0x13: "┼", 0x0B: "▀", 0x0C: "▀"}
ARMS = "─├┌└┬┴┼"    # a line goes on to the right
HALF = "▀"

# A colour a row (xterm's 256): cyan, through blue and violet, to pink.
NEON = [51, 45, 39, 33, 69, 63, 99, 135, 171, 207, 206, 205, 204, 198]


def load(path):
    lines = open(path, "rb").read().split(b"\x9b")
    while lines and not lines[-1].strip():
        lines.pop()
    rows = []
    for l in lines:
        l = l.ljust(40, b" ")[:40]
        bad = [b for b in l if b not in ATASCII]
        if bad:
            sys.exit("%s: no box character for %s" % (path, ", ".join("0x%02X" % b for b in bad)))
        rows.append([ATASCII[b] for b in l])
    return rows


def doubled(rows):
    out = []
    for r in rows:
        o = []
        for c, v in enumerate(r):
            o.append(v)
            nxt = r[c + 1] if c + 1 < len(r) else " "
            o.append("─" if v in ARMS else HALF if v == HALF or nxt == HALF else " ")
        out.append(o)
    return out


CLOSING = set(" ─┴└┘")     # a row of these closes a line of letters


def strokes(art):
    """The spaces a flood from the edge of their line of letters does not
    reach.  Each line (SYN-ACK, WIRE) is flooded on its own: the top of WIRE
    would otherwise close the gaps between the letters above it."""
    h, w = len(art), len(art[0])
    mask = []
    top = 0
    while top < h:
        bottom = top
        while bottom < h - 1 and not set(art[bottom]) <= CLOSING:
            bottom += 1
        outside = set()
        todo = [(r, c) for r in range(top, bottom + 1) for c in (0, w - 1)] + \
               [(r, c) for r in (top, bottom) for c in range(w)]
        while todo:
            r, c = todo.pop()
            if not (top <= r <= bottom and 0 <= c < w) or (r, c) in outside or art[r][c] != " ":
                continue
            outside.add((r, c))
            todo += [(r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)]
        for r in range(top, bottom + 1):
            mask.append([art[r][c] == " " and (r, c) not in outside for c in range(w)])
        top = bottom + 1
    return mask


def runs(mask):
    out = []
    for r, row in enumerate(mask):
        c = 0
        while c < len(row):
            if row[c]:
                k = c
                while k < len(row) and row[k]:
                    k += 1
                out.append((r, c, k - c))
                c = k
            else:
                c += 1
    return out


def ansi_rows(art):
    rows = []
    for r, row in enumerate(art):
        text = "".join(row).rstrip()
        rows.append("\033[38;5;%dm" % NEON[r % len(NEON)] + text)
    return rows


def c_string(s):
    out, hexed = [], False
    for byte in s.encode("utf-8"):
        c = chr(byte)
        if byte in (0x22, 0x5C):
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
    args = sys.argv[1:]
    look = args[:1] == ["-a"]
    art = doubled(load(args[-1]))
    mask = strokes(art)
    if look:
        sys.stdout.write("\033[2J\033[H" + "\r\n".join(ansi_rows(art)) + "\033[0m\r\n")
        for row in mask:
            print("".join("#" if m else "." for m in row))
        return
    rs = runs(mask)
    rows = ansi_rows(art)
    print("// title.h -- made by title.py from ../atari/title4.atascii; do not edit.\n")
    print("#define TITLE_ROWS %d" % len(art))
    print("#define TITLE_COLS %d\n" % len(art[0]))
    print("// The outlines, a row at a time from the left edge, each in its colour,")
    print("// the rows joined by CR LF: TITLE_LEN bytes.")
    body = rows[0] + "".join("\r\n" + r for r in rows[1:])
    print("#define TITLE_LEN %d" % len(body.encode("utf-8")))
    print("#define TITLE_ART \\")
    print(" \\\n".join("    " + c_string(r if i == 0 else "\r\n" + r) for i, r in enumerate(rows)))
    print()
    print("// The letters' strokes: %d runs of spaces, each its row, column and" % len(rs))
    print("// length, for the static.")
    print("#define TITLE_RUNS %d" % len(rs))
    print("static const unsigned char title_runs[%d] = {" % (3 * len(rs)))
    for i in range(0, len(rs), 6):
        print("    " + " ".join("%d, %d, %d," % x for x in rs[i:i + 6]))
    print("};")


if __name__ == "__main__":
    main()
