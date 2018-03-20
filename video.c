/* video.c - video functions for c64 emulator */
/* by Brian Huffman 12-1-00 */

#include <stdio.h>
#include <stdlib.h>
#include <SDL/SDL.h>
#include "video.h"
#include "mem_c64.h"
#include "vic_redraw.h"

#define SCREEN_WIDTH 384
#define SCREEN_HEIGHT 260

/* if screen is 320x200, top-left pixel is (24,51) */
#define FIRST_VISIBLE_COLUMN (184-(SCREEN_WIDTH>>1))
#define FIRST_VISIBLE_RASTER (151-(SCREEN_HEIGHT>>1))

static SDL_Surface *screen, *shadow;
static Uint8 image[SCREEN_HEIGHT][SCREEN_WIDTH];
static render_line old_data[SCREEN_HEIGHT];

static SDL_Color palette[16] =
	{ { 0x00, 0x00, 0x00, 0 }
	, { 0xfc, 0xfc, 0xfc, 0 }
	, { 0xa8, 0x00, 0x00, 0 }
	, { 0x54, 0xfc, 0xfc, 0 }
	, { 0xa8, 0x00, 0xa8, 0 }
	, { 0x00, 0xa8, 0x00, 0 }
	, { 0x00, 0x00, 0xa8, 0 }
	, { 0xfc, 0xfc, 0x00, 0 }
	, { 0xa8, 0x54, 0x00, 0 }
	, { 0x80, 0x2c, 0x00, 0 }
	, { 0xfc, 0x54, 0x54, 0 }
	, { 0x54, 0x54, 0x54, 0 }
	, { 0x80, 0x80, 0x80, 0 }
	, { 0x54, 0xfc, 0x54, 0 }
	, { 0x54, 0x54, 0xfc, 0 }
	, { 0xa8, 0xa8, 0xa8, 0 }
	};

void video_init () {

	/* Initialize the display, in any mode */
	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, 0, SDL_SWSURFACE);
	if ( screen == NULL ) {
		fprintf(stderr, "Couldn't set %ix%i video mode: %s\n",
			SCREEN_WIDTH, SCREEN_HEIGHT, SDL_GetError());
		exit(1);
	}

	/* Initialize the shadow surface, in 8-bit palettized mode */
	shadow = SDL_CreateRGBSurfaceFrom( image, SCREEN_WIDTH, SCREEN_HEIGHT, 8,
		SCREEN_WIDTH, 0, 0, 0, 0);
	if ( shadow == NULL ) {
		fprintf(stderr, "Couldn't set %ix%i video mode: %s\n",
			SCREEN_WIDTH, SCREEN_HEIGHT, SDL_GetError());
		exit(1);
	}
	/* set the palette for the shadow surface */
	SDL_SetColors(shadow, palette, 0, 16);

	SDL_WM_SetCaption("BC64: Commodore 64 emulator", NULL);
}

void video_screenshot() {
	static int number = 0;
	char filename[20];

	sprintf(filename, "bc64_%03i.bmp", number++);
	printf("saving screenshot: %s\n", filename);
	SDL_SaveBMP(screen, filename);
}

static void video_fill_line (int y, int color) {
	SDL_Rect rect;
	rect.x = 0;
	rect.y = y;
	rect.w = SCREEN_WIDTH;
	rect.h = 1;
	SDL_FillRect(shadow, &rect, color);
	// SDL_BlitSurface(shadow, &rect, screen, &rect);
}

inline static void draw_byte(int byte, Uint8 *pixels) {
	Uint8 fg, bg, c[4];
	if (byte & (1<<31)) {
		c[0] = (byte >> 8) & 0x0f;
		c[1] = (byte >> 12) & 0x0f;
		c[2] = (byte >> 16) & 0x0f;
		c[3] = (byte >> 20) & 0x0f;
		pixels[0] = pixels[1] = c[(byte>>6) & 0x03];
		pixels[2] = pixels[3] = c[(byte>>4) & 0x03];
		pixels[4] = pixels[5] = c[(byte>>2) & 0x03];
		pixels[6] = pixels[7] = c[(byte   ) & 0x03];
	} else {
		bg = (byte >> 8) & 0x0f;
		fg = (byte >> 12) & 0x0f;
		pixels[0] = byte & 0x80 ? fg : bg;
		pixels[1] = byte & 0x40 ? fg : bg;
		pixels[2] = byte & 0x20 ? fg : bg;
		pixels[3] = byte & 0x10 ? fg : bg;
		pixels[4] = byte & 0x08 ? fg : bg;
		pixels[5] = byte & 0x04 ? fg : bg;
		pixels[6] = byte & 0x02 ? fg : bg;
		pixels[7] = byte & 0x01 ? fg : bg;
	}
}

inline static void draw_sprite_1x(int sprite, Uint8 *pixels) {
	int x, c1 = (sprite >> 24) & 0x0f;
	for (x = 0; x < 24; x++)
		if (sprite & (0x800000 >> x)) pixels[x] = c1;
}

inline static void draw_sprite_1m(int sprite, Uint8 *pixels, int c1, int c3) {
	int x, c[4], tmp;
	c[1] = c1;
	c[2] = (sprite >> 24) & 0x0f;
	c[3] = c3;
	for (x = 0; x < 24; x += 2) {
		tmp = (sprite >> (22-x)) & 3;
		if (tmp) pixels[x] = pixels[x + 1] = c[tmp];
	}
}

inline static void draw_sprite_2x(int sprite, Uint8 *pixels) {
	int x, c1 = (sprite >> 24) & 0x0f;
	for (x = 0; x < 24; x++) {
		if (sprite & (0x800000 >> x))
			pixels[2*x] = pixels[2*x + 1] = c1;
	}
}

inline static void draw_sprite_2m(int sprite, Uint8 *pixels, int c1, int c3) {
	int x, c[4], tmp;
	c[1] = c1;
	c[2] = (sprite >> 24) & 0x0f;
	c[3] = c3;
	for (x = 0; x < 24; x += 2) {
		tmp = (sprite >> (22-x)) & 3;
		if (tmp) pixels[2*x] = pixels[2*x + 1] =
			pixels[2*x + 2] = pixels[2*x + 3] = c[tmp];
	}
}

static void video_redraw_line(int y, const render_line data)
{
	int i, cx, border_left, border_right, bg_left;
	SDL_Rect rect;

	/* is screen covered by border? */
	if ( data.csel_xscroll == -1 ) {
		video_fill_line (y, data.border_color);
		return;
	}

	/* draw character data */
	bg_left = (data.csel_xscroll & 0x07) + 24 - FIRST_VISIBLE_COLUMN;
	for (cx=0; cx<40; cx++) {
		draw_byte (data.block_data[cx], &(image[y][bg_left + 8*cx]));
	}

	/* draw sprites */
	for (i=0; i<8; i++) {
		int sprite, sprite_x, c1, c2;
		sprite = data.sprite_data[i];
		if (!sprite) continue;
		sprite_x = data.sprite_xpos[i] - FIRST_VISIBLE_COLUMN;
		c1 = data.sprite_color1;
		c2 = data.sprite_color2;
		switch (sprite >> 28) {
		case 0:
			draw_sprite_1x(sprite, &(image[y][sprite_x]));
			break;
		case 1:
			draw_sprite_1x(sprite, &(image[y][sprite_x]));
			break;
		case 2:
			draw_sprite_1m(sprite, &(image[y][sprite_x]), c1, c2);
			break;
		case 3:
			draw_sprite_1m(sprite, &(image[y][sprite_x]), c1, c2);
			break;
		case 4:
			draw_sprite_2x(sprite, &(image[y][sprite_x]));
			break;
		case 5:
			draw_sprite_2x(sprite, &(image[y][sprite_x]));
			break;
		case 6:
			draw_sprite_2m(sprite, &(image[y][sprite_x]), c1, c2);
			break;
		case 7:
			draw_sprite_2m(sprite, &(image[y][sprite_x]), c1, c2);
			break;
		}
	}

	/* draw side borders */
	border_left = (data.csel_xscroll & 0x08 ? 24 : 31) - FIRST_VISIBLE_COLUMN;
	border_right = (data.csel_xscroll & 0x08 ? 344 : 335) - FIRST_VISIBLE_COLUMN;

	rect.y = y;
	rect.h = 1;
	rect.x = 0;
	rect.w = border_left;
	SDL_FillRect(shadow, &rect, data.border_color);
	rect.x = border_right;
	rect.w = SCREEN_WIDTH - border_right;
	SDL_FillRect(shadow, &rect, data.border_color);
}
/*
static void video_redraw_ifchanged(int y, const render_line new) {
	int i, changed=0;
	render_line old = old_data[y];

	if (old.border_color != new.border_color) changed = 1;
	old.border_color = new.border_color;
	if (old.csel_xscroll != new.csel_xscroll) changed = 1;
	old.csel_xscroll = new.csel_xscroll;
	if (old.sprite_color1 != new.sprite_color1) changed = 1;
	old.sprite_color1 = new.sprite_color1;
	if (old.sprite_color2 != new.sprite_color2) changed = 1;
	old.sprite_color2 = new.sprite_color2;

	for(i=0; i<8; i++) {
		if (old.sprite_data[i] != new.sprite_data[i]) changed = 1;
		old.sprite_data[i] = new.sprite_data[i];
		if (old.sprite_xpos[i] != new.sprite_xpos[i]) changed = 1;
		old.sprite_xpos[i] = new.sprite_xpos[i];
	}
	for(i=0; i<40; i++) {
		if (old.block_data[i] != new.block_data[i]) changed = 1;
		old.block_data[i] = new.block_data[i];
	}

	if (changed) video_redraw_line(y, new);
}
*/
void video_draw_sdl_screen() {
	int i;
	const render_line *render_data = vic_get_render_data();

	for (i=0; i<SCREEN_HEIGHT; i++) {
		video_redraw_line(i, render_data[FIRST_VISIBLE_RASTER + i]);
	}

	SDL_BlitSurface(shadow, NULL, screen, NULL);
	SDL_UpdateRect(screen, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

static int time_left(void) {
	static int next_time = 0;
	int now;

	now = SDL_GetTicks();
	if ( next_time <= now - 100 ) {
		printf("%i ms slow!\n", now-next_time);
		next_time = now+20;
		return(0);
	}
	/*
	else if ( next_time <= now ) {
		next_time += 20;
		return(0);
	}
	*/
	next_time += 20;
	return (next_time - now - 20);
}

void callback_frame (void) {
	static int skips_left = 0, skip_frames = 9, skip_auto = 1;
	static int total_frames = 0, total_drawn = 0, total_delayed = 0;
	static int next_time = 0;
	int now, remaining;


	/* wait for next 50th of a second */
	remaining = next_time - SDL_GetTicks();
	if (remaining < -100) {
		printf("%i ms slow!\n", -remaining);
		next_time -= remaining;
		remaining = 0;
	}
	next_time += 20;

	if (remaining > 40) {
		now = SDL_GetTicks();
		SDL_Delay (remaining - 20);
		total_delayed += SDL_GetTicks() - now;
	}
	if (remaining >= 0 && skip_auto) skips_left = 0;

	/* draw the screen, if there is time */
	if (skips_left-- <= 0) {
		video_draw_sdl_screen();
		total_drawn++;
		skips_left = skip_frames;
	}
	
	/* print framerate statistics */	
	if (++total_frames == 50) {
		printf("%i frames/sec, ", total_drawn);
		printf("%i percent idle\n", total_delayed/10);
		total_drawn = 0;
		total_delayed = 0;
		total_frames = 0;
	}
}