/* mem_c64.c - memory interface for C64 emulator */
/* by Brian Huffman 11-30-00 */

/* memory management is described on page 260 of the C64 PRG */

#include <stdio.h>
#include <string.h>
#include "mem_c64.h"
#include "keyboard.h"
#include "vic2.h"
#include "cia1.h"

#undef MEM_DEBUG

/* ROM images */
unsigned char kernal_rom[0x2000];
unsigned char basic_rom[0x2000];

/* 256 pages of 256 bytes each; this table marks which are ordinary RAM */
#define PAGE_ZERO          (1<<0)
#define PAGE_IO_RAM        (1<<1)
#define PAGE_ROM           (1<<2)

int ram_page_flag[0x100];

/*
  Reading from memory should be extremely fast.
  The 'readable' array is maintained so that there
  is no need for slow lookup tables, etc.
*/

void mem_init( FILE *fk, FILE *fb, FILE *fc ) {
	/* load rom images */
	fread (kernal_rom, 0x2000, 1, fk);
	fread (basic_rom, 0x2000, 1, fb);
	fread (character_rom, 0x1000, 1, fc);

	/* patch kernal: put 0x02 at start of all highlevel routines */
	/* wait for key press */
	// kernal_rom[0xe5cd & 0x1fff] = 0x02;
	/* copy screen line */
	// kernal_rom[0xe9d4 & 0x1fff] = 0x02;
	/* read from serial port */
	kernal_rom[0xee13 & 0x1fff] = 0x02;
	/* write to serial port */
	kernal_rom[0xed40 & 0x1fff] = 0x02;

	mem_reset();
}

void mem_reset() {
	int i;

	/* clear all memory */
	for (i=0; i< 0x10000; i++) readable[i] = ram_64k[i] = 0x00;
	for (i=0; i< 0x1000; i++) io_ram[i] = 0x00;
	for (i=0; i< 0x0400; i++) color_ram[i] = 0x00;

	/* initialize flags, ram_page_flag array */
	for (i=0; i<256; i++) ram_page_flag[i] = 0;
	ram_page_flag[0] = PAGE_ZERO;

	/* initialize pointer to stack page */
	stack = readable + 0x100;

	/* set HIRAM, LORAM, clear CHAREN */
	/* load rom images into 'readable' */
	mem_flags = 0;
	update_mem_flags( 7 );

	/* set video access to first 16K of memory */
	mem_write(0xdd00, 0);
}

void mem_load_cartridge (FILE *cart) {
	int flags = mem_flags;
	update_mem_flags(0);

	printf("loading cartridge image...\n");

	/* load cartridge into memory, if necessary */
	if (cart != NULL) {
		fread (readable + 0x8000, 0x8000, 1, cart);
		fseek (cart, 0, SEEK_SET);
		fread (ram_64k + 0x8000, 0x8000, 1, cart);
	}
	update_mem_flags(flags);
}

/******************* MEMORY READ *************************/

static unsigned char mem_read_io(int address) {

	if (address < 0xd400) /* VIC video controller */
		return vic_mem_read(address & 0x3f);
	else if (address < 0xd800) /* SID sound synthesizer */
		return readable[address];
	else if (address < 0xdc00) /* Color RAM */
		return readable[address];
	else if (address < 0xdd00) /* CIA1 Keyboard */
		return readable[address];
	else if (address < 0xde00) /* CIA2 Serial Bus */
		return readable[address];
	else return 0xff; /* Disconnected */
}

unsigned char mem_read_slow(int address) {

	/* does the address reside in IO RAM? */
	if (ram_page_flag[address >> 8] & PAGE_IO_RAM)
		return mem_read_io(address);
	else
		return readable[address];
}

/******************* MEMORY WRITE ************************/

static void mem_write_io(int address, int value) {
	/* select which chip to use:
	D000-D3FF   VIC (Video Controller)                1 K Bytes
	D400-D7FF   SID (Sound Synthesizer)               1 K Bytes
	D800-DBFF   Color RAM                             1 K Nybbles
	DC00-DCFF   CIA1 (Keyboard)                       256 Bytes
	DD00-DDFF   CIA2 (Serial Bus, User Port/RS-232)   256 Bytes
	DE00-DEFF   Open I/O slot #1 (CP/M Enable)        256 Bytes
	DF00-DFFF   Open I/O slot #2 (Disk)               256 Bytes
	*/

	if (address < 0xd400) {/* VIC video controller */
		vic_mem_write(address, value);
	}
	else if (address < 0xd800) { /* SID sound synthesizer */
		readable[address] = 0xff;
	}
	else if (address < 0xdc00) { /* Color RAM */
		readable[address] = io_ram[address & 0x0fff] = value | 0xf0;
		color_ram[address & 0x3ff] = value & 0x0f;
	}
	else if (address < 0xdd00) { /* CIA1 Keyboard */
		cia1_mem_write (address, value);
	}
	else if (address < 0xde00) { /* CIA2 Serial Bus */
		readable[address] = value;
		if (address == 0xdd00) mem_set_video_bank(value);
#ifdef MEM_DEBUG
		else fprintf(stderr,
			"CIA 2 accessed at $%04x, set to #$%02x\n",
			address, value);
#endif
	}
	else { /* Disconnected */
		readable[address] = 0xff;
	}
}

void mem_write(int address, int value) {

	int page_flag = ram_page_flag[address >> 8];

	/*
	Writing to memory does extra work to ensure that it
	always updates the 'readable' array after each write.
	*/

	/* does the address reside in ordinary RAM? */
	if (!page_flag) {
		/* if so, then put it in readable */
		readable[address] = ram_64k[address] = value;
		return;
	}

	/* if we made it this far, the memory must be somehow special */

	/* writes always go to underlying ram, except in I/O address space */
	if ( !(page_flag & PAGE_IO_RAM) ) {
		ram_64k[address] = value;
		/* put in readable unless in ROM memory */
		if ( !(page_flag & PAGE_ROM) ) {
			readable[address] = value;
		}
		/* are they changing the memory map? */
		if (address == 0x0001) {
			update_mem_flags(value & 0x07);
			return;
		}
	} else {
		/* we are in the I/O address space */
		mem_write_io(address, value);
	}
}


/********************** VIDEO MEMORY CONFIGURATION *****************/

/* video bank    starts on 0x4000 boundary */
/* video matrix  starts on 0x0400 boundary (bit 7-4) */
/* bitmap        starts on 0x2000 boundary (bit 3)   */
/* character map starts on 0x0800 boundary (bit 3-1) */

void mem_set_video_memptr(int value) {
	int char_base;
	/* D018   VIC Memory Control Register */

	/* return immediately if nothing changed */
	if (mem_videoptr == value &&
		mem_videobank == (mem_bitmap_base & 0xc000) )
		return;

	mem_videoptr = value;

	/* do video matrix memory pointer */
	mem_video_matrix = mem_videobank + ((value << 6) & (0x4000 - 0x0400));
	video_matrix = ram_64k + mem_video_matrix;

	/* do bitmap memory pointer */
	mem_bitmap_base = mem_videobank + ((value << 10) & (0x4000 - 0x2000));
	bitmap_base = ram_64k + mem_bitmap_base;

	/* do character memory pointer */
	char_base = mem_videobank + ((value << 10) & (0x4000 - 0x0800));

	/* character rom images located at $1x00 and $9x00 */
	character_base =
		((char_base & 0x7000) == 0x1000) ?
		character_rom + (char_base & 0x0fff) :
		ram_64k + char_base;

#ifdef MEM_DEBUG
	if ((char_base & 0x7000) == 0x1000)
		printf("reading characters from CHARGEN ROM\n");
	printf("character base = $%04x\n", char_base);
	printf("bitmap base = $%04x\n", mem_bitmap_base);
	printf("video matrix = $%04x\n", mem_video_matrix);
#endif

}

void mem_set_video_bank(int value) {
	int new_videobank = ((~value) & 0x03) << 14;

	if (new_videobank == mem_videobank) return;

	mem_videobank = new_videobank;
	video_bank = ram_64k + mem_videobank;
	mem_set_video_memptr(mem_videoptr);
}


/************************* ROM CONFIGURATION **********************/

void update_mem_flags(int new_flags) {
	const int load_basic[8]  = {0,0,0,1,0,0,0,1};
	const int load_kernal[8] = {0,0,1,1,0,0,1,1};
	const int load_io[8]     = {0,0,0,0,0,1,1,1};
	const int load_char[8]   = {0,1,1,1,0,0,0,0};
	int page;

	if (new_flags == mem_flags) return;

#ifdef MEM_DEBUG
	printf("Memory configuration flags updated: %i\n", new_flags);
#endif

	/* should BASIC be loaded? */
	if ( load_basic[new_flags] && !load_basic[mem_flags] ) {
		/* copy basic rom into location a000 */
#ifdef MEM_DEBUG
		printf("BASIC loaded\n");
#endif
		memcpy (readable+0xa000, basic_rom, 0x2000);
		for (page = 0xa0; page < 0xc0; page++)
			ram_page_flag[page] |= PAGE_ROM;
	}
	/* should Kernal be loaded? */
	if ( load_kernal[new_flags] && !load_kernal[mem_flags] ) {
		/* copy kernal rom into location e000 */
#ifdef MEM_DEBUG
		printf("KERNAL loaded\n");
#endif
		memcpy (readable+0xe000, kernal_rom, 0x2000);
		for (page = 0xe0; page < 0x100; page++)
			ram_page_flag[page] |= PAGE_ROM;
	}
	/* should I/O be loaded? */
	if ( load_io[new_flags] && !load_io[mem_flags] ) {
#ifdef MEM_DEBUG
		printf("I/O loaded\n");
#endif
		memcpy (readable+0xd000, io_ram, 0x1000);
		for (page = 0xd0; page < 0xe0; page++) {
			ram_page_flag[page] |= PAGE_IO_RAM;
			ram_page_flag[page] &= ~(PAGE_ROM);
		}
	}
	/* should CHARGEN be loaded? */
	if ( load_char[new_flags] && !load_char[mem_flags] ) {
#ifdef MEM_DEBUG
		printf("CHARGEN loaded\n");
#endif
		memcpy (readable+0xd000, character_rom, 0x1000);
		for (page = 0xd0; page < 0xe0; page++) {
			ram_page_flag[page] |= PAGE_ROM;
			ram_page_flag[page] &= ~(PAGE_IO_RAM);
		}
	}

	/* should BASIC be unloaded? */
	if ( !load_basic[new_flags] && load_basic[mem_flags] ) {
		/* copy ram into location a000 */
#ifdef MEM_DEBUG
		printf("BASIC unloaded\n");
#endif
		memcpy( readable+0xa000, ram_64k+0xa000, 0x2000);
		for (page = 0xa0; page < 0xc0; page++)
			ram_page_flag[page] &= ~(PAGE_ROM);
	}
	/* should Kernal be unloaded? */
	if ( !load_kernal[new_flags] && load_kernal[mem_flags] ) {
		/* copy ram into location e000 */
#ifdef MEM_DEBUG
		printf("KERNAL unloaded\n");
#endif
		memcpy( readable+0xe000, ram_64k+0xe000, 0x2000);
		for (page = 0xc0; page < 0x100; page++)
			ram_page_flag[page] &= ~(PAGE_ROM);
	}
	/* should IO/C be unloaded? */
	if ( !load_io[new_flags] && !load_char[new_flags] ) {
		/* copy ram into location d000 */
#ifdef MEM_DEBUG
		printf("IO/CHAR unloaded\n");
#endif
		memcpy( readable+0xd000, ram_64k+0xd000, 0x1000);
		for (page = 0xc0; page < 0x100; page++)
			ram_page_flag[page] &= ~(PAGE_ROM | PAGE_IO_RAM);
	}

	/* update flag values */
	mem_flags = new_flags;
}
