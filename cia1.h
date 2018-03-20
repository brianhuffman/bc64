/* cia1.h - keyboard functions for c64 emulator */
/* by Brian Huffman 12-24-00 */

/* this file should be totally portable--
   system specific stuff goes in keyboard.c */

void cia1_mem_write(int address, int data);

void cia1_init ();
void cia1_set_joysticks(int joy1, int joy2);

unsigned char cia1_mem_read(int address);
