#ifndef BWC_SCREEN_H
#define BWC_SCREEN_H

// NitrOS-9 on the TFR/911H: the tether's terminal, 80 columns by 24 rows.
// The world's width is set on the title screen: 40, with each place two
// columns wide (see conio_wrapper.h), so that the shapes are about as
// wide as they are tall, or 80, a place to a column.
extern unsigned char screen_width;
#define SCREEN_WIDTH (screen_width)
#define SCREEN_HEIGHT (24)
#endif // BWC_SCREEN_H
