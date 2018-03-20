/* 6510.c - processor emulation for C64 emulator */
/* by Brian Huffman 11-29-00 */

#include <stdio.h>
#include <stdlib.h>
#include "6510.h"
#include "mem_c64.h"

//#define VICELOG

/* define processor status flags: NV1BDIZC */
#define C_FLAG 0x01
#define Z_FLAG 0x02
#define I_FLAG 0x04
#define D_FLAG 0x08
#define B_FLAG 0x10
/* bit 5 is always 1 */
#define V_FLAG 0x40
#define N_FLAG 0x80

/* declare processor registers */
static int reg_pc;
static int reg_a, reg_x, reg_y, reg_s, reg_p;
//static int flag_n, flag_z, flag_c;
static int flag_nz, flag_c;

/* clock and interrupt services */
static void (*callback[CB_MAX])(void);
static int callback_time[CB_MAX];
static int next_callback[CB_MAX];
static int time_left = 0;

#ifdef WATCHPOINT
/* screen debugging flag */
int VERBOSE = 0;
#endif

#ifdef COUNT_INSTRUCTIONS
int instr_count[256];
#endif

/* declare arrays for memory storage */
unsigned char ram_64k[0x10000];
unsigned char readable[0x10000];
unsigned char io_ram[0x1000];
unsigned char character_rom[0x1000];
unsigned char color_ram[0x0400];

unsigned char *stack;
unsigned char *character_base;
unsigned char *video_matrix;
unsigned char *bitmap_base;
unsigned char *video_bank;
int mem_video_matrix, mem_bitmap_base;
int mem_videoptr, mem_videobank;

/* declare set of flags for memory configuration */
/* bit0 = LORAM; bit1 = HIRAM; bit2 = CHAREN */
int mem_flags;

/***************************/
/**** Utility functions ****/
/***************************/

int cpu6510_clock (void) {
	return callback_time[next_callback[0]] - time_left;
}

void cpu6510_callback (int source, void (*cb)(void), int time) {
	int now, i;
	int before, after;
	
	//printf("source %i registered callback for time %i\n", source, time);
	
	/* only 16 slots */
	if (source >= CB_MAX) return;
	
	/* figure out current time */
	now = callback_time[next_callback[0]] - time_left;

	/* unregister old callback if we are overwriting one */
	if (callback[source] != NULL) {
		for (i=0; i<CB_MAX; i++) {
			if (next_callback[i] == source) {
				next_callback[i] = next_callback[source];
				break;
			}
		}
	}
	
	/* put the new callback in the table */
	callback[source] = cb;
	callback_time[source] = time;
	
	/* place the new callback in the list */
	if (cb != NULL) {
		before = 0;
		while (1) {
			after = next_callback[before];
			//printf("callback_time[%i] = %i\n", after, callback_time[after]);
			if (time < callback_time[after] || after == 0) {
				next_callback[before] = source;
				next_callback[source] = after;
				break;
			}
			before = after;
		}
	}
	
	/* calculate new time_left value */
	time_left = callback_time[next_callback[0]] - now;
	//printf("clock = %i, next = %i, time_left = %i\n", now, next_callback[0], time_left);
}

static void handle_callbacks() {
	int old, new;
	void (*cb)(void);
	
	/* get current callback function */
	old = next_callback[0];
	new = next_callback[old];
	cb = callback[old];

	//printf ("clock = %i, handling callback for %i\n", callback_time[old] - time_left, old);

	/* unregister current callback function */
	next_callback[0] = new;
	callback[old] = NULL;
	time_left += callback_time[new] - callback_time[old];
	//printf ("new time_left = %i\n", time_left);
	/* call the callback */
	if (cb != NULL) cb();
}	
  /*
    types of interrupts to take care of :
    60 Hz clock interrupt
    Video line interrupt

    and periodic other stuff:
    SDL_Delay
    update screen
    read keyboard
  */


void cpu6510_bad_line() {
	time_left -= 40;
}

inline static void clock_advance (int ticks) {
	time_left -= ticks;
}

/***********************************/
/**** Addressing mode functions ****/
/***********************************/

/* this file has all the addressing mode routines in it */
#include "6510_addressing.c"


/**********************************/
/**** General Opcode Functions ****/
/***********************************/

/* list of documented instructions:
   ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC,
   CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP,
   JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI,
   RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA
*/

/* this file has all the general opcode instructions in it */
#include "6510_instructions.c"


/****************************************/
/**** High-Level optimized functions ****/
/****************************************/

/* this file has all the highlevel optimized routines in it */
#include "6510_highlevel.c"


/************************/
/**** Main functions ****/
/************************/

void cpu6510_reset() {
	/* reset clock */
	//time_left = 0;
	/* initialize program counter to RESET vector */
	reg_pc = mem_read_16(0xfffc);
}

void cpu6510_irq() {
#ifndef VICELOG
	/* check to see if interrupts are enabled */
	if ( (reg_p & I_FLAG) == 0 ) {

	/* if so, then jump to IRQ vector */
	cpu6510_JSR( mem_read_16(0xfffe) );
	/* push processor status */
	cpu6510_PHP();
	/* disable further interrupts */
	cpu6510_SEI();
	/* IRQ takes 7(?) cycles */
	clock_advance(7);

	if (reg_s & (~0xff)) {
		fprintf(stderr, "stack overflow!\n");
		exit(2);
	}
  }
#endif
}

void cpu6510_nmi() {
#ifndef VICELOG
	/* jump to NMI vector */
	cpu6510_JSR( mem_read_16(0xfffa) );
	/* push processor status, with B flag clear */
	update_p();
	stack_write(reg_s--, reg_p & ~B_FLAG);
        //cpu6510_PHP();
#endif
}


#ifdef VICELOG
void sync_with_logfile() {
	static FILE *logfile = NULL;
	static int vice_pc=0, vice_a=0, vice_x=0, vice_y=0, old_s=0;
	static int instruction_count = 0;

	int header, irq_occurred;
	int old_pc = vice_pc;

	instruction_count++;

	/* make sure we can read from log */
	if (logfile == NULL) {
		printf("Reading vicelog file\n");
		logfile = fopen("vicelog", "r");
		if (logfile == NULL) {
			printf("vicelog file not found\n");
			exit(1);
		}
	}

	/* header format: pc_hi(lsb), pc_lo, a, x, y, nmi, irq, brk */
	/* read header byte */
	header = fgetc(logfile);
	if (header == EOF) exit(0);

	/* check for occurrence of interrupt */
	irq_occurred = (reg_s == old_s - 3);
	old_s = reg_s;

	/* was there supposed to be an interrupt? */
	if (header & 0xe0) {
		if (!irq_occurred) {
			switch(header) {
			case 0x80:
				printf("%i\t$%04x: ", instruction_count, old_pc);
				printf("BRK should have occurred\n", old_pc);
				cpu6510_BRK();
				break;
			case 0x40:
				/* printf("%i\t$%04x: ", instruction_count, old_pc); */
				/* printf("IRQ should have occurred\n", old_pc); */
				printf("%i\tPC=%04x A=%02x X=%02x Y=%02x P=%02x S=%02x\n"
					,instruction_count,reg_pc,reg_a,reg_x,reg_y,reg_p,reg_s);
				cpu6510_JSR( mem_read_16(0xfffe) );
				cpu6510_PHP();
				cpu6510_SEI();
				break;
			case 0x20:
				printf("%i\t$%04x: ", instruction_count, old_pc);
				printf("NMI should have occurred\n", old_pc);
				cpu6510_JSR( mem_read_16(0xfffa) );
				cpu6510_PHP();
				break;
			}
		}
		/* read new header */
		header = fgetc(logfile);
	}
	else if (irq_occurred) {
		printf("%i\t$%04x: ", instruction_count, old_pc);
		printf("IRQ should not have occurred\n", old_pc);
		cpu6510_RTI();
	}

	/* update vice register values */
	if (header & 0x01) vice_pc = (vice_pc & 0x00ff) | (fgetc(logfile) << 8);
	if (header & 0x02) vice_pc = (vice_pc & 0xff00) | getc(logfile);
	if (header & 0x04) vice_a = fgetc(logfile);
	if (header & 0x08) vice_x = fgetc(logfile);
	if (header & 0x10) vice_y = fgetc(logfile);

	/* some messages shouldn't be printed */
	if (
		(old_pc == 0xff5e) || /*read vic raster*/
		(old_pc == 0xfd6e) || /*memory test*/
		(old_pc == 0xee13) || /*disk loading*/
		(old_pc == 0xf505) || /*disk status*/
		(old_pc == 0xea93) || /*key input*/
		(old_pc == 0xeaab) || /*key input*/
		(old_pc == 0xeab1) || /*key input*/
		(old_pc == 0xf6bc) || /*key input*/
		(old_pc == 0xf6c2)    /*key input*/
		) {
		reg_pc = vice_pc;
		reg_a = vice_a;
		reg_x = vice_x;
		reg_y = vice_y;
		return;
	}

	/* if past end of file, don't change anything */
	if (feof(logfile)) exit(0);

	/* check to see if registers match */
	if (reg_pc != vice_pc) {
		printf("%i\t$%04x: ", instruction_count, old_pc);
		printf("PC became %04x, should be %04x\n", reg_pc, vice_pc);
		reg_pc = vice_pc;
	}
	if (reg_a != vice_a) {
		printf("%i\t$%04x: ", instruction_count, old_pc);
		printf("A became %02x, should be %02x\n", reg_a, vice_a);
		reg_a = vice_a;
	}
	if (reg_x != vice_x) {
		printf("%i\t$%04x: ", instruction_count, old_pc);
		printf("X became %02x, should be %02x\n", reg_x, vice_x);
		reg_x = vice_x;
	}
	if (reg_y != vice_y) {
		printf("%i\t$%04x: ", instruction_count, old_pc);
		printf("Y became %02x, should be %02x\n", reg_y, vice_y);
		reg_y = vice_y;
	}
}
# endif


/************************ start of main loop ************************/

void cpu6510_main ()
{
	int opcode;
	//time_left += cycles;

	while (1) {
		while (time_left <= 0) handle_callbacks();

#ifdef WATCHPOINT
		if (reg_pc == WATCHPOINT) VERBOSE = 1;
		if (VERBOSE == 1) print_state();
#endif
/*
		if (reg_pc == 0x03a2) {
			int i;
			FILE *fout;
			fout = fopen("dump", "w");
			fputc(0, fout);
			fputc(0, fout);
			for (i=0; i<=0xffff; i++) {
				fputc(mem_read(i), fout);
			}
			fclose(fout);
			exit(0);
		}
*/
#ifdef VICELOG
		sync_with_logfile();
#endif

		/* dispatch next instruction */
		opcode = mem_read(reg_pc++);
		switch (opcode) {

		case 0x00:  cpu6510_BRK();            clock_advance(7);  break;
		case 0x01:  cpu6510_ORA(addr_inx());  clock_advance(6);  break;
		case 0x02:  do_highlevel();                              break;
		case 0x03:  cpu6510_SLO(addr_inx());  clock_advance(8);  break;
		case 0x04:  cpu6510_NOP(addr_zpg());  clock_advance(3);  break;
		case 0x05:  cpu6510_ORA(addr_zpg());  clock_advance(3);  break;
		case 0x06:  cpu6510_ASL(addr_zpg());  clock_advance(5);  break;
		case 0x07:  cpu6510_SLO(addr_zpg());  clock_advance(5);  break;
		case 0x08:  cpu6510_PHP();            clock_advance(3);  break;
		case 0x09:  cpu6510_ORA(addr_imm());  clock_advance(2);  break;
		case 0x0a:  cpu6510_ASL_a();          clock_advance(2);  break;
		case 0x0b:  cpu6510_ANC(addr_imm());  clock_advance(2);  break;
		case 0x0c:  cpu6510_NOP(addr_abs());  clock_advance(4);  break;
		case 0x0d:  cpu6510_ORA(addr_abs());  clock_advance(4);  break;
		case 0x0e:  cpu6510_ASL(addr_abs());  clock_advance(6);  break;
		case 0x0f:  cpu6510_SLO(addr_abs());  clock_advance(6);  break;
		case 0x10:  cpu6510_BPL(addr_imm());  clock_advance(2);  break;
		case 0x11:  cpu6510_ORA(addr_iny());  clock_advance(5);  break;
		case 0x12:  cpu6510_JAM();                               break;
		case 0x13:  cpu6510_JAM();   /* SLO (ind),y */           break;
		case 0x14:  cpu6510_JAM();   /* NOP (zpx)   */           break;
		case 0x15:  cpu6510_ORA(addr_zpx());  clock_advance(4);  break;
		case 0x16:  cpu6510_ASL(addr_zpx());  clock_advance(6);  break;
		case 0x17:  cpu6510_JAM();   /* SLO (zpx)   */           break;
		case 0x18:  cpu6510_CLC();            clock_advance(2);  break;
		case 0x19:  cpu6510_ORA(addr_aby());  clock_advance(4);  break;
		case 0x1a:  cpu6510_JAM();   /* NOP         */           break;
		case 0x1b:  cpu6510_JAM();   /* SLO (aby)   */           break;
		case 0x1c:  cpu6510_NOP(addr_abx());  clock_advance(4);  break;
		case 0x1d:  cpu6510_ORA(addr_abx());  clock_advance(4);  break;
		case 0x1e:  cpu6510_ASL(addr_abx());  clock_advance(7);  break;
		case 0x1f:  cpu6510_JAM();   /* SLO (abx)   */           break;
		case 0x20:  cpu6510_JSR(addr_abs());  clock_advance(6);  break;
		case 0x21:  cpu6510_AND(addr_inx());  clock_advance(6);  break;
		case 0x22:  cpu6510_JAM();                               break;
		case 0x23:  cpu6510_RLA(addr_inx());  clock_advance(8);  break;
		case 0x24:  cpu6510_BIT(addr_zpg());  clock_advance(3);  break;
		case 0x25:  cpu6510_AND(addr_zpg());  clock_advance(3);  break;
		case 0x26:  cpu6510_ROL(addr_zpg());  clock_advance(5);  break;
		case 0x27:  cpu6510_RLA(addr_zpg());  clock_advance(5);  break;
		case 0x28:  cpu6510_PLP();            clock_advance(4);  break;
		case 0x29:  cpu6510_AND(addr_imm());  clock_advance(2);  break;
		case 0x2a:  cpu6510_ROL_a();          clock_advance(2);  break;
		case 0x2b:  cpu6510_JAM();   /* ANC (imm)    */          break;
		case 0x2c:  cpu6510_BIT(addr_abs());  clock_advance(4);  break;
		case 0x2d:  cpu6510_AND(addr_abs());  clock_advance(4);  break;
		case 0x2e:  cpu6510_ROL(addr_abs());  clock_advance(6);  break;
		case 0x2f:  cpu6510_RLA(addr_abs());  clock_advance(6);  break;
		case 0x30:  cpu6510_BMI(addr_imm());  clock_advance(2);  break;
		case 0x31:  cpu6510_AND(addr_iny());  clock_advance(5);  break;
		case 0x32:  cpu6510_JAM();                               break;
		case 0x33:  cpu6510_JAM();                               break;
		case 0x34:  cpu6510_JAM();                               break;
		case 0x35:  cpu6510_AND(addr_zpx());  clock_advance(4);  break;
		case 0x36:  cpu6510_ROL(addr_zpx());  clock_advance(6);  break;
		case 0x37:  cpu6510_JAM();                               break;
		case 0x38:  cpu6510_SEC();            clock_advance(2);  break;
		case 0x39:  cpu6510_AND(addr_aby());  clock_advance(4);  break;
		case 0x3a:  cpu6510_JAM();                               break;
		case 0x3b:  cpu6510_RLA(addr_aby());  clock_advance(7);  break;
		case 0x3c:  cpu6510_NOP(addr_abx());  clock_advance(4);  break;
		case 0x3d:  cpu6510_AND(addr_abx());  clock_advance(4);  break;
		case 0x3e:  cpu6510_ROL(addr_abx());  clock_advance(7);  break;
		case 0x3f:  cpu6510_RLA(addr_abx());  clock_advance(7);  break;
		case 0x40:  cpu6510_RTI();            clock_advance(6);  break;
		case 0x41:  cpu6510_EOR(addr_inx());  clock_advance(6);  break;
		case 0x42:  cpu6510_JAM();                               break;
		case 0x43:  cpu6510_JAM();                               break;
		case 0x44:  cpu6510_NOP(addr_zpg());  clock_advance(3);  break;
		case 0x45:  cpu6510_EOR(addr_zpg());  clock_advance(3);  break;
		case 0x46:  cpu6510_LSR(addr_zpg());  clock_advance(7);  break;
		case 0x47:  cpu6510_SRE(addr_inx());  clock_advance(8);  break;
		case 0x48:  cpu6510_PHA();            clock_advance(3);  break;
		case 0x49:  cpu6510_EOR(addr_imm());  clock_advance(2);  break;
		case 0x4a:  cpu6510_LSR_a();          clock_advance(2);  break;
		case 0x4b:  cpu6510_ASR(addr_imm());  clock_advance(2);  break;
		case 0x4c:  cpu6510_JMP(addr_abs());  clock_advance(3);  break;
		case 0x4d:  cpu6510_EOR(addr_abs());  clock_advance(4);  break;
		case 0x4e:  cpu6510_LSR(addr_abs());  clock_advance(6);  break;
		case 0x4f:  cpu6510_JAM();                               break;
		case 0x50:  cpu6510_BVC(addr_imm());  clock_advance(2);  break;
		case 0x51:  cpu6510_EOR(addr_iny());  clock_advance(5);  break;
		case 0x52:  cpu6510_JAM();                               break;
		case 0x53:  cpu6510_JAM();                               break;
		case 0x54:  cpu6510_NOP(addr_zpx());  clock_advance(4);  break;
		case 0x55:  cpu6510_EOR(addr_zpx());  clock_advance(4);  break;
		case 0x56:  cpu6510_LSR(addr_zpx());  clock_advance(6);  break;
		case 0x57:  cpu6510_JAM();                               break;
		case 0x58:  cpu6510_CLI();            clock_advance(2);  break;
		case 0x59:  cpu6510_EOR(addr_aby());  clock_advance(4);  break;
		case 0x5a:  cpu6510_JAM();                               break;
		case 0x5b:  cpu6510_JAM();                               break;
		case 0x5c:  cpu6510_JAM();                               break;
		case 0x5d:  cpu6510_EOR(addr_abx());  clock_advance(4);  break;
		case 0x5e:  cpu6510_LSR(addr_abx());  clock_advance(7);  break;
		case 0x5f:  cpu6510_JAM();                               break;
		case 0x60:  cpu6510_RTS();            clock_advance(6);  break;
		case 0x61:  cpu6510_ADC(addr_inx());  clock_advance(6);  break;
		case 0x62:  cpu6510_JAM();                               break;
		case 0x63:  cpu6510_RRA(addr_inx());  clock_advance(8);  break;
		case 0x64:  cpu6510_JAM();                               break;
		case 0x65:  cpu6510_ADC(addr_zpg());  clock_advance(3);  break;
		case 0x66:  cpu6510_ROR(addr_zpg());  clock_advance(5);  break;
		case 0x67:  cpu6510_RRA(addr_zpg());  clock_advance(5);  break;
		case 0x68:  cpu6510_PLA();            clock_advance(4);  break;
		case 0x69:  cpu6510_ADC(addr_imm());  clock_advance(2);  break;
		case 0x6a:  cpu6510_ROR_a();          clock_advance(2);  break;
		case 0x6b:  cpu6510_JAM();                               break;
		case 0x6c:  cpu6510_JMP(addr_ind());  clock_advance(5);  break;
		case 0x6d:  cpu6510_ADC(addr_abs());  clock_advance(4);  break;
		case 0x6e:  cpu6510_ROR(addr_abs());  clock_advance(6);  break;
		case 0x6f:  cpu6510_JAM();                               break;
		case 0x70:  cpu6510_BVS(addr_imm());  clock_advance(2);  break;
		case 0x71:  cpu6510_ADC(addr_iny());  clock_advance(5);  break;
		case 0x72:  cpu6510_JAM();                               break;
		case 0x73:  cpu6510_RRA(addr_iny());  clock_advance(8);  break;
		case 0x74:  cpu6510_NOP(addr_zpx());  clock_advance(4);  break;
		case 0x75:  cpu6510_ADC(addr_zpx());  clock_advance(4);  break;
		case 0x76:  cpu6510_ROR(addr_zpx());  clock_advance(6);  break;
		case 0x77:  cpu6510_JAM();                               break;
		case 0x78:  cpu6510_SEI();            clock_advance(2);  break;
		case 0x79:  cpu6510_ADC(addr_aby());  clock_advance(4);  break;
		case 0x7a:  cpu6510_JAM();                               break;
		case 0x7b:  cpu6510_JAM();                               break;
		case 0x7c:  cpu6510_JAM();                               break;
		case 0x7d:  cpu6510_ADC(addr_abx());  clock_advance(4);  break;
		case 0x7e:  cpu6510_ROR(addr_abx());  clock_advance(7);  break;
		case 0x7f:  cpu6510_RRA(addr_abx());  clock_advance(7);  break;
		case 0x81:  cpu6510_STA(addr_inx());  clock_advance(6);  break;
		case 0x82:  cpu6510_NOP(addr_imm());  clock_advance(2);  break;
		case 0x83:  cpu6510_SAX(addr_inx());  clock_advance(6);  break;
		case 0x84:  cpu6510_STY(addr_zpg());  clock_advance(3);  break;
		case 0x85:  cpu6510_STA(addr_zpg());  clock_advance(3);  break;
		case 0x86:  cpu6510_STX(addr_zpg());  clock_advance(3);  break;
		case 0x87:  cpu6510_SAX(addr_zpg());  clock_advance(3);  break;
		case 0x88:  cpu6510_DEY();            clock_advance(2);  break;
		case 0x8a:  cpu6510_TXA();            clock_advance(2);  break;
		case 0x8b:  cpu6510_JAM();                               break;
		case 0x8c:  cpu6510_STY(addr_abs());  clock_advance(4);  break;
		case 0x8d:  cpu6510_STA(addr_abs());  clock_advance(4);  break;
		case 0x8e:  cpu6510_STX(addr_abs());  clock_advance(4);  break;
		case 0x8f:  cpu6510_SAX(addr_abs());  clock_advance(4);  break;
		case 0x90:  cpu6510_BCC(addr_imm());  clock_advance(2);  break;
		case 0x91:  cpu6510_STA(addr_iny());  clock_advance(5);  break;
		case 0x92:  cpu6510_JAM();                               break;
		case 0x93:  cpu6510_JAM();                               break;
		case 0x94:  cpu6510_STY(addr_zpx());  clock_advance(4);  break;
		case 0x95:  cpu6510_STA(addr_zpx());  clock_advance(4);  break;
		case 0x96:  cpu6510_STX(addr_zpy());  clock_advance(4);  break;
		case 0x97:  cpu6510_SAX(addr_zpy());  clock_advance(4);  break;
		case 0x98:  cpu6510_TYA();            clock_advance(2);  break;
		case 0x99:  cpu6510_STA(addr_aby());  clock_advance(4);  break;
		case 0x9a:  cpu6510_TXS();            clock_advance(2);  break;
		case 0x9b:  cpu6510_JAM();                               break;
		case 0x9c:  cpu6510_SHY(addr_abx());  clock_advance(5);  break;
		case 0x9d:  cpu6510_STA(addr_abx());  clock_advance(4);  break;
		case 0x9e:  cpu6510_SHX(addr_aby());  clock_advance(5);  break;
		case 0x9f:  cpu6510_SHA(addr_aby());  clock_advance(5);  break;
		case 0xa0:  cpu6510_LDY(addr_imm());  clock_advance(2);  break;
		case 0xa1:  cpu6510_LDA(addr_inx());  clock_advance(6);  break;
		case 0xa2:  cpu6510_LDX(addr_imm());  clock_advance(2);  break;
		case 0xa3:  cpu6510_JAM();                               break;
		case 0xa4:  cpu6510_LDY(addr_zpg());  clock_advance(3);  break;
		case 0xa5:  cpu6510_LDA(addr_zpg());  clock_advance(3);  break;
		case 0xa6:  cpu6510_LDX(addr_zpg());  clock_advance(3);  break;
		case 0xa7:  cpu6510_JAM();                               break;
		case 0xa8:  cpu6510_TAY();            clock_advance(2);  break;
		case 0xa9:  cpu6510_LDA(addr_imm());  clock_advance(2);  break;
		case 0xaa:  cpu6510_TAX();            clock_advance(2);  break;
		case 0xab:  cpu6510_JAM();                               break;
		case 0xac:  cpu6510_LDY(addr_abs());  clock_advance(4);  break;
		case 0xad:  cpu6510_LDA(addr_abs());  clock_advance(4);  break;
		case 0xae:  cpu6510_LDX(addr_abs());  clock_advance(4);  break;
		case 0xaf:  cpu6510_JAM();                               break;
		case 0xb0:  cpu6510_BCS(addr_imm());  clock_advance(2);  break;
		case 0xb1:  cpu6510_LDA(addr_iny());  clock_advance(5);  break;
		case 0xb2:  cpu6510_JAM();                               break;
		case 0xb3:  cpu6510_LAX(addr_iny());  clock_advance(5);  break;
		case 0xb4:  cpu6510_LDY(addr_zpx());  clock_advance(4);  break;
		case 0xb5:  cpu6510_LDA(addr_zpx());  clock_advance(4);  break;
		case 0xb6:  cpu6510_LDX(addr_zpy());  clock_advance(4);  break;
		case 0xb7:  cpu6510_JAM();                               break;
		case 0xb8:  cpu6510_CLV();            clock_advance(2);  break;
		case 0xb9:  cpu6510_LDA(addr_aby());  clock_advance(4);  break;
		case 0xba:  cpu6510_TSX();            clock_advance(2);  break;
		case 0xbb:  cpu6510_JAM();                               break;
		case 0xbc:  cpu6510_LDY(addr_abx());  clock_advance(4);  break;
		case 0xbd:  cpu6510_LDA(addr_abx());  clock_advance(4);  break;
		case 0xbe:  cpu6510_LDX(addr_aby());  clock_advance(4);  break;
		case 0xbf:  cpu6510_LAX(addr_aby());  clock_advance(4);  break;
		case 0xc0:  cpu6510_CPY(addr_imm());  clock_advance(2);  break;
		case 0xc1:  cpu6510_CMP(addr_inx());  clock_advance(6);  break;
		case 0xc2:  cpu6510_NOP(addr_imm());  clock_advance(2);  break;
		case 0xc3:  cpu6510_DCP(addr_inx());  clock_advance(8);  break;
		case 0xc4:  cpu6510_CPY(addr_zpg());  clock_advance(3);  break;
		case 0xc5:  cpu6510_CMP(addr_zpg());  clock_advance(3);  break;
		case 0xc6:  cpu6510_DEC(addr_zpg());  clock_advance(5);  break;
		case 0xc7:  cpu6510_DCP(addr_zpg());  clock_advance(5);  break;
		case 0xc8:  cpu6510_INY();            clock_advance(2);  break;
		case 0xc9:  cpu6510_CMP(addr_imm());  clock_advance(2);  break;
		case 0xca:  cpu6510_DEX();            clock_advance(2);  break;
		case 0xcb:  cpu6510_SBX(addr_imm());  clock_advance(2);  break;
		case 0xcc:  cpu6510_CPY(addr_abs());  clock_advance(4);  break;
		case 0xcd:  cpu6510_CMP(addr_abs());  clock_advance(4);  break;
		case 0xce:  cpu6510_DEC(addr_abs());  clock_advance(6);  break;
		case 0xcf:  cpu6510_JAM();                               break;
		case 0xd0:  cpu6510_BNE(addr_imm());  clock_advance(2);  break;
		case 0xd1:  cpu6510_CMP(addr_iny());  clock_advance(5);  break;
		case 0xd2:  cpu6510_JAM();                               break;
		case 0xd3:  cpu6510_JAM();                               break;
		case 0xd4:  cpu6510_JAM();                               break;
		case 0xd5:  cpu6510_CMP(addr_zpx());  clock_advance(4);  break;
		case 0xd6:  cpu6510_DEC(addr_zpx());  clock_advance(6);  break;
		case 0xd7:  cpu6510_JAM();                               break;
		case 0xd8:  cpu6510_CLD();            clock_advance(2);  break;
		case 0xd9:  cpu6510_CMP(addr_aby());  clock_advance(4);  break;
		case 0xda:  cpu6510_JAM();                               break;
		case 0xdb:  cpu6510_JAM();                               break;
		case 0xdc:  cpu6510_JAM();                               break;
		case 0xdd:  cpu6510_CMP(addr_abx());  clock_advance(4);  break;
		case 0xde:  cpu6510_DEC(addr_abx());  clock_advance(7);  break;
		case 0xdf:  cpu6510_JAM();                               break;
		case 0xe0:  cpu6510_CPX(addr_imm());  clock_advance(2);  break;
		case 0xe1:  cpu6510_SBC(addr_inx());  clock_advance(6);  break;
		case 0xe2:  cpu6510_NOP(addr_imm());  clock_advance(2);  break;
		case 0xe3:  cpu6510_JAM();                               break;
		case 0xe4:  cpu6510_CPX(addr_zpg());  clock_advance(3);  break;
		case 0xe5:  cpu6510_SBC(addr_zpg());  clock_advance(3);  break;
		case 0xe6:  cpu6510_INC(addr_zpg());  clock_advance(5);  break;
		case 0xe7:  cpu6510_ISB(addr_zpg());  clock_advance(5);  break;
		case 0xe8:  cpu6510_INX();            clock_advance(2);  break;
		case 0xe9:  cpu6510_SBC(addr_imm());  clock_advance(2);  break;
		case 0xea:  /* NOP no operation */    clock_advance(2);  break;
		case 0xeb:  cpu6510_SBC(addr_imm());  clock_advance(2);  break;
		case 0xec:  cpu6510_CPX(addr_abs());  clock_advance(4);  break;
		case 0xed:  cpu6510_SBC(addr_abs());  clock_advance(4);  break;
		case 0xee:  cpu6510_INC(addr_abs());  clock_advance(6);  break;
		case 0xef:  cpu6510_JAM();                               break;
		case 0xf0:  cpu6510_BEQ(addr_imm());  clock_advance(2);  break;
		case 0xf1:  cpu6510_SBC(addr_iny());  clock_advance(5);  break;
		case 0xf2:  cpu6510_JAM();                               break;
		case 0xf3:  cpu6510_JAM();                               break;
		case 0xf4:  cpu6510_JAM();                               break;
		case 0xf5:  cpu6510_SBC(addr_zpx());  clock_advance(4);  break;
		case 0xf6:  cpu6510_INC(addr_zpx());  clock_advance(6);  break;
		case 0xf7:  cpu6510_JAM();                               break;
		case 0xf8:  cpu6510_SED();            clock_advance(2);  break;
		case 0xf9:  cpu6510_SBC(addr_aby());  clock_advance(4);  break;
		case 0xfa:  cpu6510_JAM();                               break;
		case 0xfb:  cpu6510_ISB(addr_aby());  clock_advance(4);  break;
		case 0xfc:  cpu6510_NOP(addr_abx());  clock_advance(4);  break;
		case 0xfd:  cpu6510_SBC(addr_abx());  clock_advance(4);  break;
		case 0xfe:  cpu6510_INC(addr_abx());  clock_advance(7);  break;
		case 0xff:  cpu6510_ISB(addr_abx());  clock_advance(7);  break;
		}
	} /* keep looping until clock runs out: while (clock > 0) */
}

/* list of documented instructions:
   ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC,
   CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP,
   JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI,
   RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA
*/

void print_state() {
	int i;

	/* print register values */
	fprintf(stdout, "PC:%04x  A:%02x  X:%02x  Y:%02x  S:%02x  P:",
		reg_pc, reg_a, reg_x, reg_y, reg_s);

	/* make sure processor status is up to date */
	update_p();

	for (i=7; i>=0; i--)
		fprintf(stdout, "%01x", (reg_p >> i) & 1);

	/* print stack contents */

	for (i=0xff; i>reg_s; i--) fprintf(stdout, " %02x", mem_read(0x100 + i));

	fprintf(stdout, "\n");

	fflush(stdout);
}

/* new interrupt scheme:
   We need to keep track of a set of flags, each of which represents
   a possible interrupt source. Whenever these flags are updated, or
   whenever the interrupt mask is cleared, the flags need to be checked.
   If the set of flags is non-zero, then the irq is executed.

   Possible interrupt sources:
   VIC-II chip
   CIA 1
   CIA 2
*/

/* new clock scheme:
   Clock count starts at zero whenever the cpu is initialized.
   Every subsystem that needs something to happen at regular
   intervals sets a "time of next interrupt" amount.
   Inner CPU loop keeps a "time until next interrupt" value,
   which gets decremented after each instruction. When it
   reaches zero, the clock gets synchronized, and interrupts
   are serviced. Then we calculate a new "time until next
   interrupt" value.
*/

