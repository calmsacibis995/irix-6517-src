/*
 * Module to dump pure (raw) data.
 * Also prints global sum of bytes.
 */

#include <stdio.h>
#include "convert.h"

#define BINARY	0x7

static int data_length;
static char data[6000];
static unsigned long bytesum = 0;

extern unsigned long record_address;
extern address data_record;

int
PureInitialize(int record_size)
{
	data_record = BINARY;
	return 0;
}


int
PureConvert(char *buffer, int length)
{
	unsigned int i;

	/* Transfer data */
	for (i = 0; i < length; i++) {
		/* Calculate bytesum */
		bytesum += ((unsigned char *) buffer)[i];
		data[i] = buffer[i];
	}

	data_length = length;
	record_address += length;

	return 0;
}

int
PureRead(char *buffer, int length, int fd)
{
	if (length > 0)
		return read(fd, buffer, length);
	return 0;
}

int
PureWrite(void)
{
	fwrite(data, data_length, 1, stdout);
	return 0;
}


int
PureTerminate(void)
{
	fprintf(stderr, "Byte sum = 0x%016lx\n", bytesum);
	return 0;
}

int
PureClose(int fd)
{
	return 0;
}

