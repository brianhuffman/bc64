/* keyboard.h - keyboard emulation for c64 */
/* by Brian Huffman 12-1-00 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <SDL/SDL.h>

void key_init ();

void keyboard_keydown (SDLKey key, SDLMod mod);
void keyboard_keyup (SDLKey key, SDLMod mod);

int keyboard_read_rows (int column_mask);
int keyboard_read_joy2 ();

void joystick_select (int x);
void joystick_down (int x);
void joystick_up (int x);

/* list of keys on commodore keyboard
   00-07  del, ret, right, f7, f1, f3, f5, down
   10-17  3, W, A, 4, Z, S, E, lshift
   20-27  5, R, D, 6, C, F, T, X
   30-37  7, Y, G, 8, B, H, U, V
   40-47  9, I, J, 0, M, K, O, N
   50-57  +, P, L, -, ., :, @, ,
   60-67  pound, *, ;, home, rshift, =, ^, /
   70-77  1, <, ctrl, 2, spc, C=, Q, stop
*/

typedef enum {
	C64K_DEL       = 0,
	C64K_RETURN    = 1,
	C64K_RIGHT     = 2,
	C64K_F7        = 3,
	C64K_F1        = 4,
	C64K_F3        = 5,
	C64K_F5        = 6,
	C64K_DOWN      = 7,
	C64K_3         = 8,
	C64K_w         = 9,
	C64K_a         = 10,
	C64K_4         = 11,
	C64K_z         = 12,
	C64K_s         = 13,
	C64K_e         = 14,
	C64K_LSHIFT    = 15,
	C64K_5         = 16,
	C64K_r         = 17,
	C64K_d         = 18,
	C64K_6         = 19,
	C64K_c         = 20,
	C64K_f         = 21,
	C64K_t         = 22,
	C64K_x         = 23,
	C64K_7         = 24,
	C64K_y         = 25,
	C64K_g         = 26,
	C64K_8         = 27,
	C64K_b         = 28,
	C64K_h         = 29,
	C64K_u         = 30,
	C64K_v         = 31,
	C64K_9         = 32,
	C64K_i         = 33,
	C64K_j         = 34,
	C64K_0         = 35,
	C64K_m         = 36,
	C64K_k         = 37,
	C64K_o         = 38,
	C64K_n         = 39,
	C64K_PLUS      = 40,
	C64K_p         = 41,
	C64K_l         = 42,
	C64K_MINUS     = 43,
	C64K_PERIOD    = 44,
	C64K_COLON     = 45,
	C64K_AT        = 46,
	C64K_COMMA     = 47,
	C64K_POUND     = 48,
	C64K_ASTERISK  = 49,
	C64K_SEMICOLON = 50,
	C64K_HOME      = 51,
	C64K_RSHIFT    = 52,
	C64K_EQUALS    = 53,
	C64K_ARROWUP   = 54,
	C64K_SLASH     = 55,
	C64K_1         = 56,
	C64K_ARROWLEFT = 57,
	C64K_CTRL      = 58,
	C64K_2         = 59,
	C64K_SPACE     = 60,
	C64K_COMMODORE = 61,
	C64K_q         = 62,
	C64K_STOP      = 63,
	SHIFT          = 64,
	COMM           = 128
} C64Key;

/*
commodore symbols (25 + 3 unique):
!"#$%&'()+-@*=^;:[],.<>/? {pi}{pound}{left arrow}
commodore keys with shift,cm graphic symbols (5):
+-@*{pound}
left-arrow and equals are unaffected by shift.

symbols on a PC keyboard (25 + 7 unique):
!@#$%^&*()-=+[];:'",<.>/?   ~\`_{}|


commodore keys (10 unique):
{left arrow}+-{pound}@*{^pi}={:[}{;]}
PC keys (8 unique):
{`~}{-_}{=+}[]\{;:}{'"}

ascii mapping:
underscore   -> left arrow
left brace   -> shift-plus (big-plus)
right brace  -> shift-minus (v-bar)
backslash    -> pound
vertical bar -> cm-minus (left-checker)
back quote   -> shift-asterisk (h-bar)
tilde        -> pi

desired mapping:
underscore   -> cm-at (underscore)
left brace   -> shift-pound (nw tri)
right brace  -> cm-asterisk (ne tri)
backslash    -> pound
vertical bar -> shift-minus (v-bar)
back quote   -> left arrow
tilde        -> pi

with commodore key:
left bracket  -> cm-plus (checker)
right bracket -> cm-minus (left-checker)
backslash     -> cm-pound (bottom-checker)
backquote     -> shift-asterisk (h-bar)
minus         -> shift-plus (big-plus)
equals        -> shift-at (right angle)

*/

#endif
