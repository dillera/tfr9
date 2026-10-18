#include "conio_wrapper.h"
#include "data.h"
#include "shutdown.h"
#include "sound.h"

void sound_tick(void);

/*
 * cc65 conio like, text console functions
 * on NitrOS-9: the tether's terminal, through fujinet/c/term.c
 *
 */

static byte colorset;       // the screen in reverse video ('C')
byte in_simulation;         // set by the first frame (double_buffer.c)

unsigned char screen_width = 40;
byte cell_shift = 1;

void set_screen_width(byte width)
{
	screen_width = width;
	cell_shift = width == 40;
}

// At 40, the two columns of each graphics character: the box drawing
// keeps its vertical strokes in the left column and carries its
// horizontal ones on through the right, so that the lines of a shape join
// up; the blocks are drawn twice as wide.
static const byte wide[26][2] = {
	{ TG_HLINE,     TG_HLINE     },	// ─
	{ TG_VLINE,     ' '          },	// │
	{ TG_ULCORNER,  TG_HLINE     },	// ┌
	{ TG_URCORNER,  ' '          },	// ┐
	{ TG_LLCORNER,  TG_HLINE     },	// └
	{ TG_LRCORNER,  ' '          },	// ┘
	{ TG_RTEE,      ' '          },	// ┤
	{ TG_LTEE,      TG_HLINE     },	// ├
	{ TG_TTEE,      TG_HLINE     },	// ┬
	{ TG_BTEE,      TG_HLINE     },	// ┴
	{ TG_CROSS,     TG_HLINE     },	// ┼
	{ TG_FULL,      ' '          },	// ▌
	{ ' ',          TG_FULL      },	// ▐
	{ TG_LOWERHALF, TG_LOWERHALF },	// ▄
	{ TG_UPPERHALF, TG_UPPERHALF },	// ▀
	{ TG_LOWERHALF, ' '          },	// ▖
	{ ' ',          TG_LOWERHALF },	// ▗
	{ TG_UPPERHALF, ' '          },	// ▘
	{ ' ',          TG_UPPERHALF },	// ▝
	{ TG_UPPERHALF, TG_FULL      },	// ▜
	{ TG_FULL,      TG_UPPERHALF },	// ▛
	{ TG_LOWERHALF, TG_FULL      },	// ▟
	{ TG_FULL,      TG_LOWERHALF },	// ▙
	{ TG_FULL,      TG_FULL      },	// █
	{ TG_UPPERHALF, TG_LOWERHALF },	// ▚
	{ TG_LOWERHALF, TG_UPPERHALF },	// ▞
};

// Each shape's color, by its number.
static const byte shape_colors[6] = {
	TC_BRIGHT + TC_RED, TC_BRIGHT + TC_YELLOW, TC_BRIGHT + TC_GREEN,
	TC_BRIGHT + TC_CYAN, TC_BRIGHT + TC_MAGENTA, TC_BRIGHT + TC_WHITE
};

static void put_colored(byte c, byte fg)
{
	term_color(c == ' ' ? TC_DEFAULT : fg, TC_DEFAULT);
	term_putc((char) c);
}

void cputc_cell(char c, uint8_t shape)
{
	byte ch = (byte) c, fg = shape_colors[shape % 6];

	if (!cell_shift) {
		put_colored(ch, fg);
	} else if (ch >= TG_HLINE && ch < TG_HLINE + 26) {
		put_colored(wide[ch - TG_HLINE][0], fg);
		put_colored(wide[ch - TG_HLINE][1], fg);
	} else {
		put_colored(ch, fg);
		put_colored(' ', fg);
	}
	term_color(TC_DEFAULT, TC_DEFAULT);
}

void hirestxt_init(void)
{
	term_init();
	init_sound();
}

// The CoCo's other color set: here the whole screen in reverse video.
void switch_colorset(void)
{
	colorset = !colorset;
	term_raw(colorset ? "\033[?5h" : "\033[?5l");
	term_flush();
}

// ESC or BREAK (^C, ^E) quits: in the simulation as Q does, so that the
// server is told; before it, at once.
static char quit_key(char c)
{
	if (c != KEY_BREAK)
		return c;
	if (in_simulation)
		return 'q';
	cleanup_client();
	exit(0);
	return 0;
}

char os9_key(void)
{
	sound_tick();
	if (term_ready())
		return quit_key((char) term_getc());
	term_sleep(1);
	return 0;
}

byte cgetc(void)
{
	return (byte) quit_key((char) term_getc());
}

void chlinexy(unsigned char x, unsigned char y, unsigned char length)
{
	gotoxy(x, y);
	while (length--)
	{
		cputc(CH_HLINE);
	}
}

void cputcxy(unsigned char x, unsigned char y, char c)
{
	gotoxy(x, y);
	cputc(c);
}

void cputsxy(unsigned char x, unsigned char y, const char *s)
{
	gotoxy(x, y);
	cputs(s);
}

// term_done() clears the screen.  An error may come in the middle of a
// frame, so what is drawn so far is sent first.
unsigned char doesclrscrafterexit(void)
{
	term_frame(0);
	term_flush();
	return 1;
}
