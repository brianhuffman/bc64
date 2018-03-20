/* keyboard.c - keyboard emulation for c64 */
/* will also handle joystick, eventually */
/* by Brian Huffman 12-1-00 */

#include <stdio.h>
#include <stdlib.h>
#include "keyboard.h"
#include "video.h"
#include "cia1.h"
#include "mem_c64.h"
#include "6510.h"
#include <SDL/SDL.h>


static int keymap[SDLK_LAST];
static int key_rows_data[8];
//static int joystick_data[2];
static int joy_data = 0xff;
static int select_joystick = 2;

static int positional = 0;
/*
key mapping types:
- unmapped
- direct map to a C64 Key
- map to a shifted C64 Key
- modifier-dependent mapping
- map to a joystick switch

unmapped = -1
direct map is given by a code < 256.
modifier-dependent has (normal, shift, comm) at bit positions (0,8,16)
joystick switches are specified in bits 24-28.

typedef struct keymap_s {
	unsigned int key_row : 3,
	unsigned int key_col : 3,
	unsigned int key_row_sh : 3,
	unsigned int key_col_sh : 3,
	unsigned int key_row_cm : 3,
	unsigned int key_col_cm : 3,
	unsigned int : ,
	unsigned int : ,
	unsigned int : ,
	unsigned int : ,
	unsigned int joy_row : 3,
	unsigned int is_joy : 1,
	unsigned int is_key : 1,
} keymap_t;

*/

inline static void press(int code) {
	key_rows_data[(code >> 3) & 7] &= ~(1 << (code & 7));
}
inline static void release(code) {
	key_rows_data[(code >> 3) & 7] |= (1 << (code & 7));
}

static void show() {
	int i;
	for (i=0;i<8;i++) printf("%02x ",key_rows_data[i]);
	printf("\n");
}

void keyboard_keydown (SDLKey key, SDLMod mod) {
	int code = keymap[key];

	if (code < 0) return;
	
	if (code > 255) {
		if (mod & KMOD_ALT) code >>= 16;
		else if (mod & KMOD_SHIFT) code >>= 8;
		
		if (!(code & SHIFT)) {
			release(C64K_LSHIFT);
			release(C64K_RSHIFT);
		}
		if (!(code & COMM)) {
			release(C64K_COMMODORE);
		}
	}
	//printf("pressing key %i\n", code & 255);
	press(code);
	if (code & SHIFT) press(C64K_LSHIFT);
	if (code & COMM) press(C64K_COMMODORE);
	//show();
}

void keyboard_keyup (SDLKey key, SDLMod mod) {
	int code = keymap[key];
	
	if (code < 0) return;
	
	if (code > 255) {
		release(code >> 16);
		release(code >> 8);
	}
	if (code > 63) {
		if (mod & KMOD_LSHIFT) press(C64K_LSHIFT);
		else release(C64K_LSHIFT);
		if (mod & KMOD_RSHIFT) press(C64K_RSHIFT);
		else release(C64K_RSHIFT);
		if (mod & KMOD_ALT) press(C64K_COMMODORE);
		else release(C64K_COMMODORE);
	}	

	release(code);
	//show();
}

void joystick_select (int x) {
	printf ("joystick %i\n", x);
	if (x == 1) select_joystick = 1;
	if (x == 2) select_joystick = 2;
}

void joystick_down (int x) {
	joy_data &= ~x;
	if (select_joystick == 1) cia1_set_joysticks(joy_data, 0xff);
	else cia1_set_joysticks(0xff, joy_data);
}

void joystick_up (int x) {
	joy_data |= x;
	if (select_joystick == 1) cia1_set_joysticks(joy_data, 0xff);
	else cia1_set_joysticks(0xff, joy_data);
}

#define keys(a,b,c) ((a) + ((b) << 8)) + ((c) << 16)

void key_init () {
	int i;
	
	cia1_set_joysticks(0xff,0xff);

	for (i=0; i<8; i++) key_rows_data[i] = 255;
	
	/* positional key mappings */
	for (i=0; i<SDLK_LAST; i++) keymap[i] = -1;

	keymap[SDLK_ESCAPE]       = C64K_STOP;
	keymap[SDLK_TAB]          = C64K_HOME;
	keymap[SDLK_BACKSPACE]    = C64K_DEL;
	keymap[SDLK_RETURN]       = C64K_RETURN;
	keymap[SDLK_RIGHT]        = C64K_RIGHT;
	keymap[SDLK_LEFT]         = SHIFT + C64K_RIGHT;
	keymap[SDLK_DOWN]         = C64K_DOWN;
	keymap[SDLK_UP]           = SHIFT + C64K_DOWN;
	keymap[SDLK_LSHIFT]       = SHIFT + C64K_LSHIFT;
	keymap[SDLK_RSHIFT]       = SHIFT + C64K_RSHIFT;
	keymap[SDLK_LCTRL]        = C64K_CTRL;
	keymap[SDLK_LALT]         = C64K_COMMODORE;
	keymap[SDLK_F1]           = C64K_F1;
	keymap[SDLK_F2]           = SHIFT + C64K_F1;
	keymap[SDLK_F3]           = C64K_F3;
	keymap[SDLK_F4]           = SHIFT + C64K_F3;
	keymap[SDLK_F5]           = C64K_F5;
	keymap[SDLK_F6]           = SHIFT + C64K_F5;
	keymap[SDLK_F7]           = C64K_F7;
	keymap[SDLK_F8]           = SHIFT + C64K_F7;

	keymap[SDLK_BACKQUOTE]    = C64K_ARROWLEFT;
	keymap[SDLK_MINUS]        = C64K_MINUS;
	keymap[SDLK_EQUALS]       = C64K_PLUS;
	keymap[SDLK_LEFTBRACKET]  = C64K_AT;
	keymap[SDLK_RIGHTBRACKET] = C64K_ARROWUP;
	keymap[SDLK_BACKSLASH]    = C64K_POUND;
	keymap[SDLK_SEMICOLON]    = C64K_COLON;
	keymap[SDLK_QUOTE]        = C64K_SEMICOLON;
	keymap[SDLK_COMMA]        = C64K_COMMA;
	keymap[SDLK_PERIOD]       = C64K_PERIOD;
	keymap[SDLK_SLASH]        = C64K_SLASH;
	keymap[SDLK_SPACE]        = C64K_SPACE;

	keymap[SDLK_0] = C64K_0;
	keymap[SDLK_1] = C64K_1;
	keymap[SDLK_2] = C64K_2;
	keymap[SDLK_3] = C64K_3;
	keymap[SDLK_4] = C64K_4;
	keymap[SDLK_5] = C64K_5;
	keymap[SDLK_6] = C64K_6;
	keymap[SDLK_7] = C64K_7;
	keymap[SDLK_8] = C64K_8;
	keymap[SDLK_9] = C64K_9;
	keymap[SDLK_a] = C64K_a;
	keymap[SDLK_b] = C64K_b;
	keymap[SDLK_c] = C64K_c;
	keymap[SDLK_d] = C64K_d;
	keymap[SDLK_e] = C64K_e;
	keymap[SDLK_f] = C64K_f;
	keymap[SDLK_g] = C64K_g;
	keymap[SDLK_h] = C64K_h;
	keymap[SDLK_i] = C64K_i;
	keymap[SDLK_j] = C64K_j;
	keymap[SDLK_k] = C64K_k;
	keymap[SDLK_l] = C64K_l;
	keymap[SDLK_m] = C64K_m;
	keymap[SDLK_n] = C64K_n;
	keymap[SDLK_o] = C64K_o;
	keymap[SDLK_p] = C64K_p;
	keymap[SDLK_q] = C64K_q;
	keymap[SDLK_r] = C64K_r;
	keymap[SDLK_s] = C64K_s;
	keymap[SDLK_t] = C64K_t;
	keymap[SDLK_u] = C64K_u;
	keymap[SDLK_v] = C64K_v;
	keymap[SDLK_w] = C64K_w;
	keymap[SDLK_x] = C64K_x;
	keymap[SDLK_y] = C64K_y;
	keymap[SDLK_z] = C64K_z;
/*
	keymap[SDLK_KP0] = C64K_0;
	keymap[SDLK_KP1] = C64K_1;
	keymap[SDLK_KP2] = C64K_2;
	keymap[SDLK_KP3] = C64K_3;
	keymap[SDLK_KP4] = C64K_4;
	keymap[SDLK_KP5] = C64K_5;
	keymap[SDLK_KP6] = C64K_6;
	keymap[SDLK_KP7] = C64K_7;
	keymap[SDLK_KP8] = C64K_8;
	keymap[SDLK_KP9] = C64K_9;
*/
	keymap[SDLK_KP_PERIOD]   = C64K_PERIOD;
	keymap[SDLK_KP_DIVIDE]   = C64K_SLASH;
	keymap[SDLK_KP_MULTIPLY] = C64K_ASTERISK;
	keymap[SDLK_KP_MINUS]    = C64K_MINUS;
	keymap[SDLK_KP_PLUS]     = C64K_PLUS;
	keymap[SDLK_KP_ENTER]    = C64K_RETURN;
	keymap[SDLK_KP_EQUALS]   = C64K_EQUALS;

	if (!positional) {
		keymap[SDLK_BACKQUOTE]
			= keys(C64K_ARROWLEFT, SHIFT+C64K_ARROWUP, SHIFT+C64K_AT);
		keymap[SDLK_MINUS]
			= keys(C64K_MINUS, COMM+C64K_AT, SHIFT+C64K_ASTERISK);
		keymap[SDLK_EQUALS]
			= keys(C64K_EQUALS, C64K_PLUS, SHIFT+C64K_PLUS);
		keymap[SDLK_LEFTBRACKET]
			= keys(SHIFT+C64K_COLON, SHIFT+C64K_POUND, COMM+C64K_PLUS);
		keymap[SDLK_RIGHTBRACKET]
			= keys(SHIFT+C64K_SEMICOLON, COMM+C64K_ASTERISK, COMM+C64K_MINUS);
		keymap[SDLK_BACKSLASH]
			= keys(C64K_POUND, SHIFT+C64K_MINUS, COMM+C64K_POUND);
		keymap[SDLK_SEMICOLON]
			= keys(C64K_SEMICOLON, C64K_COLON, COMM+C64K_COLON);
		keymap[SDLK_QUOTE]
			= keys(SHIFT+C64K_7, SHIFT+C64K_2, COMM+C64K_SEMICOLON);
		keymap[SDLK_2] = keys(C64K_2, C64K_AT, COMM+C64K_2);
		keymap[SDLK_6] = keys(C64K_6, C64K_ARROWUP, COMM+C64K_6);
		keymap[SDLK_7] = keys(C64K_7, SHIFT+C64K_6, COMM+C64K_7);
		keymap[SDLK_8] = keys(C64K_8, C64K_ASTERISK, COMM+C64K_8);
		keymap[SDLK_9] = keys(C64K_9, SHIFT+C64K_8, COMM+C64K_9);
		keymap[SDLK_0] = keys(C64K_0, SHIFT+C64K_9, COMM+C64K_0);
	}
}

/*
void key_main_sdl () {
	int numkeys, i, row, col;
	int joy_data;

	// read joystick position
	joy_data = 0xff;
	if (key_state[SDLK_KP8]) joy2_data &= ~0x01;
	if (key_state[SDLK_KP2]) joy2_data &= ~0x02;
	if (key_state[SDLK_KP4]) joy2_data &= ~0x04;
	if (key_state[SDLK_KP6]) joy2_data &= ~0x08;
	if (key_state[SDLK_KP0]) joy2_data &= ~0x10;

	if (select_joystick == 1) cia1_set_joysticks(joy_data, 0xff);
	else cia1_set_joysticks(0xff, joy_data);
}
*/

int keyboard_read_rows (int column_mask) {
	int col, rows = 0xff;
	
	for (col=0; col<8; col++) {
		/* is that bit a zero in the column mask? */
		if ( ((column_mask >> col) & 1) == 0) rows &= key_rows_data[col];
	}

	//printf("keyboard_read_rows(%02x) = %02x\n", column_mask, rows);
	return (rows);
}


