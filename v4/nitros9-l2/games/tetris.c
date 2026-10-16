// tetris.c -- Tetris for NitrOS-9 Level 2 on the TFR/911H.
//
// Draws with ANSI escape sequences on the PC terminal the tether runs in.
// It opens its own path to /term, so it can turn off echo and line editing
// without changing the Shell's path, and redraws only the cells that change.
//
// Keys: arrows or a/d/w/s (or j/l/i/k), space = drop, p = pause, q = quit.
// On the TFR/911H, arrows arrive as a CoCo 3 sends them: left $08,
// right $09, up $0C, down $0A.
//
// Build:  cmoc --os9 -o tetris tetris.c

#include <cmoc.h>

typedef unsigned char byte;

#define W 10                // well width
#define H 20                // well height
#define TOP 2               // screen row of the top of the well
#define LEFT 25             // screen column of the left wall
#define PANEL (LEFT + 2 * W + 7)  // screen column of the score panel

#define EMPTY 0
#define GHOST 8
#define UNKNOWN 255

// Seven pieces, four rotations each, as 4x4 bitmaps (top-left is $8000).
const unsigned shapes[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444},  // I
    {0x6600, 0x6600, 0x6600, 0x6600},  // O
    {0x4E00, 0x4640, 0x0E40, 0x4C40},  // T
    {0x6C00, 0x4620, 0x06C0, 0x8C40},  // S
    {0xC600, 0x2640, 0x0C60, 0x4C80},  // Z
    {0x8E00, 0x6440, 0x0E20, 0x44C0},  // J
    {0x2E00, 0x4460, 0x0E80, 0xC440},  // L
};

// Cell looks, by board value: empty, the seven pieces, the ghost.
const char *const looks[9] = {
    "\033[0m .",
    "\033[0;106m  ",           // I cyan
    "\033[0;103m  ",           // O yellow
    "\033[0;105m  ",           // T magenta
    "\033[0;102m  ",           // S green
    "\033[0;101m  ",           // Z red
    "\033[0;104m  ",           // J blue
    "\033[0;48;5;208m  ",      // L orange
    "\033[0;90m[]",            // ghost
};

// Ticks (1/60 s) per row, by level.
const byte speeds[20] = {48, 43, 38, 33, 28, 23, 18, 13, 8, 6,
                         5,  5,  5,  4,  4,  4,  3,  3,  3, 2};

byte board[H][W];   // EMPTY or a piece number 1..7
byte shown[H][W];   // what the screen shows, or UNKNOWN
byte term;          // our path to /term
byte sig;           // set by the signal intercept (^C, ^E)

byte cur, rot, nextp;
int px, py;
unsigned score, lines;
byte level, timer, bagpos;
byte bag[7];
byte panel_dirty, paused, over;
unsigned seed;

// ── OS-9 system calls ──────────────────────────────────────────────

char out[600];
unsigned outlen;

void flush(void) {
    byte path = term;
    char *p = out;
    unsigned n = outlen;
    if (n == 0) return;
    asm {
        lda     :path
        ldx     :p
        pshs    y
        ldy     :n
        os9     $8A             ; I$Write
        puls    y
    }
    outlen = 0;
}

// Next keystroke, or -1 if none is waiting.
int key(void) {
    byte path = term, ch = 0, got = 0;
    asm {
        lda     :path
        ldb     #1              ; SS.Ready
        os9     $8D             ; I$GetStt
        bcs     @none
        lda     :path
        leax    :ch
        pshs    y
        ldy     #1
        os9     $89             ; I$Read
        puls    y
        bcs     @none
        inc     :got
@none
    }
    // Not "got ? ch : -1": CMOC gives that the type of ch, making -1 into 255.
    if (!got) return -1;
    return (int)ch;
}

void sleep_tick(void) {
    asm {
        ldx     #1
        os9     $0A             ; F$Sleep
    }
}

// The signal intercept: OS-9 calls it with U = &sig and B = the signal.
asm __norts__ void intercept(void) {
    asm {
        stb     ,u
        rti
    }
}

void catch_signals(void) {
    byte *flag = &sig;
    void (*handler)(void) = intercept;
    asm {
        ldx     :handler
        pshs    u
        ldu     :flag
        os9     $09             ; F$Icpt
        puls    u
    }
}

// Open /term, with echo, pause and the EOF key turned off on this path.
byte open_term(void) {
    char name[7];
    byte opts[32];
    byte path = 0, err = 0;
    name[0] = '/'; name[1] = 't'; name[2] = 'e'; name[3] = 'r';
    name[4] = 'm'; name[5] = 13; name[6] = 0;
    asm {
        lda     #3              ; UPDAT.
        leax    :name
        os9     $84             ; I$Open
        bcs     @err
        sta     :path
        clrb
@err
        stb     :err
    }
    if (err) return 0;
    asm {
        lda     :path
        ldb     #0              ; SS.Opt
        leax    :opts
        os9     $8D             ; I$GetStt
    }
    opts[4] = 0;                // PD.EKO: no echo
    opts[7] = 0;                // PD.PAU: no page pause
    opts[12] = 0;               // PD.EOF: ESC is just a key
    asm {
        lda     :path
        ldb     #0              ; SS.Opt
        leax    :opts
        os9     $8E             ; I$SetStt
    }
    return path;
}

void close_term(void) {
    byte path = term;
    asm {
        lda     :path
        os9     $8F             ; I$Close
    }
}

// Seconds and minutes from the clock, to seed the randomizer.
unsigned clock_seed(void) {
    byte t[6];
    asm {
        leax    :t
        os9     $15             ; F$Time
    }
    return (unsigned)t[3] * 3600u + (unsigned)t[4] * 60u + t[5];
}

// ── Output helpers ─────────────────────────────────────────────────

int cur_row, cur_col;    // where the terminal's cursor is, or 0
byte cur_look;

void put(const char *s) {
    while (*s) {
        if (outlen >= sizeof out) flush();
        out[outlen++] = *s++;
    }
}

void put_num(unsigned n, byte width) {
    char buf[6];
    byte i = 0;
    do {
        buf[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n && i < 5);
    while (width > i) {
        put(" ");
        width--;
    }
    while (i) {
        char c[2];
        c[0] = buf[--i];
        c[1] = 0;
        put(c);
    }
}

void go(int row, int col) {
    if (row == cur_row && col == cur_col) return;
    put("\033[");
    put_num((unsigned)row, 0);
    put(";");
    put_num((unsigned)col, 0);
    put("H");
    cur_row = row;
    cur_col = col;
}

void text(int row, int col, const char *s) {
    go(row, col);
    put("\033[0m");
    put(s);
    cur_row = 0;
    cur_look = UNKNOWN;
}

// ── The game ───────────────────────────────────────────────────────

unsigned rnd(void) {
    seed = seed * 25173u + 13849u;
    return seed >> 4;
}

byte next_piece(void) {
    byte i, j, t;
    if (bagpos == 0) {          // refill the bag of seven, shuffled
        for (i = 0; i < 7; i++) bag[i] = i;
        for (i = 6; i > 0; i--) {
            j = (byte)(rnd() % (i + 1));
            t = bag[i]; bag[i] = bag[j]; bag[j] = t;
        }
        bagpos = 7;
    }
    return bag[--bagpos];
}

byte fits(byte p, byte r, int x, int y) {
    unsigned m = shapes[p][r];
    unsigned bit = 0x8000;
    int i, cx, cy;
    for (i = 0; i < 16; i++, bit >>= 1) {
        if (!(m & bit)) continue;
        cx = x + (i & 3);
        cy = y + (i >> 2);
        if (cx < 0 || cx >= W || cy >= H) return 0;
        if (cy >= 0 && board[cy][cx]) return 0;
    }
    return 1;
}

void spawn(void) {
    cur = nextp;
    nextp = next_piece();
    rot = 0;
    px = 3;
    py = (shapes[cur][0] & 0xF000) ? 0 : -1;
    timer = 0;
    panel_dirty = 1;
    if (!fits(cur, rot, px, py)) over = 1;
}

void clear_lines(void) {
    int y, x, yy;
    byte full, n = 0;
    for (y = H - 1; y >= 0; y--) {
        full = 1;
        for (x = 0; x < W; x++) {
            if (!board[y][x]) { full = 0; break; }
        }
        if (!full) continue;
        for (yy = y; yy > 0; yy--) {
            for (x = 0; x < W; x++) board[yy][x] = board[yy - 1][x];
        }
        for (x = 0; x < W; x++) board[0][x] = EMPTY;
        n++;
        y++;                    // look at this row again
    }
    if (n) {
        static const unsigned points[5] = {0, 40, 100, 300, 1200};
        score += points[n] * (level + 1);
        lines += n;
        level = (byte)(lines / 10);
        if (level > 19) level = 19;
        panel_dirty = 1;
    }
}

void lock(void) {
    unsigned m = shapes[cur][rot];
    unsigned bit = 0x8000;
    int i, cy;
    for (i = 0; i < 16; i++, bit >>= 1) {
        if (!(m & bit)) continue;
        cy = py + (i >> 2);
        if (cy < 0) { over = 1; continue; }
        board[cy][px + (i & 3)] = (byte)(cur + 1);
    }
    clear_lines();
    if (!over) spawn();
}

// Move down one row, or lock the piece if it can't.
void step(void) {
    if (fits(cur, rot, px, py + 1)) {
        py++;
    } else {
        lock();
    }
}

void rotate(void) {
    static const signed char kicks[5] = {0, -1, 1, -2, 2};
    byte r = (byte)((rot + 1) & 3);
    byte i;
    for (i = 0; i < 5; i++) {
        if (fits(cur, r, px + kicks[i], py)) {
            rot = r;
            px += kicks[i];
            return;
        }
    }
}

void handle(int k) {
    if (k >= 'A' && k <= 'Z') k += 'a' - 'A';
    if (k == 'q' || k == 27) { over = 2; return; }
    if (k == 'p') { paused = !paused; panel_dirty = 1; return; }
    if (paused || over) return;
    switch (k) {
        case 8: case 'a': case 'j':
            if (fits(cur, rot, px - 1, py)) px--;
            break;
        case 9: case 'd': case 'l':
            if (fits(cur, rot, px + 1, py)) px++;
            break;
        case 12: case 'w': case 'i': case 'x':
            rotate();
            break;
        case 10: case 's': case 'k':
            if (fits(cur, rot, px, py + 1)) {
                py++;
                score++;
                panel_dirty = 1;
                timer = 0;
            }
            break;
        case ' ':
            while (fits(cur, rot, px, py + 1)) {
                py++;
                score += 2;
            }
            panel_dirty = 1;
            lock();
            break;
    }
}

// ── Drawing ────────────────────────────────────────────────────────

void draw_frame(void) {
    int y;
    byte x;
    put("\033[0m\033[2J\033[?25l");     // clear, hide the cursor
    cur_row = 0;
    cur_look = UNKNOWN;
    for (y = 0; y < H; y++) {
        text(TOP + y, LEFT, "<!");
        text(TOP + y, LEFT + 2 + 2 * W, "!>");
    }
    go(TOP + H, LEFT);
    put("\033[0m<!");
    for (x = 0; x < W; x++) put("==");
    put("!>");
    go(TOP + H + 1, LEFT + 2);
    for (x = 0; x < W; x++) put("\\/");
    cur_row = 0;

    text(TOP, PANEL, "T E T R I S");
    text(TOP + 1, PANEL, "on the TFR/911H");
    text(TOP + 3, PANEL, "Score");
    text(TOP + 4, PANEL, "Lines");
    text(TOP + 5, PANEL, "Level");
    text(TOP + 7, PANEL, "Next");
    text(TOP + 13, PANEL, "<- -> or a d   move");
    text(TOP + 14, PANEL, "up    or w     rotate");
    text(TOP + 15, PANEL, "down  or s     soft drop");
    text(TOP + 16, PANEL, "space          drop");
    text(TOP + 17, PANEL, "p pause    q quit");

    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) shown[y][x] = UNKNOWN;
}

void draw_panel(void) {
    byte r, c;
    unsigned m, bit;
    go(TOP + 3, PANEL + 6);
    put("\033[0m");
    cur_look = UNKNOWN;
    put_num(score, 6);
    go(TOP + 4, PANEL + 6);
    put_num(lines, 6);
    go(TOP + 5, PANEL + 6);
    put_num(level, 6);
    cur_row = 0;
    m = shapes[nextp][0];
    bit = 0x8000;
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++, bit >>= 1) {
            go(TOP + 8 + r, PANEL + 2 * c);
            if (m & bit) {
                put(looks[nextp + 1]);
                cur_look = (byte)(nextp + 1);
            } else {
                put("\033[0m  ");
                cur_look = UNKNOWN;
            }
            cur_col += 2;
        }
    }
    text(TOP + 19, PANEL, paused ? "** PAUSED **  " : "              ");
    panel_dirty = 0;
}

// What the well should look like: the board, the ghost, the piece.
byte want[H][W];

void compose(void) {
    unsigned m = shapes[cur][rot];
    unsigned bit;
    int i, y, x, gy;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) want[y][x] = board[y][x];
    if (over) return;
    gy = py;
    while (fits(cur, rot, px, gy + 1)) gy++;
    for (i = 0, bit = 0x8000; i < 16; i++, bit >>= 1) {
        if (!(m & bit)) continue;
        y = gy + (i >> 2);
        if (y >= 0 && !want[y][px + (i & 3)]) want[y][px + (i & 3)] = GHOST;
    }
    for (i = 0, bit = 0x8000; i < 16; i++, bit >>= 1) {
        if (!(m & bit)) continue;
        y = py + (i >> 2);
        if (y >= 0) want[y][px + (i & 3)] = (byte)(cur + 1);
    }
}

void draw_well(void) {
    int y, x;
    byte w;
    compose();
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            w = want[y][x];
            if (w == shown[y][x]) continue;
            go(TOP + y, LEFT + 2 + 2 * x);
            if (w != cur_look) {
                put(looks[w]);
                cur_look = w;
            } else {
                // Same colour as the cell before: skip the escape sequence.
                const char *s = looks[w];
                while (*s && *s != 'm') s++;
                put(s + 1);
            }
            cur_col += 2;
            shown[y][x] = w;
        }
    }
}

void play(void) {
    int k, y, x;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) board[y][x] = EMPTY;
    score = lines = 0;
    level = 0;
    bagpos = 0;
    paused = 0;
    over = 0;
    nextp = next_piece();
    spawn();
    draw_frame();
    panel_dirty = 1;

    while (!over && !sig) {
        while ((k = key()) >= 0) handle(k);
        if (!paused && !over && ++timer >= speeds[level]) {
            timer = 0;
            step();
        }
        draw_well();
        if (panel_dirty) draw_panel();
        flush();
        sleep_tick();
    }
}

int main(void) {
    int k;
    byte again = 1;
    term = open_term();
    if (!term) {
        printf("tetris: cannot open /term\n");
        return 1;
    }
    catch_signals();
    seed = clock_seed();

    while (again && !sig) {
        play();
        if (over == 2 || sig) break;     // quit
        draw_well();
        text(TOP + 8, LEFT + 2, "  GAME  OVER  ");
        text(TOP + 10, LEFT + 2, " r: again     ");
        text(TOP + 11, LEFT + 2, " q: quit      ");
        flush();
        again = 0;
        while (!sig) {
            k = key();
            if (k == 'r' || k == 'R') { again = 1; break; }
            if (k == 'q' || k == 'Q' || k == 27) break;
            sleep_tick();
        }
    }

    put("\033[0m\033[2J\033[H\033[?25h");   // clear, show the cursor
    flush();
    close_term();
    return 0;
}
