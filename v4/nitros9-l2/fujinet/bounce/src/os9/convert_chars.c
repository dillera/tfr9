#include <cmoc.h>
#include <coco.h>

#include "chardef.h"
#include "convert_chars.h"

void convert_chars(uint8_t *data, uint8_t len) {
	uint8_t i;
	for (i = 0; i < len; i++) {
		switch(data[i]) {
			case 'r': data[i] = TG_ULCORNER;  break;   // ┌
			case ')': data[i] = TG_URCORNER;  break;   // ┐
			case 'L': data[i] = TG_LLCORNER;  break;   // └
			case '!': data[i] = TG_LRCORNER;  break;   // ┘
			case 'J': data[i] = TG_RTEE;      break;   // ┤
			case 't': data[i] = TG_LTEE;      break;   // ├
			case 'T': data[i] = TG_TTEE;      break;   // ┬
			case '2': data[i] = TG_BTEE;      break;   // ┴
			case '|': data[i] = TG_VLINE;     break;   // │
			case '-': data[i] = TG_HLINE;     break;   // ─
			case '+': data[i] = TG_CROSS;     break;   // ┼

			case 'a': data[i] = TG_LEFTHALF;  break;   // ▌
			case 'b': data[i] = TG_RIGHTHALF; break;   // ▐
			case 'c': data[i] = TG_LOWERHALF; break;   // ▄
			case 'd': data[i] = TG_UPPERHALF; break;   // ▀
			case 'e': data[i] = TG_QUAD_LL;   break;   // ▖
			case 'f': data[i] = TG_QUAD_LR;   break;   // ▗
			case 'g': data[i] = TG_QUAD_UL;   break;   // ▘
			case 'h': data[i] = TG_QUAD_UR;   break;   // ▝
			case 'i': data[i] = TG_QUAD_NOLL; break;   // ▜
			case 'j': data[i] = TG_QUAD_NOLR; break;   // ▛
			case 'k': data[i] = TG_QUAD_NOUL; break;   // ▟
			case 'l': data[i] = TG_QUAD_NOUR; break;   // ▙
			case 'm': data[i] = TG_FULL;      break;   // █
			case 'n': data[i] = TG_QUAD_ULLR; break;   // ▚
			case 'p': data[i] = TG_QUAD_URLL; break;   // ▞

			default: break;
		}
	}
}
