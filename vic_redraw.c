/* vic_redraw.c - screen redrawing functions for c64 emulator */

#include "vic_redraw.h"
#include "6510.h"
#include "mem_c64.h"

#define STANDARD  (0)
#define MULTI     (1)
#define CHARACTER (0)
#define BITMAP    (2)
#define EXTCOLOR  (4)
#define IDLE      (8)
#define DISABLED (16)

int sprite_collisions( render_line data );

static render_line render_data[312];

const render_line *vic_get_render_data() {
	return (render_data);
}

inline static int standard_block (int g, int c0, int c1) {
	return ( g | (c0<<8) | (c1<<12) );
}

inline static int multicolor_block (int g, int c0, int c1, int c2, int c3) {
	return ( g | (c0<<8) | (c1<<12) | (c2<<16) | (c3<<20) | (1<<31) );
}

int vic_redraw_screen_line(int raster_line, int *video_mode,
	int vic_registers[0x40]) {

	static int vc_base, rc, blank;
	static int c_buffer[40], c_colors[40];

	int bad_line, vmli, i;
	int c_data, g_data, color0, color1, color2, color3;

	int *block = render_data[raster_line].block_data;
	int *sprite = render_data[raster_line].sprite_data;

	int border_top, border_bottom;

	/* reset to top of screen */
	if (raster_line == 0x00) vc_base=-40;

	/* set disable flag */
	if (raster_line == 0x30) {
		if (vic_registers[0x11] & 0x10) *video_mode &= ~DISABLED;
		else *video_mode |= DISABLED;
	}

	/* is this a bad line? */
	bad_line = (!((raster_line ^ vic_registers[0x11]) & 0x07)) &&
		(raster_line >= 0x30) &&
		(raster_line <= 0xf7) &&
		(*video_mode < DISABLED);

	if (bad_line) {
		if (rc == 7) vc_base += 40;
		rc = 0;
		*video_mode &= ~IDLE;
		for (vmli = 0; vmli < 40; vmli++) {
			c_buffer[vmli] = mem_read_video_matrix (vc_base + vmli);
			c_colors[vmli] = mem_read_color_ram (vc_base + vmli);
		}
		cpu6510_bad_line();
	}
	else {
		if (rc == 7) *video_mode |= IDLE;
		else rc++;
	}

	/* misc. screen settings */
	render_data[raster_line].border_color = vic_registers[0x20]; //*video_mode;
	render_data[raster_line].csel_xscroll = vic_registers[0x16] & 0x0f;
	render_data[raster_line].sprite_color1 = vic_registers[0x25];
	render_data[raster_line].sprite_color2 = vic_registers[0x26];

	/* vertical border and screen blanking */
	if (vic_registers[0x11] & 0x08) {
		border_top = 0x33;
		border_bottom = 0xfb;
	} else {
		border_top = 0x37;
		border_bottom = 0xf7;
	}
	if ((raster_line == border_top) && (*video_mode < DISABLED)) blank = 0;
	else if (raster_line == border_bottom) blank = 1;

	if (blank) {
		render_data[raster_line].csel_xscroll = -1;
		return 0;
	}

	switch (*video_mode) {

	case (CHARACTER):
		for (vmli = 0; vmli < 40; vmli++) {
			c_data = c_buffer[vmli];
			g_data = mem_read_character_base ((c_data<<3) | rc);
			color0 = vic_registers[0x21];
			color1 = c_colors[vmli];
			block[vmli] = standard_block(g_data, color0, color1);
		}
		break;

	case (MULTI|CHARACTER):
		for (vmli = 0; vmli < 40; vmli++) {
			c_data = c_buffer[vmli];
			g_data = mem_read_character_base ((c_data<<3) | rc);
			color0 = vic_registers[0x21];
			color1 = vic_registers[0x22];
			color2 = vic_registers[0x23];
			color3 = c_colors[vmli];
			if (color3 & 0x08)
				block[vmli] = multicolor_block(g_data, color0, color1, color2, color3 & 7);
			else
				block[vmli] = standard_block(g_data, color0, color3 & 7);
		}
		break;

	case (BITMAP):
		for (vmli = 0; vmli < 40; vmli++) {
			c_data = c_buffer[vmli];
			g_data = mem_read_bitmap_base (((vc_base+vmli)<<3) | rc);
			color0 = c_data & 0x0f;
			color1 = c_data >> 4;
			block[vmli] = standard_block(g_data, color0, color1);
		}
		break;

	case (MULTI|BITMAP):
		for (vmli = 0; vmli < 40; vmli++) {
			c_data = c_buffer[vmli];
			g_data = mem_read_bitmap_base (((vc_base+vmli)<<3) | rc);
			color0 = vic_registers[0x21];
			color1 = (c_data>>4) & 0x0f;
			color2 = c_data & 0x0f;
			color3 = c_colors[vmli];
			block[vmli] = multicolor_block(g_data, color0, color1, color2, color3);
		}
		break;

	case (EXTCOLOR|CHARACTER):
		for (vmli = 0; vmli < 40; vmli++) {
			c_data = c_buffer[vmli];
			g_data = mem_read_character_base (((c_data & 0x3f)<<3) | rc);
			color0 = vic_registers[0x21 + ((c_data >> 6) & 3)];
			color1 = c_colors[vmli];
			block[vmli] = standard_block(g_data, color0, color1);
		}
		break;

	case (EXTCOLOR|MULTI|CHARACTER):
		for (vmli = 0; vmli < 40; vmli++) {
			c_data = c_buffer[vmli];
			g_data = mem_read_character_base (((c_data & 0x3f)<<3) | rc);
			if (c_colors[vmli] & 0x08)
				block[vmli] = multicolor_block(g_data, 0, 0, 0, 0);
			else
				block[vmli] = standard_block(g_data, 0, 0);
		}
		break;

	case (EXTCOLOR|BITMAP):
		for (vmli = 0; vmli < 40; vmli++) {
			g_data = mem_read_bitmap_base ((((vc_base+vmli) & 0x33f)<<3) | rc);
			block[vmli] = standard_block(g_data, 0, 0);
		}
		break;

	case (EXTCOLOR|MULTI|BITMAP):
		for (vmli = 0; vmli < 40; vmli++) {
			g_data = mem_read_bitmap_base ((((vc_base+vmli) & 0x33f)<<3) | rc);
			block[vmli] = multicolor_block(g_data, 0, 0, 0, 0);
		}
		break;

	case (IDLE|CHARACTER):
	case (IDLE|MULTI|CHARACTER):
	case (IDLE|EXTCOLOR|CHARACTER):
		color0 = vic_registers[0x21];
		g_data = mem_read_video_bank (0x3fff);
		for (vmli = 0; vmli < 40; vmli++)
			block[vmli] = standard_block(g_data, color0, 0);
		break;
	case (IDLE|BITMAP):
	case (IDLE|EXTCOLOR|MULTI|CHARACTER):
	case (IDLE|EXTCOLOR|BITMAP):
		g_data = mem_read_video_bank (0x3fff);
		for (vmli = 0; vmli < 40; vmli++)
			block[vmli] = standard_block(g_data, 0, 0);
		break;
	case (IDLE|MULTI|BITMAP):
		color0 = vic_registers[0x21];
		g_data = mem_read_video_bank (0x3fff);
		for (vmli = 0; vmli < 40; vmli++)
			block[vmli] = multicolor_block(g_data, color0, 0, 0, 0);
		break;
	case (IDLE|EXTCOLOR|MULTI|BITMAP):
		g_data = mem_read_video_bank (0x3fff);
		for (vmli = 0; vmli < 40; vmli++)
			block[vmli] = multicolor_block(g_data, 0, 0, 0, 0);
		break;
	}


	/****************** DRAW SPRITES ********************/

	for (i=0; i<8; i++) {
		int mask = (1<<i);
		int sprite_y, sprite_ptr;
		sprite[i] = 0;

		/* is this sprite enabled? */
		if (!(vic_registers[0x15] & mask)) continue;

		/* -1 is necessary because sprites are displayed 1 row late! */
		sprite_y = raster_line - vic_registers[2*i+1] - 1;
		/* is it vertically doubled? */
		if (vic_registers[0x17] & mask) sprite_y >>= 1;
		if (sprite_y < 0 || sprite_y >= 21) continue;

		sprite_ptr = (mem_read_video_matrix(0x3f8+i) << 6) + (3 * sprite_y);
		sprite[i] = (mem_read_video_bank(sprite_ptr + 2) << 0) |
			(mem_read_video_bank(sprite_ptr + 1) << 8) |
			(mem_read_video_bank(sprite_ptr + 0) << 16);
		if (!sprite[i]) continue;

		sprite[i] |= (vic_registers[0x27 + i] << 24);          /* sprite color */
		if (vic_registers[0x1b] & mask) sprite[i] |= (1<<28);  /* bg priority */
		if (vic_registers[0x1c] & mask) sprite[i] |= (1<<29);  /* multicolor */
		if (vic_registers[0x1d] & mask) sprite[i] |= (1<<30);  /* x-expand */

		render_data[raster_line].sprite_xpos[i] =
			vic_registers[2*i] | ((vic_registers[0x10] & mask) ? 0x100 : 0);
	}

	return sprite_collisions(render_data[raster_line]);
}

/**********************************************************************/
/* SPRITE COLLISION DETECTION */

int sprite_collisions( render_line data ) {
	int i, j, temp, result=0, mob_mask[8];

	/* preprocess multicolor MOBs */
	for (i=0; i<8; i++) {
		temp = data.sprite_data[i] & 0xffffff;
		if (data.sprite_data[i] & (1<<29)) {
			temp = (temp & 0x555555) | ((temp>>1) & 0x555555);
			temp |= (temp<<1);
		}
		mob_mask[i] = temp;
	}
	/* detect MOB-MOB collisions */
	for (i=0; i<7; i++) {
		if (!mob_mask[i]) continue;
		for (j=i+1; j<8; j++) {
			if (!mob_mask[j]) continue;
			/* check between sprites i, j */
			temp = data.sprite_xpos[i] - data.sprite_xpos[j];
			if ((temp > 24) || (temp < -24)) continue;
			if (mob_mask[i] & (mob_mask[j] << temp))
				result |= (1<<i) | (1<<j);
		}
	}

	/* low byte MOB-MOB; high byte MOB-DATA */
	return (result);
}


int bitmask_1( int bits, int offset, int multi ) {
	if (multi) bits = (bits & 0x55) * 3;
	bits <<= (16-offset);
	return bits;
}
int bitmask_2( int bits, int offset, int multi ) {
	if (multi) bits = (bits & 0x55) * 3;
	bits <<= (16-offset);
	return bits;
}
