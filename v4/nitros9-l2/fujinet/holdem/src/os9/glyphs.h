// glyphs.h -- Texas Hold'em on NitrOS-9: the characters past ASCII that
// it draws with, each a code from 0x80 up, which src/os9/screen.c sends
// as UTF-8.  title.py reads the codes from here, so each line is
// "#define G_NAME 0xNN // the character".
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifndef GLYPHS_H
#define GLYPHS_H

#define G_HLINE      0x80    // ─
#define G_VLINE      0x81    // │
#define G_ARC_UL     0x82    // ╭
#define G_ARC_UR     0x83    // ╮
#define G_ARC_LL     0x84    // ╰
#define G_ARC_LR     0x85    // ╯
#define G_SPADE      0x86    // ♠
#define G_HEART      0x87    // ♥
#define G_DIAMOND    0x88    // ♦
#define G_CLUB       0x89    // ♣
#define G_CHIP       0x8A    // ●
#define G_UPPER      0x8B    // ▀
#define G_LOWER      0x8C    // ▄
#define G_FULL       0x8D    // █
#define G_QUAD_ULLR  0x8E    // ▚
#define G_LARROW     0x8F    // ←
#define G_RARROW     0x90    // →
#define G_UARROW     0x91    // ↑
#define G_DARROW     0x92    // ↓
#define G_POINT_R    0x93    // ►
#define N_GLYPHS     20

// Their UTF-8, four bytes apart (padded with NULs), in the order above.
#define GLYPH_UTF8 \
    "\xE2\x94\x80\0" \
    "\xE2\x94\x82\0" \
    "\xE2\x95\xAD\0" \
    "\xE2\x95\xAE\0" \
    "\xE2\x95\xB0\0" \
    "\xE2\x95\xAF\0" \
    "\xE2\x99\xA0\0" \
    "\xE2\x99\xA5\0" \
    "\xE2\x99\xA6\0" \
    "\xE2\x99\xA3\0" \
    "\xE2\x97\x8F\0" \
    "\xE2\x96\x80\0" \
    "\xE2\x96\x84\0" \
    "\xE2\x96\x88\0" \
    "\xE2\x96\x9A\0" \
    "\xE2\x86\x90\0" \
    "\xE2\x86\x92\0" \
    "\xE2\x86\x91\0" \
    "\xE2\x86\x93\0" \
    "\xE2\x96\xBA\0"

#endif
