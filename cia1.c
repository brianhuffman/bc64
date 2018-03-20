/* cia1.c - keyboard functions for c64 emulator */
/* by Brian Huffman 12-24-00 */

/* this file should be totally portable--
   system specific stuff goes in keyboard.c */

#include <stdio.h>
#include "cia1.h"
#include "mem_c64.h"
#include "6510.h"
#include "keyboard.h"

/* declare variables to hold register contents, etc */
static int registers[0x10];
static int column_mask;
static int joy1_state, joy2_state;
static int irq_mask;
static int timerA, timerA_latch, alarmA;
static int timerB, timerB_latch, alarmB;

void callback_timer1A (void);
void callback_timer1B (void);

static void set_register(int address, int data) {
	int offset;
	for (offset = 0xdc00 + address; offset < 0xdd00; offset += 0x10) {
		mem_io_write(offset, data);
	}
}

void cia1_set_joysticks(int joy1, int joy2) {
	joy1_state = joy1;
	set_register(0x01, keyboard_read_rows(column_mask) & joy1_state);
	joy2_state = joy2;
	set_register(0x00, column_mask & joy2_state);
}


/******************** REGISTER MEMORY WRITE *************************/

void cia1_mem_write(int address, int data) {

	/* address space only covers 4 bits */
	address &= 0x0f;

	/* do any special functions that need to be taken care of */
	switch (address) {
	case 0x00:
		data |= (registers[0x02] ^ 0xff);
		column_mask = data;

		/* update keyboard row registers */
		set_register(0x01, keyboard_read_rows(column_mask) & joy1_state);
		data &= joy2_state;
		break;

	case 0x01:
		break;
	case 0x02:
	case 0x03:
		/* for data direction registers, 1=output, 0=input */
		/* usually set to 255; if a bit is set to zero, then it is
		as if the corresponding bit in reg 0 is always set to 1 */
		break;
	case 0x04: /* timer A low register */
		timerA_latch &= 0xff00;
		timerA_latch |= data;
		printf ("timerA_latch = %i\n", timerA_latch);
		break;
	case 0x05: /* timer A high register */
		timerA_latch &= 0x00ff;
		timerA_latch |= data << 8;
		printf ("timerA_latch = %i\n", timerA_latch);
		break;
	case 0x06: /* timer B low register */
		timerB_latch &= 0xff00;
		timerB_latch |= data;
		printf ("timerB_latch = %i\n", timerB_latch);
		break;
	case 0x07: /* timer B high register */
		timerB_latch &= 0x00ff;
		timerB_latch |= data << 8;
		printf ("timerB_latch = %i\n", timerB_latch);
		break;
	case 0x08:
		break;
	case 0x09:
		break;
	case 0x0a:
		break;
	case 0x0b:
		break;
	case 0x0c:
		break;

	case 0x0d: /* interrupt control register */
		/* set corresponding mask bit for each 1 */
		if (data & 128) irq_mask |= data;
		/* clear corresponding mask bit for each 1 */
		else irq_mask &= ~data;
		/* don't write mask to register */
		data = 0;
		break;

	case 0x0e: /* control register A */
		printf ("control reg A = %02x\n", data);
		/* force load if bit 4 is set */
		if (data & 16) {
			timerA = timerA_latch;
			data &= ~16;
		}

		/* stop or start depending on bit 1 */
		if (data & 1) {
			/* start counter */
			alarmA = cpu6510_clock() + timerA;
			cpu6510_callback (CB_TIMER1A, callback_timer1A, alarmA);
		} else {
			/* stop counter */
			timerA = cpu6510_clock() - alarmA;
			cpu6510_callback (CB_TIMER1A, NULL, alarmA);
		}
		break;
	case 0x0f: /* control register B */
		printf ("control reg B = %02x\n", data);
		/* force load if bit 4 is set */
		if (data & 16) {
			timerB = timerB_latch;
			data &= ~16;
		}

		/* stop or start depending on bit 1 */
		if (data & 1) {
			/* start counter */
			alarmB = cpu6510_clock() + timerB;
			cpu6510_callback (CB_TIMER1B, callback_timer1B, alarmB);
		} else {
			/* stop counter */
			timerB = cpu6510_clock() - alarmB;
			cpu6510_callback (CB_TIMER1B, NULL, alarmB);
		}
		break;
	}

	/* write 16 copies of registers into io space */
	registers[address] = data;
	set_register(address, data);
}


/******************** REGISTER MEMORY READ *************************/
unsigned char cia1_mem_read(int address)
{
	address &= 0x0f;
	/*
  | 0 | 0 | 0 | 0 | 0 | PRA      |  PERIPHERAL DATA REG A                 |
  | 0 | 0 | 0 | 1 | 1 | PRB      |  PERIPHERAL DATA REG B                 |
  | 0 | 0 | 1 | 0 | 2 | DDRA     |  DATA DIRECTION REG A                  |
  | 0 | 0 | 1 | 1 | 3 | DDRB     |  DATA DIRECTION REG B                  |
  | 0 | 1 | 0 | 0 | 4 | TA LO    |  TIMER A LOW REGISTER                  |
  | 0 | 1 | 0 | 1 | 5 | TA HI    |  TIMER A HIGH REGISTER                 |
  | 0 | 1 | 1 | 0 | 6 | TB LO    |  TIMER B LOW REGISTER                  |
  | 0 | 1 | 1 | 1 | 7 | TB HI    |  TIMER B HIGH REGISTER                 |
  | 1 | 0 | 0 | 0 | 8 | TOD 10THS|  10THS OF SECONDS REGISTER             |
  | 1 | 0 | 0 | 1 | 9 | TOD SEC  |  SECONDS REGISTER                      |
  | 1 | 0 | 1 | 0 | A | TOD MIN  |  MINUTES REGISTER                      |
  | 1 | 0 | 1 | 1 | B | TOD HR   |  HOURS-AM/PM REGISTER                  |
  | 1 | 1 | 0 | 0 | C | SDR      |  SERIAL DATA REGISTER                  |
  | 1 | 1 | 0 | 1 | D | ICR      |  INTERRUPT CONTROL REGISTER            |
  | 1 | 1 | 1 | 0 | E | CRA      |  CONTROL REG A                         |
  | 1 | 1 | 1 | 1 | F | CRB      |  CONTROL REG B                         |
  */
	return 0;
}	


/******************** INITIALIZATION ********************/
void cia1_init () {
	int x;
	for (x=0; x<0x0f; x++) cia1_mem_write(x, 0);
}


void callback_timer1A (void) {
	/* reload latch value */
	timerA = timerA_latch;
	
	/* is timer in continuous mode? */
	if (!(registers[0x0e] & 0x08)) {
		alarmA += timerA;
		cpu6510_callback (CB_TIMER1A, callback_timer1A, alarmA);
	}
	
	/* does it generate an IRQ? */
	if (irq_mask & 0x01) cpu6510_irq();
}

void callback_timer1B (void) {
	/* reload latch value */
	timerB = timerB_latch;
	
	/* is timer in continuous mode? */
	if (!(registers[0x0f] & 0x08)) {
		alarmB += timerB;
		cpu6510_callback (CB_TIMER1B, callback_timer1B, alarmB);
	}
	
	/* does it generate an IRQ? */
	if (irq_mask & 0x02) cpu6510_irq();
}


/* CIA memory map
                                REGISTER MAP
  +---+---+---+---+---+----------+----------------------------------------+
  |RS3|RS2|RS1|RS0|REG|   NAME   |                                        |
  +---+---+---+---+---+----------+----------------------------------------+
  | 0 | 0 | 0 | 0 | 0 | PRA      |  PERIPHERAL DATA REG A                 |
  | 0 | 0 | 0 | 1 | 1 | PRB      |  PERIPHERAL DATA REG B                 |
  | 0 | 0 | 1 | 0 | 2 | DDRA     |  DATA DIRECTION REG A                  |
  | 0 | 0 | 1 | 1 | 3 | DDRB     |  DATA DIRECTION REG B                  |
  | 0 | 1 | 0 | 0 | 4 | TA LO    |  TIMER A LOW REGISTER                  |
  | 0 | 1 | 0 | 1 | 5 | TA HI    |  TIMER A HIGH REGISTER                 |
  | 0 | 1 | 1 | 0 | 6 | TB LO    |  TIMER B LOW REGISTER                  |
  | 0 | 1 | 1 | 1 | 7 | TB HI    |  TIMER B HIGH REGISTER                 |
  | 1 | 0 | 0 | 0 | 8 | TOD 10THS|  10THS OF SECONDS REGISTER             |
  | 1 | 0 | 0 | 1 | 9 | TOD SEC  |  SECONDS REGISTER                      |
  | 1 | 0 | 1 | 0 | A | TOD MIN  |  MINUTES REGISTER                      |
  | 1 | 0 | 1 | 1 | B | TOD HR   |  HOURS-AM/PM REGISTER                  |
  | 1 | 1 | 0 | 0 | C | SDR      |  SERIAL DATA REGISTER                  |
  | 1 | 1 | 0 | 1 | D | ICR      |  INTERRUPT CONTROL REGISTER            |
  | 1 | 1 | 1 | 0 | E | CRA      |  CONTROL REG A                         |
  | 1 | 1 | 1 | 1 | F | CRB      |  CONTROL REG B                         |
  +---+---+---+---+---+----------+----------------------------------------+

  from C64 PRG, pp.428
*/

/*
startup register accesses:
CIA1: register 13 set to 127
  clear all interrupt mask bits
CIA1: register 14 set to 8
CIA1: register 15 set to 8
  bit0: START TIMER = no
  bit1: PBON = no; timer doesn't appear on PB6/7
  bit2: OUTMODE = pulse
  bit3: RUNMODE = one-shot
  bit4: LOAD = no
  bit5: INMODE = 02 pulses
  bit6: SPMODE = serial port input
  bit7: TODIN = 60 Hz clock

  bit5,6: INMODE = 02 pulses
  bit7: ALARM = writing TOD sets clock
  
CIA1: register 4 set to 149
CIA1: register 5 set to 66
CIA1: register 13 set to 129
  enable timer A interrupt
CIA1: register 14 set to 17
  force load and start timer A
CIA1: register 4 set to 37
CIA1: register 5 set to 64
CIA1: register 13 set to 129
CIA1: register 14 set to 17
*/
