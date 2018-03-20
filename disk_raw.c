/*
 *  disk_raw.c
 *  bc64
 *
 *  Created by Brian Huffman on Mon Sep 30 2002.
 *  Copyright (c) 2002 Brian Huffman. All rights reserved.
 *
 */

#include <string.h>
#include <stdio.h>
#include <unistd.h> // chdir
#include <stdlib.h> // malloc free

#include "disk_raw.h"
#include "serial.h"

typedef enum {
	CHANNEL_CLOSED,
	CHANNEL_READ,
	CHANNEL_WRITE,
} ChannelMode;

typedef struct {
	ChannelMode mode;
	unsigned char *buffer;
	int buffer_len;
	int buffer_pos;
	unsigned char filename[64];
} ChannelInfo;

ChannelInfo channel[16];
char error_buffer[64];
char *dirname = "/Users/brian/Backup/c64/games";

/* the disk driver interface */
//DiskDriver disk_raw = {raw_open, raw_close, raw_read, raw_write};

/* pet-to-ascii conversion */
void convert_filename (const unsigned char *src, char *dest) {
	int i;
	for (i = 0; i<63; i++) {
		if (src[i] == 0) break;
		/* remap lower case chaacters */
		if (src[i] >= 65 && src[i] <= 90) dest[i] = src[i] + 32;
		/* remap upper case characters */
		else if (src[i] >= 193 && src[i] <= 218) dest[i] = src[i] - 128;
		/* other chars map directly */
		else dest[i] = src[i];
	}
	/* put terminating zero on filename */
	dest[i] = 0;
}

/* set error message buffer with error code */
static int condition (int code, char *description, int track, int block) {
	snprintf (error_buffer, 64, "%i %s %i %i", code, description, track, block);
	printf ("%s\n", error_buffer);
	return (code ? -1 : 0);
}

static int command_channel (const char *cmd) {
	if (strlen(cmd) > 58) return condition (32, "syntax error", 0, 0);
	else return condition (0, "ok", 0, 0);
}

static int match (const char *pat, const char *str) {
	if (*pat == '*') return 1;
	else if (*pat == *str && *pat == 0) return 1;
	else if (*pat == *str || *pat == '?') return match (pat+1, str+1);
	else return 0;
}
	
static int load_file (int ch, const char *cmd) {
	FILE *file;
	int size;
	
	if (strlen(cmd) > 63) return condition (32, "syntax error", 0, 0);
	convert_filename (cmd, channel[ch].filename);
	
	chdir (dirname);
	printf ("opening \"%s\"\n", channel[ch].filename);
	file = fopen (channel[ch].filename, "r");
	
	if (file == NULL) return condition (62, "file not found", 0, 0); 
	
	fseek (file, 0, SEEK_END);
	size = ftell (file);
	rewind (file);
	channel[ch].buffer = malloc (size);
	
	if (channel[ch].buffer == NULL) return condition (20, "read error", 0 ,0);
	
	channel[ch].buffer_len = fread (channel[ch].buffer, 1, size, file);
	channel[ch].buffer_pos = 0;
	channel[ch].mode = CHANNEL_READ;
	fclose(file);
	
	printf ("channel %i, loaded %i bytes\n", ch, size);

	return condition (0, "ok", 0, 0);
}
static int load_directory (int ch, const char *cmd) {
	return condition (0, "ok", 0, 0);
}

/**************************************************************************************/

void raw_init () {
	int i;
	for (i=0; i<16; i++) {
		channel[i].mode = CHANNEL_CLOSED;
		channel[i].filename[0] = 0;
		channel[i].buffer = NULL;
		channel[i].buffer_len = 0;
		channel[i].buffer_pos = 0;
	}
	channel[15].buffer = error_buffer;
}		

int raw_open (int ch, const char *cmd) {
	printf ("channel %i, open %s\n", ch, cmd);
	
	if (channel[ch].mode != CHANNEL_CLOSED) raw_close (ch);
	
	if (cmd[0] == '$') return load_directory (ch, cmd);
	
	if (ch == 0) return load_file (ch, cmd);
	//if (ch == 1) return save_file (ch, cmd);
	if (ch == 15) return command_channel(cmd);

	convert_filename(cmd, channel[ch].filename);
	
	return condition (0, "ok", 0, 0);
}

int raw_close (int ch) {
	printf ("channel %i, close\n", ch);

	channel[ch].mode = CHANNEL_CLOSED;
	if (ch != 15 && channel[ch].buffer != NULL) free (channel[ch].buffer);
	return condition (0, "ok", 0, 0);
}

int raw_read (int ch) {
	int result;

	if (channel[ch].buffer == NULL) {
		return condition (61, "file not open", 0, 0);
	}
	if (channel[ch].buffer_pos >= channel[ch].buffer_len) {
		return SERIAL_TIME_OUT;
	}

	result = channel[ch].buffer[channel[ch].buffer_pos++];
	
	if (channel[ch].buffer_pos == channel[ch].buffer_len) {
		printf ("end of file\n");
		return (result | SERIAL_END_OF_FILE);
	}
	else return result;
}

int raw_write (int ch, int size, const char *data) {
	FILE *file;

	if (ch == 15) return command_channel(data);

	if (channel[ch].mode != CHANNEL_WRITE) {
		return condition (61, "file not open", 0, 0);
	}
	return 0;
}
