/* vic2.h - video functions for c64 emulator */
/* by Brian Huffman 12-14-00 */

/* this file should be totally portable--
   system specific stuff goes in video.c */

#ifndef __VIC2_H
#define __VIC2_H

void vic_init ();
void vic_mem_write(int address, int data);
unsigned char vic_mem_read(int address);

void vic_update_raster(int value);
void vic_print_state();

void callback_raster (void);
#endif
