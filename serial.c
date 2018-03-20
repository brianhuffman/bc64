/* serial.c - serial (disk) functions for c64 emulator */

#include <stdio.h>
#include <string.h>
#include "serial.h"
#include "disk_raw.h"

#define NUM_DEV 32
/* declare status word conditions */
#define TIME_OUT_WRITE (1<<0)
#define TIME_OUT_READ  (1<<1)
#define READ_ERROR     (1<<4)
#define END_OF_FILE    (1<<6)
#define DEVICE_NOT_PRESENT (1<<7)

/* declare mode constants */
enum {
	STANDBY,
	LSN_SEC,
	TLK_SEC,
	LSN_OPEN,
	TLK_LOAD,
	LSN_SAVE,
	LSN_CLOSE
};

/* declare disk file formats */
enum { D64, T64, RAW };

static int device = 0x1f;
static int second = 0;

#define BUFFER_SIZE (256)
static char buffer[BUFFER_SIZE];
static int buffer_pos = 0;

/* initialization */
void serial_init () {
	raw_init();
}

/************ READ from serial port **************/

int serial_read() {
	if (device != 0x08) return SERIAL_TIME_OUT;
	
	return raw_read (second & 0x0f);
}


/************ WRITE to serial port **************/

void flush_buffer () {
	int channel = second & 0x0f;
	if (device != 0x08) return;
	
	switch (second & 0xf0) {
	case 0x60:
		if (buffer_pos > 0) raw_write (channel, buffer_pos, buffer);
		break;
	case 0xe0:
		raw_close (channel);
		break;
	case 0xf0:
		raw_open (channel, buffer);
		break;
	}
	buffer_pos = 0;
}

int serial_write(int atn, int a) {
	if (atn) {
		flush_buffer();

		printf("serial ATN: %02x\n", a);
		second = a;
	
		if (a < 0x60) device = a & 0x1f;
		else if (device != 0x08) return SERIAL_DEVICE_NOT_PRESENT;
	}
	else {
		if (second < 0x60) return SERIAL_TIME_OUT;
	
		printf("serial: %02x\n", a);

		buffer[buffer_pos++] = a;
		if (buffer_pos == BUFFER_SIZE) flush_buffer();
		else buffer[buffer_pos] = 0;
	}
	return 0;
}

/*
  +---------+------------+---------------+------------+-------------------+
  |  ST Bit | ST Numeric |    Cassette   |   Serial   |    Tape Verify    |
  | Position|    Value   |      Read     |  Bus R/W   |      + Load       |
  +---------+------------+---------------+------------+-------------------+
  |    0    |      1     |               |  time out  |                   |
  |         |            |               |  write     |                   |
  +---------+------------+---------------+------------+-------------------+
  |    1    |      2     |               |  time out  |                   |
  |         |            |               |    read    |                   |
  +---------+------------+---------------+------------+-------------------+
  |    2    |      4     |  short block  |            |    short block    |
  +---------+------------+---------------+------------+-------------------+
  |    3    |      8     |   long block  |            |    long block     |
  +---------+------------+---------------+------------+-------------------+
  |    4    |     16     | unrecoverable |            |   any mismatch    |
  |         |            |   read error  |            |                   |
  +---------+------------+---------------+------------+-------------------+
  |    5    |     32     |    checksum   |            |     checksum      |
  |         |            |     error     |            |       error       |
  +---------+------------+---------------+------------+-------------------+
  |    6    |     64     |  end of file  |  EOI line  |                   |
  +---------+------------+---------------+------------+-------------------+
  |    7    |   -128     |  end of tape  | device not |    end of tape    |
  |         |            |               |   present  |                   |
  +---------+------------+---------------+------------+-------------------+
*/

