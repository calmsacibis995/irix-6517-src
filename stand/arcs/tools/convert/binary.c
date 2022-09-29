/*
 * magic format handler.  binary format is incredibly trivial:
 *	
 *	data records consist of a record type, a four-byte address followed 
 *	by a 2-byte length followed by a stream of binary data bytes, 
 *	followed by a checksum.
 *
 *	end records consists of a record type and a four-byte start address.
 */

#include <stdio.h>
#include "convert.h"

#define DATA_REC	0xd	
#define END_REC		0xe	
#define BINARY	0x7

#define DATAHDR_SIZE	9	/* 1 byte record type + 4 byte address 
				   + 2 byte length + 2 byte checksum  */	
#define ENDHDR_SIZE	5	/* 1 byte record type + 4 byte entry point */


static int data_length;
static char data[6000];
static char *cdp;
static unsigned int checksum;

extern unsigned long record_address;
extern unsigned long start_address;
extern address data_record;

int
BinInitialize(int record_size)
{
	data_record = BINARY;
	return 0;
}


int
BinConvert(char *buffer, int length)
{
	unsigned int i;
	char *cdp = data;
	unsigned short checksum = 0;

	*cdp++ = (char) DATA_REC;

	/* Write out address, LSB first */
	*cdp++ = (char) (record_address & 0xff);
	*cdp++ = (char) ((record_address >>  8) & 0xff);
	*cdp++ = (char) ((record_address >> 16) & 0xff);
	*cdp++ = (char) ((record_address >> 24) & 0xff);	

	/* Write out length, LSB first */
	*cdp++ = (char) (length & 0xff);
 	*cdp++ = (char) ((length >> 8) & 0xff);

	/* Transfer data */
	for (i = 0; i < length; i++) {
		
		/* Calculate checksum */
		checksum += (unsigned short) *buffer;
		*cdp++ = *buffer++;
	}

	/* Stuff the checksum into the buffer, LSB first */
	*cdp++ = (char) (checksum & 0xff);
	*cdp++ = (char) ((checksum >> 8) & 0xff);

	fprintf(stderr, "Data Addr: 0x%x Length: %d Checksum: 0x%x\n",
	        record_address, length, checksum);

	data_length = length + DATAHDR_SIZE;
	record_address += length;

	return 0;
}


int
BinWrite(void)
{
	fwrite(data, data_length, 1, stdout);
	return 0;
}


int
BinTerminate(void)
{
	char *cdp = data;

	*cdp++ = (char) END_REC;

	/* Stuff in entry point address */
	*cdp++ = (char) ( start_address & 0xff);
	*cdp++ = (char) ((start_address >> 8) & 0xff);
	*cdp++ = (char) ((start_address >> 16) & 0xff);
	*cdp++ = (char) ((start_address >> 24) & 0xff);

	data_length = ENDHDR_SIZE;

	fprintf(stderr, "ENTRY POINT: 0x%x\n", start_address);
	return BinWrite();
}
