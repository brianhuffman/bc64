/* vic_redraw.h - screen redrawing functions for c64 emulator */

#ifndef __VIC_REDRAW_H
#define __VIC_REDRAW_H

int vic_redraw_screen_line(int raster_line, int *video_mode,
	int vic_registers[0x40]);

/* structures to hold rendering data */
typedef struct render_line_s {
	int block_data[40];
	int border_color;
	int csel_xscroll;
	int sprite_data[8];
	int sprite_xpos[8];
	int sprite_color1;
	int sprite_color2;
} render_line;

void vic_update_raster(int value);
const render_line *vic_get_render_data();

/*
block data:
-----------
( 0- 7)  pixel data
( 8-11)  color #0
(12-15)  color #1
(16-19)  color #2
(20-23)  color #3
(31)     multicolor?

sprite data:
------------
( 0-23)  pixel data
(24-27)  color #1
(28)     bg priority?
(29)     multicolor?
(30)     x-expand?

sprite enable is implicit (non-zero pixels)
*/

#endif
