/*
 *  disk_raw.h
 *  bc64
 *
 *  Created by Brian Huffman on Mon Sep 30 2002.
 *  Copyright (c) 2002 Brian Huffman. All rights reserved.
 *
 */

#include "serial.h"

//DiskDriver disk_raw;
void raw_init ();

int raw_open  (int channel, const char *command);
int raw_close (int channel);
int raw_read  (int channel);
int raw_write (int channel, int size, const char *data);
