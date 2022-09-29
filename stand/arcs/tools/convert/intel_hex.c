/* $Revision: 1.4 $ */
/* $Log: intel_hex.c,v $
 * Revision 1.4  1994/11/09 22:03:17  sfc
 * Merge sherwood/redwood up and including redwood version 1.4
 * > ----------------------------
 * > revision 1.4
 * > date: 93/07/15 19:46:40;  author: igehy;  state: ;  lines: +18 -30
 * > Made compile -fullwarn.
 * > ----------------------------
 *
 * Revision 1.4  1993/07/15  19:46:40  igehy
 * Made compile -fullwarn.
 *
 * Revision 1.3  1991/09/09  14:05:29  sfc
 * Added more IntHex 86 capability.
 *
 * Revision 1.2  90/08/16  09:56:37  kevinm
 * Added capability of writing extended address records so
 * address spaces of > 64 K can be hexified.
 * 
 * Revision 1.1  87/09/21  11:14:05  huy
 * Initial revision
 *  */

#include <stdio.h>
#include <malloc.h>
#include "convert.h"
#include "intel_hex.h"

/* 
 * Record types
 */
#define DATA_RECORD 0x00
#define EOF_RECORD  0x01
#define ADDR_RECORD 0x02

#define HEXIFY1( source, destination ) \
	*destination++ = HEX( ((source >> 4) & 0xf)); \
	*destination++ = HEX( source & 0xf); \
	checksum += source & 0xff;
static char *hex_digits = "0123456789ABCDEF";
#define HEX( x ) ( hex_digits[ (int)x ] )

static int	Hexify(char *, char *, int);
static int	Hexify2(unsigned short, char *);
static int	Hexify_short(char *, char *, int);

static char *data;
address record_address;
static address record_offset;
static unsigned int checksum;


int
IntelhexInitialize(int record_size)
{
	unsigned int length;

	length = 12 + (record_size * 2);

	return !(data = calloc( length, 1 ));
}

int
StepConvert(char *buffer, int length)
{
	char *cdp = data;

	checksum = 0;
	*cdp++ = ':';
	HEXIFY1( (unsigned int)length, cdp );
	cdp += Hexify2( (unsigned int)record_address, cdp );
	HEXIFY1( DATA_RECORD, cdp );
	cdp += Hexify( buffer, cdp, length );
	HEXIFY1( -checksum, cdp );

	record_address += length / format->blocking_factor;
	cdp = 0;

	return OK;
}

int
IntelhexConvert(char *buffer, int length)
{
	char *cdp = data;
#define MAXADDRMASK	0xffff0000
#define MAXADDR		65536

	/* if we are about to cross over a 64 K boundary, then
	 * create an additional offset record
	 */
	if (record_address & MAXADDRMASK) {
		fprintf(stderr, "IntelHexConvert: Bad record address\n");
		return 1;
	}
	if (record_address == 0) {
		/* return error if the record is not on an even boundary */

		checksum = 0;
		*cdp++ = ':';
		HEXIFY1( (unsigned int)2, cdp );
		cdp += Hexify2( (unsigned int)0, cdp );
		HEXIFY1( ADDR_RECORD, cdp );
		cdp += Hexify2( (unsigned int)record_offset<<12, cdp );
		record_offset += 1;
		HEXIFY1( -checksum, cdp );
		*cdp = 0;
		IntelhexWrite();
		record_address &= ~MAXADDRMASK;
		cdp = data;
	}

	if (((record_address + length) & MAXADDRMASK) &&
	    ((record_address + length) & ~MAXADDRMASK)) {
		/* This record is going to overflow the 16 bit address space.
		 * First, put out a data record to fill out to the
		 * end of the 16 bit address range, then reset
		 * record address to zero and put out an extended address
		 * record. Falling through to the normal case code puts out
		 * the remainder of the data after the wrap.
		 */

		unsigned int shortlen;

		shortlen = MAXADDR - record_address;
		checksum = 0;
		*cdp++ = ':';
		HEXIFY1(shortlen, cdp);
		cdp += Hexify2((unsigned int)record_address, cdp);
		HEXIFY1(DATA_RECORD, cdp);
		if (short_number < 0) 
			cdp += Hexify(buffer, cdp, shortlen);
		else
			cdp += Hexify_short(buffer, cdp, shortlen);
		HEXIFY1(-checksum, cdp);
		record_address += shortlen;
		*cdp = 0;
		IntelhexWrite();
		if (record_address & ~MAXADDRMASK) {
			fprintf(stderr, "Bad address\n");
			return 1;
		}
		record_address = 0;
		length -= shortlen;
		cdp = data;

		/* Now, put out a 'extended address' record */
		checksum = 0;
		*cdp++ = ':';
		HEXIFY1( (unsigned int)2, cdp );
		cdp += Hexify2( (unsigned int)0, cdp );
		HEXIFY1( ADDR_RECORD, cdp );
		cdp += Hexify2( (unsigned int)record_offset<<12, cdp );
		record_offset += 1;
		HEXIFY1( -checksum, cdp );
		*cdp = 0;
		IntelhexWrite();
		cdp = data;
	}

	checksum = 0;
	*cdp++ = ':';
	HEXIFY1( (unsigned int)length, cdp );
	cdp += Hexify2( (unsigned int)record_address, cdp );
	HEXIFY1( DATA_RECORD, cdp );
	if ( short_number < 0 ) 
		cdp += Hexify( buffer, cdp, length );
	else
		cdp += Hexify_short( buffer, cdp, length );
	HEXIFY1( -checksum, cdp );
	record_address += length;
	if (record_address & MAXADDRMASK) {
		if (record_address & ~MAXADDRMASK) {
			/* If this happens, it is a coding error. This
			 * case is supposed to be handled above.
			 */
			fprintf(stderr, "Intel_hex: record address error\n");
			return 1;
		}
		/* Reset record_address to zero. The next time
		 * through will print the extended address record,
		 * followed by the data record.
		 */
		record_address = 0;
	}

	*cdp = 0;

	return OK;
}

int
IntelhexWrite(void)
{
	puts( data );

	return 0;
}

int
IntelhexTerminate(void)
{
	char *cdp = data;

	checksum = 0;

	*cdp++ = ':';
	HEXIFY1( 0, cdp );
	cdp += Hexify2( (unsigned int)start_address, cdp );
	HEXIFY1( EOF_RECORD, cdp );
	HEXIFY1( -checksum, cdp );

	*cdp = 0;

	return IntelhexWrite();
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
	register int index = 0;
	register int increment = 1;

	/* For split converts chose which byte of the instruction */
	if ( byte_number >= 0 ) {
		index = byte_number;
		increment = 4;
	}

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

static int
Hexify_short(register char *source, register char *destination, int length)
{

	register char src;
	register int i = length;	/* length in bytes */
	register unsigned int cksum = 0;
	register int index = 0;
	register int increment = 1;
	int first = 1;

	/* For split converts chose which short of the instruction */
	if ( short_number >= 0 ) {
		index = short_number;	/* 0 or 2 */
		increment = 4;
	}

	while ( i-- > 0 ) {

		/* Get the byte to be processed */
		src = source[ index ];

		/* Convert it to hex ascii */
		*destination++ = HEX( (src >> 4) & 0xf);
		*destination++ = HEX( src & 0xf);
		cksum += (unsigned)src;

		/* Bump to next next byte to be processed */
		if ( first ) {
			first = 0;
			index++;
		}
		else {	/* go to next word */
			first = 1;
			index = short_number;
			source += increment;
		}
	}

	checksum += cksum;
	return length * 2;
}
