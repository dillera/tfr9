#ifndef CHARDEF_H
#define CHARDEF_H

// The line drawing characters, as term.c's graphics characters, which it
// sends to the terminal as UTF-8.
#include "term.h"

#define CH_HLINE        TG_HLINE
#define CH_VLINE        TG_VLINE
#define CH_ULCORNER     TG_ULCORNER
#define CH_URCORNER     TG_URCORNER
#define CH_LLCORNER     TG_LLCORNER
#define CH_LRCORNER     TG_LRCORNER
#define CH_RTEE         TG_RTEE
#define CH_LTEE         TG_LTEE
#define CH_TTEE         TG_TTEE
#define CH_BTEE         TG_BTEE
#define CH_CROSS        TG_CROSS

#define CH_CURS_LEFT    KEY_LEFT
#define CH_DEL          KEY_BS
#define CH_ENTER        KEY_ENTER

#endif // CHARDEF_H
