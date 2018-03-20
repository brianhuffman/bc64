/* highlevel.c - optimized routines for often-called sections of kernal */
/* this file should be included directly into 6510.c */

#include "serial.h"

inline static void update_nz(int result) {
	flag_nz = result;
}

/* poll keyboard buffer */
inline void kernal_e5cd() {
/*
	e5cd:  LDA $c6  	;a5c6    ;NDX no. of chars in keyboard queue
	e5cf:  STA $cc  	;85cc    ;BLNSW cursor blink enable, 0=blink
	e5d1:  STA $0292	;8d9202  ;AUTODN automatic scroll down, 0=ON
	e5d4:  BEQ $e5cd  	;f0f7    ;loop until keys are pressed
*/

	reg_a = mem_read(0xc6);
	mem_write(0xcc, reg_a);
	mem_write(0x0292, reg_a);
	update_nz(reg_a);
	if (reg_a==0) { reg_pc = 0xe5cd; clock_advance(1); }
	else reg_pc = 0xe5d6;
	clock_advance(3+3+4+2);
}

inline void kernal_e9d4() {
	int address;
/*
	e9d4:  LDA ($ac),Y  	;b1ac    ;SAL screen scrolling ptr
	e9d6:  STA ($d1),Y  	;91d1    ;PNT screen address ptr
	e9d8:  LDA ($ae),Y  	;b1ae    ;EAL color scrolling ptr
	e9da:  STA ($f3),Y  	;91f3    ;USER color address ptr
	e9dc:  DEY 		;88
	e9dd:  BPL $e9d4  	;10f5    ;loop to copy entire line
*/
	address = mem_read(0xac) + reg_y;
	if (address > 0xff) clock_advance(1);
	address += (mem_read(0xad) << 8);
	reg_a = mem_read(address);

	address = mem_read_16(0xd1) + reg_y;
	mem_write (address, reg_a);

	address = mem_read(0xae) + reg_y;
	if (address > 0xff) clock_advance(1);
	address += (mem_read(0xaf) << 8);
	reg_a = mem_read(address);

	address = mem_read_16(0xf3) + reg_y;
	mem_write (address, reg_a);

	reg_y = (reg_y - 1) & 0xff;
	update_nz(reg_y);
	if ( !(reg_p & N_FLAG) ) { reg_pc = 0xe9d4; clock_advance(1); }
	else reg_pc = 0xe9df;
	clock_advance(5+6+5+6+2+2);
}
/*
eab3:  LSR 		;4a      ;check next row
...
ead2:  BNE $eab3  	;d0df    ;loop to all rows
*/


/*
fd6c:  INC $c2  	;e6c2
...
fd86:  BEQ $fd6c  	;f0e4    ;loop to check all pages
*/

/*****************************************/
/**** serial emulation kernal patches ****/
/*****************************************/

/* LISTN */
/* SECND */
/* TALK */
/* TKSA */
/* CIOUT */
inline void kernal_ed40() {
	int atn, error;
	
	atn = mem_read(0xdd00) & 0x08;

	/* $95 = BSOUT, buffered serial char */
	error = serial_write (atn, mem_read(0x95));

	if (error == SERIAL_DEVICE_NOT_PRESENT) mem_write(0x90, mem_read(0x90) | 0x80);
	if (error == SERIAL_TIME_OUT) mem_write(0x90, mem_read(0x90) | 0x03);
	cpu6510_CLI();
	cpu6510_RTS();
}

/* ACPTR */
inline void kernal_ee13() {
	int result = serial_read ();
	if (result & SERIAL_END_OF_FILE) mem_write (0x90, mem_read(0x90) | 0x40);
	if (result == SERIAL_TIME_OUT) mem_write (0x90, mem_read(0x90) | 0x02);
	
	reg_a = result & 0xff;
	cpu6510_CLI();
	cpu6510_CLC();
	cpu6510_RTS();
}

inline void do_highlevel() {
	if (reg_pc == 0xe5cd + 1) kernal_e5cd();
	else if (reg_pc == 0xe9d4 + 1) kernal_e9d4();
	else if (reg_pc == 0xed40 + 1) kernal_ed40();
	else if (reg_pc == 0xee13 + 1) kernal_ee13();
	else cpu6510_JAM();
}
