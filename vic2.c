/* vic2.c - video functions for c64 emulator */
/* by Brian Huffman 12-14-00 */

/* this file should be totally portable--
   system specific stuff goes in video.c */

#include <stdio.h>
#include "video.h"
#include "vic2.h"
#include "vic_redraw.h"
#include "mem_c64.h"
#include "6510.h"

#undef VIC_DEBUG

/* declare list of possible video modes */
/* (MCM) + 2(BMM) + 4(ECM) */
#define MCM (1)
#define BMM (2)
#define ECM (4)

/* list of disconnected addresses */
const int disconnect[0x40] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00,
	0x01, 0x70, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
	0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/* declare variables to hold register contents, etc */
static int vic_registers[0x40];
static int raster_compare;
static int current_raster;
static int video_mode;

static int border_top, border_bottom;

/********************************************************************/
/******************** REGISTER MEMORY WRITE *************************/
/********************************************************************/

void vic_set_register(int address, int data) {
	int offset;
	vic_registers[address] = data & (~disconnect[address]);
	for (offset = 0xd000 + address; offset < 0xd400; offset += 0x40) {
		mem_io_write(offset, data);
	}
}

void vic_mem_write(int address, int data) {

	/* address space only covers 6 bits */
	address &= 0x3f;

	/* do any special functions that need to be taken care of */
	switch (address) {

	/* Multi-purpose register */
	case 0x11:
		/* bit 7: MSB of Raster Counter */
		if (data & 0x80) raster_compare |= 0x100;
		else raster_compare &= 0xff;

		/* set bit 7 to match current raster */
		if (current_raster & 0x100) data |= 0x80;
		else data &= 0x7f;

		/* bit 6: Extended Color Mode */
		if (data & 0x40) video_mode |= ECM;
		else video_mode &= ~ECM;

		/* bit 5: Bit Map Mode */
		if (data & 0x20) video_mode |= BMM;
		else video_mode &= ~BMM;

		/* bit 3: Row SELect */
		border_top    = (data & 0x08) ? 0x33 : 0x37;
		border_bottom = (data & 0x08) ? 0xfb : 0xf7;
		break;

	/* Raster Compare (lower 8 bits) */
	case 0x12:
		raster_compare = (raster_compare & 0x100) + data;
		data = current_raster &0xff;
		break;

	/* Multi-purpose register */
	case 0x16:
		/* bit 4: Multi Color Mode */
		if (data & 0x10) video_mode |= MCM;
		else video_mode &= ~MCM;
		break;

	case 0x18: /* VIC memory pointers */
		mem_set_video_memptr(data);
		break;

	case 0x19: /* interrupt latches */
		/* clear latch when a 1 is written */
		data = vic_registers[0x19] & ~data;
		break;
	}

#ifdef VIC_DEBUG
	switch (address) {
	case 0x15: case 0x17:
		break;
	case 0x11:
		printf("video mode = %i\n", video_mode);
		printf("vertical scroll = %i\n", data & 7);
	case 0x12:
		printf("raster compare set to %i\tline = %i\n", raster_compare,
			current_raster);
		break;
	case 0x16:
		printf("video mode = %i\n", video_mode);
		printf("h scroll set to %02x\tline = %i\n", data & 7, current_raster);
		break;
	case 0x18:
		printf("memory pointers set to %02x\tline = %i\n", data & 0xfe,
			current_raster);
		break;
	case 0x19:
		printf("interrupt latch set to %02x\tline = %i\n", data, current_raster);
		break;
	case 0x1a:
		printf("interrupt mask set to %02x\tline = %i\n", data, current_raster);
		break;
	default:
		if ( address >= 0x11 && address <= 0x1a)
			fprintf(stdout, "$d0%02x = $%02x\tline = %i\n",
				address, data, current_raster);
	}
#endif

	/* write value into register */
	vic_registers[address] = data & (~disconnect[address]);

	/* write 16 copies of registers into io space */
	data |= disconnect[address];
	vic_set_register(address, data);
}

/******************** REGISTER MEMORY READ *************************/

unsigned char vic_mem_read(int address) {
	int data;
	address &= 0x3f;

	switch (address) {
	case 0x11: /* MSB of raster */
		data = current_raster & 0x100
			 ? vic_registers[0x11] | 0x80
			 : vic_registers[0x11] & 0x7f;
		return data;
	case 0x12: /* raster */
		data = current_raster & 0xff;
		return data;
	case 0x13: /* light pen x */
		return 0;
	case 0x14: /* light pen y */
		return 0;
	case 0x1e: /* sprite-sprite collision */
	case 0x1f: /* sprite-data collision */
		data = vic_registers[address];
		vic_registers[address] = 0;
		return data;
	default:
		return (vic_registers[address] | disconnect[address]);
	}
}

/******************** INITIALIZATION ********************/
void vic_init () {
	int x;
	for (x=0; x<0x2f; x++) vic_mem_write(x, 0);
	current_raster = 0;
}


/****************** RASTER UPDATE *********************/

void vic_update_raster(int value) {

	/* redraw previous line */
	//vic_redraw_screen_line(value, &video_mode, vic_registers);

	current_raster = value;
#ifdef VIC_DEBUG
	if (value == 0) printf(".");
#endif

	/* update register with current raster value */
	if (current_raster & 0x100)
		vic_set_register (0x11, vic_registers[0x11] | 0x80);
	else
		vic_set_register (0x11, vic_registers[0x11] & 0x7f);

	vic_set_register (0x12, current_raster & 0xff);

	/* set latch if raster matches compare value */
	if (current_raster == raster_compare) {
#ifdef VIC_DEBUG
		if (!(vic_registers[0x19] & 0x01))
			printf("latch set at raster = %i\n", current_raster);
#endif
		/* set raster compare latch */
		vic_set_register (0x19, vic_registers[0x19] | 0x01);
	}

	/* cause an interrupt if the value is not masked */
	if ( (vic_registers[0x19] & 0x01) &&
		(vic_registers[0x1a] & 0x01) ) {
		/* set IRQ latch */
		vic_set_register (0x19, vic_registers[0x19] | 0x80);
#ifdef VIC_DEBUG
		printf("IRQ generated at raster = %i\n", current_raster);
#endif
		cpu6510_irq();
	}
}

void callback_redraw (void) {
	vic_redraw_screen_line(current_raster, &video_mode, vic_registers);
}

void callback_raster (void) {
	static int raster = 0, when = 0;
	
	vic_update_raster (raster++);
	if (raster == 312) raster = 0;
	
	/* miner 2049'er: offset <  55 */
	/* outrun       : offset > 4 */
	/* quest        : offset > 3 */
	cpu6510_callback (CB_REDRAW, callback_redraw, when+8);
	when += 63;
	cpu6510_callback (CB_RASTER, callback_raster, when);
}

void vic_print_state() {
}
/*
typedef struct {
	unsigned int pos_y    : 8,
	unsigned int pos_x    : 9,
	unsigned int enable   : 1,
	unsigned int expand_y : 1,
	unsigned int expand_x : 1,
	unsigned int priority : 1,
	unsigned int multi    : 1,
	unsigned int color    : 4;
} mob_data;

typedef struct {
	mob_data sprite[8];
	int raster_compare;
	int raster_register;
	int pen_x : 8;
	int pen_y : 8;
	int mob_m : 8;
	int mob_d : 8; 
	int b0c : 4;
	int b1c : 4;
	int b2c : 4;
	int b3c : 4;
	int ec : 4; 
	int mm0 : 4;
	int mm1 : 4;
	int bmm : 1;
	int ecm : 1;
	int den : 1;
	int rsel : 1;
	int y : 3;
	int res : 1;
	int mcm : 1;
	int csel : 1;
	int x : 3;
	
	
} vic_state;

  | 24 ($18) | VM13 VM12 VM11 VM10 CB13 CB12 CB11  -    Memory Pointers   |
  | 25 ($19) | IRQ   -    -    -   ILP  IMMC IMBC IRST  Interrupt Register|
  | 26 ($1A) |  -    -    -    -   ELP  EMMC EMBC ERST  Enable Interrupt  |
*/

/* VIC memory map

  +----------+------------------------------------------------------------+
  | ADDRESS  | DB7  DB6  DB5  DB4  DB3  DB2  DB1  DB0     DESCRIPTION     |
  +----------+------------------------------------------------------------+
  | 00 ($00) | M0X7 M0X6 M0X5 M0X4 M0X3 M0X2 M0X1 M0X0  MOB 0 X-position  |
  | 01 ($01) | M0Y7 M0Y6 M0Y5 M0Y4 M0Y3 M0Y2 M0Y1 M0Y0  MOB 0 Y-position  |
  | 02 ($02) | M1X7 M1X6 M1X5 M1X4 M1X3 M1X2 M1Xl M1X0  MOB 1 X-position  |
  | 03 ($03) | M1Y7 M1Y6 M1Y5 M1Y4 M1Y3 M1Y2 M1Y1 M1Y0  MOB 1 Y-position  |
  | 04 ($04) | M2X7 M2X6 M2X5 M2X4 M2X3 M2X2 M2X1 M2X0  MOB 2 X-position  |
  | 05 ($05) | M2Y7 M2Y6 M2Y5 M2Y4 M2Y3 M2Y2 M2Y1 M2Y0  MOB 2 Y-position  |
  | 06 ($06) | M3X7 M3X6 M3X5 M3X4 M3X3 M3X2 M3X1 M3X0  MOB 3 X-position  |
  | 07 ($07) | M3Y7 M3Y6 M3Y5 M3Y4 M3Y3 M3Y2 M3Y1 M3Y0  MOB 3 Y-position  |
  | 08 ($08) | M4X7 M4X6 M4X5 M4X4 M4X3 M4X2 M4X1 M4X0  MOB 4 X-position  |
  | 09 ($09) | M4Y7 M4Y6 M4Y5 M4Y4 M4Y3 M4Y2 M4Y1 M4Y0  MOB 4 Y-position  |
  | 10 ($0A) | M5X7 M5X6 M5X5 M5X4 M5X3 M5X2 M5X1 M5X0  MOB 5 X-position  |
  | 11 ($0B) | M5Y7 M5Y6 M5Y5 M5Y4 M5Y3 M5Y2 M5Y1 M5Y0  MOB 5 Y-position  |
  | 12 ($0C) | M6X7 M6X6 M6X5 M6X4 M6X3 M6X2 M6X1 M6X0  MOB 6 X-position  |
  | 13 ($0D) | M6Y7 M6Y6 M6Y5 M6Y4 M6Y3 M6Y2 M6Y1 M6Y0  MOB 6 Y-position  |
  | 14 ($0E) | M7X7 M7X6 M7X5 M7X4 M7X3 M7X2 M7Xl M7X0  MOB 7 X-position  |
  | 15 ($0F) | M7Y7 M7Y6 M7Y5 M7Y4 M7Y3 M7Y2 M7Y1 M6Y0  MOB 7 Y-position  |
  | 16 ($10) | M7X8 M6X8 M5X8 M4X8 M3X8 M2X8 M1X8 M0X8  MSB of X-position |
  | 17 ($11) | RC8  ECM  BMM  DEN  RSEL Y2   Y1   Y0      See text        |
  | 18 ($12) | RC7  RC6  RC5  RC4  RC3  RC2  RC1  RC0   Raster register   |
  | 19 ($13) | LPX8 LPX7 LPX6 LPX5 LPX4 LPX3 LPX2 LPX1  Light Pen X       |
  | 20 ($14) | LPY7 LPY6 LPY5 LPY4 LPY3 LPY2 LPY1 LPY0  Light Pen Y       |
  | 21 ($15) | M7E  M6E  M5E  M4E  M3E  M2E  M1E  M0E   MOB Enable        |
  | 22 ($16) |  -    -   RES  MCM  CSEL X2   X1   X0      See text        |
  | 23 ($17) | M7YE M6YE M5YE M4YE M3YE M2YE M1YE M0YE  MOB Y-expand      |
  | 24 ($18) | VM13 VM12 VM11 VM10 CB13 CB12 CB11  -    Memory Pointers   |
  | 25 ($19) | IRQ   -    -    -   ILP  IMMC IMBC IRST  Interrupt Register|
  | 26 ($1A) |  -    -    -    -   ELP  EMMC EMBC ERST  Enable Interrupt  |
  | 27 ($1B) | M7DP M6DP M5DP M4DP M3DP M2DP M1DP M0DP  MOB-DATA Priority |
  | 28 ($1C) | M7MC M6MC M5MC M4MC M3MC M2MC M1MC M0MC  MOB Multicolor Sel|
  | 29 ($1D) | M7XE M6XE M5XE M4XE M3XE M2XE M1XE M0XE  MOB X-expand      |
  | 30 ($1E) | M7M  M6M  M5M  M4M  M3M  M2M  M1M  M0M   MOB-MOB Collision |
  | 31 ($1F) | M7D  M6D  M5D  M4D  M3D  M2D  M1D  M0D   MOB-DATA Collision|
  | 32 ($20) |  -    -    -    -   EC3  EC2  EC1  EC0   Exterior Color    |
  | 33 ($21) |  -    -    -    -   B0C3 B0C2 B0C1 B0C0  Bkgd #0 Color     |
  | 34 ($22) |  -    -    -    -   B1C3 B1C2 B1C1 B1C0  Bkgd #1 Color     |
  | 35 ($23) |  -    -    -    -   B2C3 B2C2 B2C1 B2C0  Bkgd #2 Color     |
  | 36 ($24) |  -    -    -    -   B3C3 B3C2 B3C1 B3C0  Bkgd #3 Color     |
  | 37 ($25) |  -    -    -    -   MM03 MM02 MM01 MM00  MOB Multicolor #0 |
  | 38 ($26) |  -    -    -    -   MM13 MM12 MM11 MM10  MOB Multicolor #1 |
  | 39 ($27) |  -    -    -    -   M0C3 M0C2 M0C1 M0C0  MOB 0 Color       |
  | 40 ($28) |  -    -    -    -   M1C3 M1C2 M1C1 M1C0  MOB 1 Color       |
  | 41 ($29) |  -    -    -    -   M2C3 M2C2 M2C1 M2C0  MOB 2 Color       |
  | 42 ($2A) |  -    -    -    -   M3C3 M3C2 M3C1 M3C0  MOB 3 Color       |
  | 43 ($2B) |  -    -    -    -   M4C3 M4C2 M4C1 M4C0  MOB 4 Color       |
  | 44 ($2C) |  -    -    -    -   M5C3 M5C2 M5C1 M5C0  MOB 5 Color       |
  | 45 ($2D) |  -    -    -    -   M6C3 M6C2 M6C1 M6C0  MOB 6 Color       |
  | 46 ($2E) |  -    -    -    -   M7C3 M7C2 M7C1 M7C0  MOB 7 Color       |
  +----------+------------------------------------------------------------+

  +-----------------------------------------------------------------------+
  | NOTE: A dash indicates a no connect. All no connects are read as a    |
  | "1"                                                                   |
  +-----------------------------------------------------------------------+
  from C64 PRG, pp.454-455
*/
