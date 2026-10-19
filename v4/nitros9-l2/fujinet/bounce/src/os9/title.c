// @license gpl v. 3, see fujinet/LICENSE for details.
#include "conio_wrapper.h"
#include "appkey.h"
#include "data.h"
#include "get_line.h"
#include "screen.h"
#include "shapes.h"
#include "title.h"

// NitrOS-9: the screens before the world, made for the tether's 80 by 24
// terminal.  The title is where the server, the name and the world's
// width are set, while two balls bounce; then come connecting, the kinds
// of shape, and the client's number.

#define APP_KEY_WIDTH 3     // NitrOS-9's own app key: "40" or "80"

#define BOX_L      2        // the box of settings
#define BOX_R      77
#define BOX_Y      10
#define LABEL_X    4
#define VALUE_X    13
#define ROW_SERVER 11
#define ROW_NAME   12
#define ROW_WIDTH  13
#define BALLS_Y    16       // the band the balls bounce in
#define BALLS_H    4
#define ROW_KEYS   21
#define ROW_HINT   22
#define GAP        2        // between the kinds of shape, in places

#define WHITE      (TC_BRIGHT + TC_WHITE)

extern char endpoint_input[61];
extern char *version;
extern const char build_date[];
extern const char upstream_version[];

static const byte rainbow[6] = {
	TC_BRIGHT + TC_RED, TC_BRIGHT + TC_YELLOW, TC_BRIGHT + TC_GREEN,
	TC_BRIGHT + TC_CYAN, TC_BRIGHT + TC_BLUE, TC_BRIGHT + TC_MAGENTA
};

static void color(byte fg)
{
	term_color(fg, TC_DEFAULT);
}

static void put(byte fg, const char *s)
{
	color(fg);
	term_puts(s);
}

// A row of half blocks in the colors of the rainbow.
static void stripe(byte y, byte glyph)
{
	byte x;

	term_gotoxy(0, y);
	for (x = 0; x < TERM_COLS; x++) {
		color(rainbow[(word) x * 6 / TERM_COLS]);
		term_putc((char) glyph);
	}
	color(TC_DEFAULT);
}

// The column that centers len characters.
static byte centered(word len)
{
	return len < TERM_COLS ? (byte) ((TERM_COLS - len) / 2) : 0;
}

static void center(byte y, byte fg, const char *s)
{
	term_gotoxy(centered(strlen(s)), y);
	put(fg, s);
	color(TC_DEFAULT);
}

// "BOUNCE WORLD", in letters five dots square, two dots to a character.
static const char logo_letters[] = "BOUNCEWRLD";
static const byte logo_font[10][5] = {
	{ 0x1E, 0x11, 0x1E, 0x11, 0x1E },	// B
	{ 0x0E, 0x11, 0x11, 0x11, 0x0E },	// O
	{ 0x11, 0x11, 0x11, 0x11, 0x0E },	// U
	{ 0x11, 0x19, 0x15, 0x13, 0x11 },	// N
	{ 0x0F, 0x10, 0x10, 0x10, 0x0F },	// C
	{ 0x1F, 0x10, 0x1E, 0x10, 0x1F },	// E
	{ 0x11, 0x11, 0x15, 0x1B, 0x11 },	// W
	{ 0x1E, 0x11, 0x1E, 0x12, 0x11 },	// R
	{ 0x10, 0x10, 0x10, 0x10, 0x1F },	// L
	{ 0x1E, 0x11, 0x11, 0x11, 0x1E },	// D
};

static void logo(byte y)
{
	static const char text[] = "BOUNCE WORLD";
	static const byte dots[4] = { ' ', TG_LOWERHALF, TG_UPPERHALF, TG_FULL };
	byte r, k, b, top, bottom, n;
	const char *p;

	for (r = 0; r < 3; r++) {
		term_gotoxy(centered(sizeof text * 6 - 7), y + r);
		n = 0;
		for (p = text; *p; p++) {
			if (*p == ' ') {
				term_puts("      ");
				continue;
			}
			for (k = 0; logo_letters[k] != *p; k++)
				;
			top = logo_font[k][r * 2];
			bottom = r < 2 ? logo_font[k][r * 2 + 1] : 0;
			color(rainbow[n++ % 6]);
			for (b = 0x10; b; b >>= 1)
				term_putc((char) dots[(top & b ? 2 : 0) | (bottom & b ? 1 : 0)]);
			term_putc(' ');
		}
	}
	color(TC_DEFAULT);
}

// The lines under the logo: what it is, who made it, which version.
static void credits(void)
{
	static const char by[] = "by ", mark[] = "Mark Fisher", sep[] = "  *  ",
		coco[] = "CoCo version by ", rich[] = "Rich Stephens",
		port[] = "NitrOS-9 port by ", diller[] = "DILLER",
		ver[] = "Version ", built[] = "built ", tfr[] = " for the TFR/911H";
	word len;

	center(6, TC_BRIGHT + TC_CYAN,
		"Shapes that bounce from one computer's screen to the next");

	len = strlen(by) + strlen(mark) + strlen(coco) + strlen(rich)
		+ strlen(port) + strlen(diller) + 2 * strlen(sep);
	term_gotoxy(centered(len), 7);
	put(TC_WHITE, by);
	put(WHITE, mark);
	put(TC_BRIGHT + TC_MAGENTA, sep);
	put(TC_WHITE, coco);
	put(WHITE, rich);
	put(TC_BRIGHT + TC_MAGENTA, sep);
	put(TC_WHITE, port);
	put(WHITE, diller);

	len = strlen(ver) + strlen(version) + strlen(upstream_version) + 3
		+ strlen(sep) + strlen(built) + strlen(build_date) + strlen(tfr);
	term_gotoxy(centered(len), 8);
	put(TC_WHITE, ver);
	put(TC_BRIGHT + TC_YELLOW, version);
	put(TC_WHITE, " (");
	term_puts(upstream_version);
	term_putc(')');
	put(TC_BRIGHT + TC_MAGENTA, sep);
	put(TC_WHITE, built);
	put(TC_BRIGHT + TC_YELLOW, build_date);
	put(TC_WHITE, tfr);
	color(TC_DEFAULT);
}

static void settings_box(void)
{
	byte x, y;

	color(TC_BRIGHT + TC_BLUE);
	term_gotoxy(BOX_L, BOX_Y);
	term_putc((char) TG_ULCORNER);
	term_putc((char) TG_HLINE);
	put(WHITE, " Settings ");
	color(TC_BRIGHT + TC_BLUE);
	for (x = term_wherex(); x < BOX_R; x++)
		term_putc((char) TG_HLINE);
	term_putc((char) TG_URCORNER);
	for (y = BOX_Y + 1; y < BOX_Y + 4; y++) {
		term_gotoxy(BOX_L, y);
		term_putc((char) TG_VLINE);
		term_gotoxy(BOX_R, y);
		term_putc((char) TG_VLINE);
	}
	term_gotoxy(BOX_L, BOX_Y + 4);
	term_putc((char) TG_LLCORNER);
	for (x = BOX_L + 1; x < BOX_R; x++)
		term_putc((char) TG_HLINE);
	term_putc((char) TG_LRCORNER);
	color(TC_DEFAULT);
}

// A row of the settings: its label, its value, and the rest of the row
// cleared, to the box.
static void setting(byte y, const char *label, const char *value, byte fg)
{
	byte x;

	term_gotoxy(LABEL_X, y);
	put(WHITE, label);
	term_gotoxy(VALUE_X, y);
	put(fg, value);
	color(TC_DEFAULT);
	for (x = term_wherex(); x < BOX_R; x++)
		term_putc(' ');
}

void show_server(char *s)
{
	if (*s)
		setting(ROW_SERVER, "Server", s, TC_BRIGHT + TC_GREEN);
	else
		setting(ROW_SERVER, "Server", "not set: press S, then type host:9003", TC_BRIGHT + TC_RED);
}

void show_name(char *s)
{
	if (*s)
		setting(ROW_NAME, "Name", s, TC_BRIGHT + TC_GREEN);
	else
		setting(ROW_NAME, "Name", "not set: press N", TC_BRIGHT + TC_RED);
}

static void show_width(void)
{
	if (screen_width == 40)
		setting(ROW_WIDTH, "Width", "40  each place two columns wide: round shapes", TC_BRIGHT + TC_CYAN);
	else
		setting(ROW_WIDTH, "Width", "80  a place to a column: a world twice as wide", TC_BRIGHT + TC_CYAN);
}

void show_header(void)
{
	char w[4];

	if (read_appkey(w, sizeof w, APP_KEY_WIDTH) && w[0] == '8')
		set_screen_width(80);

	term_clrscr();
	stripe(0, TG_LOWERHALF);
	logo(2);
	credits();
	settings_box();
	show_width();
	stripe(23, TG_UPPERHALF);
}

static void hint(bool missing)
{
	term_gotoxy(0, ROW_HINT);
	term_clreol();
	if (missing)
		center(ROW_HINT, TC_BRIGHT + TC_RED, "Set the server and your name first");
	else
		center(ROW_HINT, TC_BRIGHT + TC_GREEN, "Any other key enters the world");
}

void show_menu(void)
{
	static const char *const keys[4] = { "S", "N", "W", "Esc" };
	static const char *const labels[4] = { "Server", "Name", "Width", "Quit" };
	word len = 0;
	byte i;

	for (i = 0; i < 4; i++)
		len += strlen(keys[i]) + strlen(labels[i]) + 3 + (i ? 4 : 0);
	term_gotoxy(centered(len), ROW_KEYS);
	for (i = 0; i < 4; i++) {
		if (i)
			term_puts("    ");
		term_color(TC_BLACK, TC_CYAN);
		term_putc(' ');
		term_puts(keys[i]);
		term_putc(' ');
		put(WHITE, " ");
		term_puts(labels[i]);
		color(TC_DEFAULT);
	}
	hint(false);
}

// The balls that bounce on the title screen, each changing color as it
// turns at an end.
typedef struct {
	byte x, y, color, steps;
	signed char dx, dy;
} Ball;

static Ball balls[2] = {
	{ 6, BALLS_Y, 0, 0, 1, 1 },
	{ 66, BALLS_Y + 2, 3, 1, -1, -1 }
};

static void draw_ball(Ball *b)
{
	color(rainbow[b->color]);
	term_gotoxy(b->x, b->y);
	term_putc((char) TG_QUAD_LR);
	term_putc((char) TG_LOWERHALF);
	term_putc((char) TG_LOWERHALF);
	term_putc((char) TG_QUAD_LL);
	term_gotoxy(b->x, b->y + 1);
	term_putc((char) TG_QUAD_UR);
	term_putc((char) TG_UPPERHALF);
	term_putc((char) TG_UPPERHALF);
	term_putc((char) TG_QUAD_UL);
}

static void bounce(void)
{
	byte i, y;
	Ball *b;

	term_frame(1);
	for (y = BALLS_Y; y < BALLS_Y + BALLS_H; y++) {
		term_gotoxy(0, y);
		term_clreol();
	}
	for (i = 0; i < 2; i++) {
		b = &balls[i];
		if ((b->dx < 0 && b->x == 0) || (b->dx > 0 && b->x == TERM_COLS - 4)) {
			b->dx = -b->dx;
			b->color = (b->color + 1) % 6;
		}
		b->x += b->dx;
		if (++b->steps == 3) {
			b->steps = 0;
			if ((b->dy < 0 && b->y == BALLS_Y) || (b->dy > 0 && b->y == BALLS_Y + BALLS_H - 2))
				b->dy = -b->dy;
			b->y += b->dy;
		}
		draw_ball(b);
	}
	color(TC_DEFAULT);
	term_frame(0);
	term_flush();
}

// A key, while the balls bounce.  Esc quits (os9_key()).
static char wait_key(void)
{
	char c;
	byte t = 0;

	for (;;) {
		c = os9_key();
		if (c)
			return c;
		if (++t == 3) {
			t = 0;
			bounce();
		}
	}
}

// Type a new value in place of a setting's; Esc, or nothing typed, keeps
// the old one.
static bool edit(byte y, char *value, byte len)
{
	char in[ENDPOINT_LEN + 1];
	byte x;

	term_gotoxy(VALUE_X, y);
	for (x = VALUE_X; x < BOX_R; x++)
		term_putc(' ');
	term_gotoxy(VALUE_X, y);
	color(TC_BRIGHT + TC_YELLOW);
	get_line(in, len);
	color(TC_DEFAULT);
	if (!*in)
		return false;
	strcpy(value, in);
	return true;
}

void get_info_changes(void)
{
	char w[4];

	for (;;) {
		switch (wait_key()) {
		case 'S':
		case 's':
			if (edit(ROW_SERVER, endpoint_input, ENDPOINT_LEN))
				write_appkey(endpoint_input, APP_KEY_ENDPOINT);
			show_server(endpoint_input);
			break;
		case 'N':
		case 'n':
			if (edit(ROW_NAME, name, NAME_LEN))
				write_appkey(name, APP_KEY_NAME);
			show_name(name);
			break;
		case 'W':
		case 'w':
			set_screen_width(screen_width == 40 ? 80 : 40);
			strcpy(w, screen_width == 40 ? "40" : "80");
			write_appkey(w, APP_KEY_WIDTH);
			show_width();
			break;
		default:
			if (*endpoint_input && *name)
				return;
			hint(true);
			term_bell();
			break;
		}
	}
}

// A new screen, between stripes, with a heading of three pieces in their
// colors.
static void heading(const char *a, byte fa, const char *b, byte fb, const char *c, byte fc)
{
	term_clrscr();
	stripe(0, TG_LOWERHALF);
	term_gotoxy(centered(strlen(a) + strlen(b) + strlen(c)), 2);
	put(fa, a);
	put(fb, b);
	put(fc, c);
	color(TC_DEFAULT);
	stripe(23, TG_UPPERHALF);
}

void show_connecting(void)
{
	heading("Connecting to ", WHITE, endpoint_input, TC_BRIGHT + TC_GREEN, " ...", WHITE);
	logo(9);
	term_flush();
}

void shapes_heading(void)
{
	center(14, TC_BRIGHT + TC_CYAN, "Fetching the shapes");
	term_flush();
}

// The kinds of shape, in rows centered on the screen.
void show_shape_gallery(void)
{
	byte i = 0, j, k, w, h, x, y = 4, used;
	char n[4];

	itoa(shape_count, n, 10);
	heading(n, TC_BRIGHT + TC_YELLOW, " kinds of shape, from ", WHITE,
		endpoint_input, TC_BRIGHT + TC_GREEN);

	while (i < shape_count) {
		used = 0;
		h = 0;
		for (j = i; j < shape_count; j++) {
			w = shapes[j].shape_width;
			if (j > i && used + GAP + w > SCREEN_WIDTH)
				break;
			used += (j > i ? GAP : 0) + w;
			if (w > h)
				h = w;
		}
		if (y + h > 19)
			break;
		x = used < SCREEN_WIDTH ? (SCREEN_WIDTH - used) / 2 : 0;
		for (k = i; k < j; k++) {
			display_shape_data(k, x, y);
			x += shapes[k].shape_width + GAP;
		}
		y += h + 1;
		i = j;
	}
}

void show_client_id(void)
{
	term_gotoxy(centered(15 + strlen(client_str)), 20);
	put(WHITE, "You are client ");
	put(TC_BRIGHT + TC_YELLOW, client_str);
	color(TC_DEFAULT);
}

void press_key_prompt(void)
{
	static const char s[] = " Press a key to enter the world ";

	term_gotoxy(centered(sizeof s - 1), 21);
	term_color(TC_BLACK, TC_CYAN);
	term_puts(s);
	color(TC_DEFAULT);
	term_flush();
}
