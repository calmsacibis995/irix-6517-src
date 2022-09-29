/* $Header: /proj/irix6.5.7m/isms/stand/arcs/tools/convert/RCS/s_rec.c,v 1.2 1994/11/09 22:03:47 sfc Exp $ */
/* $Log: s_rec.c,v $
 * Revision 1.2  1994/11/09 22:03:47  sfc
 * Merge sherwood/redwood up and including redwood version 1.3
 * > ----------------------------
 * > revision 1.3
 * > date: 93/10/12 00:38:05;  author: jeffs;  state: ;  lines: +5 -4
 * > Use extern version of record_address.
 * > ----------------------------
 * > revision 1.2
 * > date: 93/07/15 19:48:51;  author: igehy;  state: ;  lines: +38 -46
 * > Made compile -fullwarn.
 * > ----------------------------
 *
 * Revision 1.3  1993/10/12  00:38:05  jeffs
 * Use extern version of record_address.
 *
 * Revision 1.2  1993/07/15  19:48:51  igehy
 * Made compile -fullwarn.
 *
 * Revision 1.1  1987/09/21  11:14:15  huy
 * Initial revision
 * */

#include <malloc.h>
#include "convert.h"
#include "s_rec.h"

/* 
 * Record types
 */
#define SIGNON_RECORD 0x00
#define S1_DATA_RECORD 0x01
#define S2_DATA_RECORD 0x02
#define S3_DATA_RECORD 0x03
#define S1_EOF_RECORD 0x09
#define S2_EOF_RECORD 0x08
#define S3_EOF_RECORD 0x07


static char *hex_digits = "0123456789ABCDEF";
#define HEX( x ) ( hex_digits[ (int) x ] )

static int data_length;
static char *data;
static unsigned int checksum;
static unsigned int eof_record;

/* Forward declarations */
static int	Hexify(char *, char *, int);
static int	Hexify1(unsigned char, char *);
static int	Hexify2(unsigned short, char *);
static int	Hexify3(unsigned long, char *);
static int	Hexify4(unsigned long, char *);


extern address record_address;
unsigned int data_record;

int
S1RecordInitialize(int record_size)
{
	unsigned int length;

	data_record = S1_DATA_RECORD;
	eof_record = S1_EOF_RECORD;

	length = 11 + (record_size * 2);

	return !(data = calloc( length, 1 ));
}

int
S2RecordInitialize(int record_size)
{
	unsigned int length;

	data_record = S2_DATA_RECORD;
	eof_record = S2_EOF_RECORD;

	length = 13 + (record_size * 2);

	return !(data = calloc( length, 1 ));
}


int
S3RecordInitialize(int record_size)
{
	unsigned int length;

	data_record = S3_DATA_RECORD;
	eof_record = S3_EOF_RECORD;

	length = 15 + (record_size * 2);

	return !(data = calloc( length, 1 ));
}

int
SRecordConvert(char *buffer, int length)
{
	char *cdp = data;

	checksum = 0;
	*cdp++ = 'S';
	*cdp++ = HEX( data_record );
	cdp += Hexify1( (unsigned int)length + data_record + 2 , cdp );

	/* Handle 16, 24 or 32 bit addresses as appropriate */
	switch ( data_record ) {
	case S1_DATA_RECORD:
		cdp += Hexify2( (unsigned int)record_address, cdp );
		break;
	case S2_DATA_RECORD:
		cdp += Hexify3( (unsigned int)record_address, cdp );
		break;
	default:
	case S3_DATA_RECORD:
		cdp += Hexify4( (unsigned int)record_address, cdp );
		break;
	}

	cdp += Hexify( buffer, cdp, length );
	cdp += Hexify1( ~checksum, cdp );

	record_address += length;
	*cdp = 0;
	return OK;
}

int
SRecordWrite(void)
{

	puts( data );
	return 0;
}

int
SRecordTerminate(void)
{
	char *cdp = data;

	*cdp++ = 'S';
	*cdp++ = HEX( eof_record );
	checksum = 0;
	switch ( eof_record ) {
	case S1_EOF_RECORD:
	case S2_EOF_RECORD:
		cdp += Hexify1( 3, cdp );
		cdp += Hexify2( 0, cdp );
		break;
	default:
	case S3_EOF_RECORD:
		cdp += Hexify1( 5, cdp );
		cdp += Hexify4( (unsigned int)start_address, cdp );
		break;
	}

	cdp += Hexify1( ~checksum, cdp );

	*cdp = 0;

	return SRecordWrite();
}

static int
Hexify4(unsigned long source, char *destination)
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

static int 
Hexify3(unsigned long source, char *destination)
{


	*destination++ = HEX( ((source >> 20) & 0xf));
	*destination++ = HEX( ((source >> 16) & 0xf));
	checksum += (source >> 16) & 0xff;
	*destination++ = HEX( ((source >> 12) & 0xf));
	*destination++ = HEX( ((source >> 8) & 0xf));
	checksum += (source >> 8) & 0xff;
	*destination++ = HEX( ((source >> 4) & 0xf));
	*destination++ = HEX( source & 0xf);
	checksum += source & 0xff;

	return 6;

}

static int
Hexify2(unsigned short source, char *destination)
{


	*destination++ = HEX( ((source >> 12) & 0xf));
	*destination++ = HEX( ((source >> 8) & 0xf));
	checksum += (source >> 8) & 0xff;
	*destination++ = HEX( ((source >> 4) & 0xf));
	*destination++ = HEX( source & 0xf);
	checksum += source & 0xff;

	return 4;

}

static int
Hexify1(unsigned char source, char *destination)
{


	*destination++ = HEX( ((source >> 4) & 0xf));
	*destination++ = HEX( source & 0xf);
	checksum += source & 0xff;

	return 2;

}

static int
Hexify(register char *source, register char *destination, int length)
{

	register char src;
	register int i = length;
	register unsigned int cksum = 0;
	register int index;
	register int increment;

	/* For split converts chose which byte of the instruction */
	index = byte_number < 0 ? 0 : byte_number;
	increment = byte_number < 0 ? 1 : 4;

	while ( i-- > 0 ) {

		/* Get the byte to be processed */
		src = source[ index ];

		/* Convert it to hex ascii */
		*destination++ = HEX( (src >> 4) & 0xf);
		*destination++ = HEX( src & 0xf);
		cksum += (unsigned)src;

		/* Bump to next next byte to be processed */
		source += increment;
	}

	checksum += cksum;
	return length * 2;

}
