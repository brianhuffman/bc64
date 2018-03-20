/* bc64.c - Main module for C64 emulator */
/* by Brian Huffman 12-1-00 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include "mem_c64.h"
#include "vic2.h"
#include "video.h"
#include "keyboard.h"
#include "serial.h"
#include "6510.h"

static int paused = 0;

static FILE *open_rom_file (char *filename);
static char *get_home_path();

void handle_event (const SDL_Event *event) {
	switch (event->type) {
	case SDL_KEYDOWN:
		switch (event->key.keysym.sym) {
		case SDLK_F9: // RESTORE key
			cpu6510_nmi();
			break;
		case SDLK_F10: // refresh key
			print_state();
			paused = !paused;
			printf ("emulator %s\n", paused ? "paused" : "unpaused");
			break;
		case SDLK_F11: // reset key
			cpu6510_reset();
			break;
		case SDLK_F12: // quit key
			exit (0);
			break;
		case SDLK_PRINT:
			video_screenshot();
			break;
		case SDLK_KP8:
			joystick_down(0x01);
			break;
		case SDLK_KP2:
			joystick_down(0x02);
			break;
		case SDLK_KP4:
			joystick_down(0x04);
			break;
		case SDLK_KP6:
			joystick_down(0x08);
			break;
		case SDLK_KP0:
			joystick_down(0x10);
			break;
		case SDLK_PAGEUP:
			joystick_select(1);
			break;
		case SDLK_PAGEDOWN:
			joystick_select(2);
			break;
		default:
			break;
		}
		keyboard_keydown (event->key.keysym.sym, event->key.keysym.mod);
		break;
	case SDL_KEYUP:
		switch (event->key.keysym.sym) {
		case SDLK_KP8:
			joystick_up(0x01);
			break;
		case SDLK_KP2:
			joystick_up(0x02);
			break;
		case SDLK_KP4:
			joystick_up(0x04);
			break;
		case SDLK_KP6:
			joystick_up(0x08);
			break;
		case SDLK_KP0:
			joystick_up(0x10);
			break;
		default:
			break;
		}
		keyboard_keyup (event->key.keysym.sym, event->key.keysym.mod);
		break;
	case SDL_QUIT:
		printf("Goodbye.\n");
		exit(0);
	default:
		break;
	}
}

void callback_main (void) {
	SDL_Event event;
	static int when = 0;
	
	while (SDL_PollEvent (&event)) handle_event (&event);
	callback_frame();

	when += 63*312;
	cpu6510_callback (CB_MAIN, callback_main, when);
}

int main (int argc, char **argv)
{
	/* file pointers for rom images */
	FILE *fk, *fb, *fc, *cart;
	int j;

	fk = open_rom_file ("kernal");
	fb = open_rom_file ("basic");
	fc = open_rom_file ("chargen");
	mem_init (fk, fb, fc);
	fclose (fk);
	fclose (fb);
	fclose (fc);

	if (argc >= 2) cart = fopen(argv[1], "r");
	else cart = NULL;

	if (cart != NULL) {
		printf("opened cartridge %s.\n", argv[1]);
		mem_load_cartridge(cart);
		fclose(cart);
	}

	/* Initialize the SDL library */
	if ( SDL_Init (SDL_INIT_VIDEO) < 0 ) {
		fprintf (stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		exit (1);
	}
	atexit (SDL_Quit);

	cpu6510_reset();
	video_init();
	key_init();
	serial_init();
	
	/* setup periodic interrupts */
	cpu6510_callback (CB_MAIN, callback_main, 0);
	cpu6510_callback (CB_RASTER, callback_raster, 0);

	/* start CPU loop */
	cpu6510_main ();
	
	/*
	while (1) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) handle_event (&event);

		// 60 Hz timer set to 17045 clock cycles
		if (!paused) {
			for (j=0; j<312; j++) {
				cpu6510_main (63);
				vic_update_raster (j);
			}
			cpu6510_irq(); // timer interrupt
			callback_frame();
		}
	}
	*/
	return 0;
}

FILE *open_rom_file (char *filename)
{
	FILE *fin = NULL;
	char fullpath[128];
	strcpy (fullpath, get_home_path() );
	strcat (fullpath, "/.bc64/");
	strcat (fullpath, filename);

	fin = fopen( fullpath, "r" );
	if (fin == NULL) {
		printf("Rom image \"%s\" not found.\n", fullpath);
		exit(1);
	}
	return fin;
}

char *get_home_path() {
	char *home;
	home = getenv("HOME");
	if (home == NULL) home = ".";
	return home;
}
