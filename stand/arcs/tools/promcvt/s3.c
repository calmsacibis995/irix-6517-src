#include <stdio.h>
#include <sys/types.h>
#include <malloc.h>
#include "s3.h"

#define S3_DATA_RECORD 0x03
#define S3_EOF_RECORD 0x07

static char *hex_digits = "0123456789ABCDEF";
#define HEX( x ) ( hex_digits[ (int) x ] )

static char *data;
static unsigned int checksum;

/* Forward and external declarations */

size_t record_address;
size_t record_size;

static size_t
Hexify4(unsigned long source, char *destination )
{
	*destination++ = HEX( ((source >> 28) & 0xf));
	*destination++ = HEX( ((source >> 24) & 0xf));
	checksum += (source >> 24) & 0xff;
	*destination++ = HEX( ((source >> 20) & 0xf));
	*destination++ = HEX( ((source >> 16) & 0xf));
	checksum += (source >> 16) & 0xff;
	*destination++ = HEX( ((source >> 12) & 0xf));
	*destination++ = HEX( ((source >> 8) & 0xf));
	checksum += (source >> 8) & 0xff;
	*destination++ = HEX( ((source >> 4) & 0xf));
	*destination++ = HEX( source & 0xf);
	checksum += source & 0xff;

	return 8;
}

static size_t
Hexify1(unsigned char source, char *destination )
{
	*destination++ = HEX( ((source >> 4) & 0xf));
	*destination++ = HEX( source & 0xf);
	checksum += (unsigned) (source & 0xff);

	return 2;
}

static size_t
Hexify(char *source, char *destination, size_t length)
{
	register char src;
	register size_t i = length;
	register unsigned int cksum = 0;

	while ( i-- > 0 ) {

		/* Get the byte to be processed */
		src = *source;

		/* Convert it to hex ascii */
		*destination++ = HEX( (src >> 4) & 0xf);
		*destination++ = HEX( src & 0xf);
		cksum += (unsigned)src;

		/* Bump to next next byte to be processed */
		source++;
	}

	checksum += cksum;
	return length * 2;
}

int
s3_init(size_t rec_size)
{
	size_t length;

	record_size = rec_size;
	length = (size_t) (15 + (record_size * 2));
	return !(data = calloc( length, 1 ));
}


void
s3_setloadaddr(__psunsigned_t addr)
{
	record_address = addr;
}
	
void
s3_convert(char *buffer, size_t length)
{
	char *cdp = data;

	checksum = 0;
	*cdp++ = 'S';
	*cdp++ = HEX(S3_DATA_RECORD);
	cdp += Hexify1( (unsigned char) (length + S3_DATA_RECORD + 2) , cdp );

	cdp += Hexify4( record_address, cdp );

	cdp += Hexify( buffer, cdp, length );
	cdp += Hexify1( (unsigned char) (~checksum), cdp );
	
	record_address += length;
	*cdp = 0;
}

int
s3_write(FILE *ofile) 
{
	fputs(data, ofile);
	putc('\n', ofile);
	return 0;
}

int
s3_terminate(FILE *ofile, unsigned int entry)
{
	char *cdp = data;

	*cdp++ = 'S';
	*cdp++ = HEX(S3_EOF_RECORD);
	checksum = 0;

	cdp += Hexify1( 5, cdp );
	cdp += Hexify4( entry, cdp );

	cdp += Hexify1( (unsigned char) (~checksum), cdp );

	*cdp = 0;

	return s3_write(ofile);
}

