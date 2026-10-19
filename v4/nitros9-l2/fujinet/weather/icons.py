# icons.py: fnweather's icons as half-block pixel art, made from shapes.
#   python3 icons.py c > icondata.h      the C data, for icons.c
#   python3 icons.py preview             icons.ppm, to look at
# @license gpl v. 3, see fujinet/LICENSE for details.
import sys, math

PAL = {'.': None, 'Y': (255,235,60), 'y': (220,150,20), 'W': (255,255,255),
       'w': (175,175,185), 'B': (90,140,255), 'b': (30,70,200), 'C': (90,240,255),
       'c': (0,170,190), 'R': (255,80,80)}

def canvas(w, h): return [['.'] * w for _ in range(h)]

def disc(c, cx, cy, r, col):
    for y in range(len(c)):
        for x in range(len(c[0])):
            if (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= r * r:
                c[y][x] = col

def rect(c, x0, y0, x1, y1, col):
    for y in range(max(0, y0), min(len(c), y1)):
        for x in range(max(0, x0), min(len(c[0]), x1)):
            c[y][x] = col

def plot(c, x, y, col):
    if 0 <= y < len(c) and 0 <= x < len(c[0]): c[y][x] = col

def sun(c, cx, cy, r, rays=True):
    if rays:
        for k in range(8):
            a = k * math.pi / 4
            for d in (r + 1.6, r + 2.6):
                plot(c, int(cx + d * math.cos(a)), int(cy + d * math.sin(a)), 'y')
    disc(c, cx, cy, r, 'Y')

def cloud(c, x, y, s, front='W', back='w'):
    # bumps: a union of discs over a flat bottom, at scale s
    m = canvas(len(c[0]), len(c))
    disc(m, x + 3.2 * s, y + 4.3 * s, 2.6 * s, 'W')
    disc(m, x + 6.6 * s, y + 3.0 * s, 3.2 * s, 'W')
    disc(m, x + 10.0 * s, y + 4.6 * s, 2.3 * s, 'W')
    rect(m, int(x + 2.6 * s), int(y + 4.3 * s), int(x + 10.6 * s), int(y + 6.9 * s), 'W')
    bottom = max(yy for yy in range(len(m)) for xx in range(len(m[0])) if m[yy][xx] != '.')
    for yy in range(len(m)):
        for xx in range(len(m[0])):
            if m[yy][xx] != '.':
                c[yy][xx] = back if (yy == bottom and back) else front

def drops(c, xs, y0, n, col='B'):
    for x in xs:
        for k in range(n):
            plot(c, x - k // 2, y0 + k, col)

def flakes(c, pts, col='W'):
    for x, y in pts: plot(c, x, y, col)

def bolt(c, pts, col='Y'):
    for x, y in pts: plot(c, x, y, col)

def fog(c, rows, col='w'):
    for y, x0, x1 in rows: rect(c, x0, y, x1, y + 1, col)

def big(name):
    c = canvas(14, 14)
    if name == 'clear':     # by hand, for symmetry
        return [list(r) for r in [
            "......yy......", "..y...yy...y..", "...y......y...", ".....YYYY.....",
            "....YYYYYY....", "...YYYYYYYY...", "yy.YYYYYYYY.yy", "yy.YYYYYYYY.yy",
            "...YYYYYYYY...", "....YYYYYY....", ".....YYYY.....", "...y......y...",
            "..y...yy...y..", "......yy......"]]
    elif name == 'moon':
        disc(c, 6.5, 7, 5, 'W'); disc(c, 9.3, 5.6, 4.4, '.')
        flakes(c, [(11, 9), (12, 3), (2, 1)], 'Y')
    elif name == 'few':
        sun(c, 5, 5, 3.0)
        cloud(c, 4.2, 6.4, 0.75)
    elif name == 'partly':
        sun(c, 5, 4.6, 3.2, rays=True)
        cloud(c, 1.4, 4.6, 1.0)
    elif name == 'cloudy':
        cloud(c, 3.0, 1.0, 0.85, front='w', back=None)
        cloud(c, 0.6, 4.8, 1.0)
    elif name == 'showers':
        sun(c, 9.6, 3.6, 2.6, rays=False)
        cloud(c, 0.2, 1.6, 1.0)
        drops(c, [4, 7, 10], 9, 3)
    elif name == 'rain':
        cloud(c, 0.6, 0.8, 1.0, front='w', back='w')
        drops(c, [3, 6, 9, 12], 8, 4)
        drops(c, [5, 8, 11], 11, 2, 'b')
    elif name == 'storm':
        cloud(c, 0.6, 0.8, 1.0, front='w', back='w')
        bolt(c, [(8, 7), (7, 7), (7, 8), (6, 8), (6, 9), (5, 9), (6, 10), (7, 10), (8, 10),
                 (7, 11), (6, 11), (6, 12), (5, 13)])
        drops(c, [3, 12], 8, 3)
    elif name == 'snow':
        cloud(c, 0.6, 0.8, 1.0)
        flakes(c, [(3, 9), (7, 9), (11, 9), (5, 11), (9, 11), (13, 11), (3, 13), (7, 13), (11, 13)], 'C')
    elif name == 'fog':
        cloud(c, 1.4, 0.4, 0.9, front='w', back=None)
        fog(c, [(7, 1, 13), (9, 0, 11), (11, 3, 14), (13, 1, 12)], 'W')
    return c

def small(name):
    c = canvas(8, 8)
    if name == 'clear':
        sun(c, 4, 4, 2.3, rays=False)
        for x, y in [(4, 0), (0, 4), (7, 4), (4, 7), (1, 1), (6, 1), (1, 6), (6, 6)]: plot(c, x, y, 'y')
    elif name == 'moon':
        disc(c, 3.6, 4, 3.0, 'W'); disc(c, 5.3, 3.2, 2.6, '.'); plot(c, 6, 6, 'Y')
    elif name == 'few':
        sun(c, 3, 3, 2.2, rays=False); cloud(c, 2.0, 3.2, 0.5)
    elif name == 'partly':
        sun(c, 3, 2.6, 2.3, rays=False); cloud(c, 0.4, 2.2, 0.6)
    elif name == 'cloudy':
        cloud(c, 1.6, 0.0, 0.5, front='w', back=None); cloud(c, 0.0, 2.2, 0.6)
    elif name == 'showers':
        sun(c, 5.8, 2.0, 1.7, rays=False); cloud(c, -0.3, 0.6, 0.6); drops(c, [2, 5], 6, 2)
    elif name == 'rain':
        cloud(c, 0.0, 0.0, 0.6, front='w', back='w'); drops(c, [1, 3, 5, 7], 5, 3)
    elif name == 'storm':
        cloud(c, 0.0, 0.0, 0.6, front='w', back='w'); bolt(c, [(4, 4), (3, 5), (4, 5), (3, 6), (2, 7)]); drops(c, [6], 5, 2)
    elif name == 'snow':
        cloud(c, 0.0, 0.0, 0.6); flakes(c, [(1, 5), (4, 5), (7, 5), (2, 7), (5, 7)], 'C')
    elif name == 'fog':
        fog(c, [(1, 0, 7), (3, 1, 8), (5, 0, 6), (7, 2, 8)], 'w')
    return c

NAMES = ['clear', 'few', 'partly', 'cloudy', 'showers', 'rain', 'storm', 'snow', 'fog', 'moon']

def preview(path):
    # every icon, big then small, as cells of 8x16 pixels, on a dark ground
    S = 8
    cols = len(NAMES)
    W, H = cols * 16 * S, (14 + 2 + 8) * S + 2 * S
    img = [[(20, 22, 30)] * W for _ in range(H)]
    for i, n in enumerate(NAMES):
        for which, yoff in ((big(n), 0), (small(n), 16)):
            for y, row in enumerate(which):
                for x, ch in enumerate(row):
                    if PAL[ch]:
                        for dy in range(S):
                            for dx in range(S):
                                img[(yoff + y) * S + dy][(i * 16 + x) * S + dx] = PAL[ch]
    with open(path, 'wb') as f:
        f.write(b'P6 %d %d 255\n' % (W, H))
        for row in img:
            f.write(bytes(v for px in row for v in px))

if __name__ == '__main__':
    if sys.argv[1] == 'preview':
        preview('icons.ppm')
    else:
        out = []
        out.append('// Generated by icons.py: the icons as pixels, two to a character cell')
        out.append('// (top and bottom), as letters of the palette in icons.c.  Each is one')
        out.append('// string, row after row: CMOC keeps a const char[] in the module, but')
        out.append('// an array of strings in the data area, and OS-9 allots that no room.')
        out.append('// @license gpl v. 3, see fujinet/LICENSE for details.')
        for name, w, h, f in (('big', 14, 14, big), ('small', 8, 8, small)):
            out.append('#define %s_W %d' % (name.upper(), w))
            out.append('#define %s_H %d' % (name.upper(), h))
            out.append('static const char %s_icons[%d] =' % (name, len(NAMES) * w * h + 1))
            for n in NAMES:
                out.append('    // ' + n)
                out += ['    "%s"' % ''.join(r) for r in f(n)]
            out[-1] += ';'
        print('\n'.join(out))
