// glyphs.h -- Fuji Battleship on NitrOS-9: its own graphics characters,
// which term.c sends as UTF-8 (see term_glyphs()).  All are in the IBM
// PC's character set, which nearly every terminal font has.
//
// @license gpl v. 3, see fujinet/LICENSE for details.

#ifndef GLYPHS_H
#define GLYPHS_H

#define G_HLINE   0x80      // ─
#define G_VLINE   0x81      // │
#define G_UL      0x82      // ┌
#define G_UR      0x83      // ┐
#define G_LL      0x84      // └
#define G_LR      0x85      // ┘
#define G_RTEE    0x86      // ┤
#define G_LTEE    0x87      // ├
#define G_LEFT    0x88      // ▌
#define G_RIGHT   0x89      // ▐
#define G_LOWER   0x8A      // ▄
#define G_UPPER   0x8B      // ▀
#define G_FULL    0x8C      // █
#define G_LIGHT   0x8D      // ░
#define G_MEDIUM  0x8E      // ▒
#define G_DARK    0x8F      // ▓
#define G_LARROW  0x90      // ◄
#define G_RARROW  0x91      // ►
#define G_SQUARE  0x92      // ■
#define G_BULLET  0x93      // •
#define G_RING    0x94      // ○
#define G_SUN     0x95      // ☼
#define G_WAVES   0x96      // ≈
#define G_TIMES   0x97      // ×
#define G_DOT     0x98      // ·
#define G_DH      0x99      // ═
#define G_DV      0x9A      // ║
#define G_DUL     0x9B      // ╔
#define G_DUR     0x9C      // ╗
#define G_DLL     0x9D      // ╚
#define G_DLR     0x9E      // ╝
#define N_GLYPHS  31

extern const char glyph_table[];

#endif
