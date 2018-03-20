/* 6510.h - processor emulation for C64 emulator */
/* by Brian Huffman 11-29-00 */

//void cpu6510_main (int cycles);
void cpu6510_main (void);

void cpu6510_reset (void);
void cpu6510_irq (void);
void cpu6510_nmi (void);
void cpu6510_brk (void);

void print_state (void);

void cpu6510_bad_line (void);

void cpu6510_callback (int source, void (*callback)(void), int time);
int cpu6510_clock (void);

enum {
	CB_NONE,
	CB_MAIN,
	CB_RASTER,
	CB_REDRAW,
	CB_FRAME,
	CB_TIMER1A,
	CB_TIMER1B,
	CB_MAX
};
/*
typedef struct CPUState {

	// processor registers
	// 16-bit: pc
	// 8-bit: a,x,y,s,p
	// 1-bit: c,z,i,d,b,v,n
	int reg_pc;
	int reg_a;
	int reg_x;
	int reg_y;
	int reg_s;
	int reg_p;
	int flag_n;
	int flag_z;
	int flag_c;

	// cycle counter
	int clock;
} CPUState;
*/













