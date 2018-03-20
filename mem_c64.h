/* mem_c64.h - memory interface for C64 emulator */
/* by Brian Huffman 11-30-00 */

/* memory management is described on page 260 of the C64 PRG */

#ifndef MEM_C64_H
#define MEM_C64_H

#include <stdio.h>

/* declare arrays for memory storage */
extern unsigned char ram_64k[0x10000];
extern unsigned char readable[0x10000];
extern unsigned char io_ram[0x1000];
extern unsigned char character_rom[0x1000];
extern unsigned char color_ram[0x0400];

extern unsigned char *stack;
extern unsigned char *character_base;
extern unsigned char *video_matrix;
extern unsigned char *bitmap_base;
extern unsigned char *video_bank;
extern int mem_video_matrix, mem_bitmap_base;
extern int mem_videoptr, mem_videobank;

/* declare set of flags for memory configuration */
/* bit0 = LORAM; bit1 = HIRAM; bit2 = CHAREN */
extern int mem_flags;


/***************************************/
/* static inline function declarations */
/***************************************/

static inline unsigned char mem_read(int address) {
	return (readable[address]);
}

static inline int mem_read_16(int address) {
	/* #define mem_read_16(addr) (*((short*)&mem_read(addr)) & 0xffff) */
	return (readable[address] + (readable[address+1]<<8));
}

static inline unsigned char stack_read(int address) {
	return (stack[address]);
}

static inline int stack_read_16(int address) {
	return (stack[address] + (stack[address+1]<<8));
}

static inline void stack_write(int address, int value) {
	stack[address] = value;
}

static inline void mem_io_write(int address, int value) {
	io_ram[address & 0x0fff] = value;
	if (mem_flags >= 5) readable[address] = value;
}

/******************************************/
/* inline functions for vic memory access */
/******************************************/

static inline unsigned char mem_read_color_ram(int address) {
	return color_ram[address];
}

static inline unsigned char mem_read_video_matrix(int address) {
	return video_matrix[address];
}

static inline unsigned char mem_read_character_base(int address) {
	return character_base[address];
}

static inline unsigned char mem_read_bitmap_base(int address) {
	return bitmap_base[address];
}

static inline unsigned char mem_read_video_bank(int address) {
	return video_bank[address];
}

/*****************************/
/* other function prototypes */
/*****************************/

void mem_init( FILE *fk, FILE *fb, FILE *fc );
void mem_reset();
void mem_load_cartridge( FILE *cart );

void mem_write(int address, int value);

void update_mem_flags(int new_flags);
void mem_set_video_memptr(int value);
void mem_set_video_bank(int value);

#endif
