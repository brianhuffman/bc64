/* serial.h - serial (disk) functions for c64 emulator */

#ifndef __SERIAL_H
#define __SERIAL_H

#define SERIAL_TIME_OUT (-1)
#define SERIAL_DEVICE_NOT_PRESENT (-2)
#define SERIAL_END_OF_FILE (256)

/* function declarations */
void serial_init();
int serial_read();
int serial_write(int atn, int a);

/*
typedef struct {
	int (*open)  (int channel, const char *command);
	int (*close) (int channel);
	int (*read)  (int channel);
	int (*write) (int channel, int size, const char *data);
} DiskDriver;
*/
/*
How the C1541 is called by the C64:

  LOAD "filename",8
    /28 /f0 filename /3f
    /48 /60 read data /5f
    /28 /e0 /3f

  SAVE "filename",8
    /28 /f1 filename /3f
    /28 /61 send data /3f
    /28 /e1 /3f

  OPEN 15,8,15,"string"
    /28 /ff string /3f

  PRINT# 15,"string"
    /28 /6f string\n /3f

  CLOSE 15
    /28 /ef /3f

I used '/' to denote bytes sent under Attention (ATN low).

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

#endif