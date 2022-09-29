/*
 * Speed Racer module dumps
 *
 * for Speed Racer, there are two kinds of prom images
 *   rprom: recovery prom	checks fprom and jumps to it
 *   fprom: full prom		real 'prom' functionality
 *
 */

#include <unistd.h>
#include <stdio.h>
#include "convert.h"
#include <sys/types.h>
#include <sys/time.h>
#include "sys/RACER/sflash.h"

#define BINARY	0x7

static int data_length;
static char data[6000];

static int nwritten = 0;		/* rprom header at end of data */
static int fprom_headdone = 0;		/* fprom header is at start */

static int version = 0;
static uint32_t racer_checksum = 0;
static uint32_t racer_nbytes = 0;

extern unsigned long record_address;
extern address data_record;

/*
 * COMMON ROUTINES
 */
static int
checksum_addto(uint32_t *checksum, char value)
{
	/* Calculate checksum (sum -r algorithm) */
	if (*checksum & 01)
		*checksum = (*checksum >> 1) + 0x8000;
	else
		*checksum >>= 1;

	*checksum += value;
	*checksum &= 0xFFFF;
}


static uint16_t
in_cksum(uint8_t *data, int len, uint32_t  sum)
{
	if ((int)data & 1) {
	    sum += *data;
	    data++;
	    len--;
	}
	while (len > 1) {
	    sum  += *((uint16_t *)data);
	    len  -= 2;
	    data += 2;
	}
	if (len) {
	    sum = *data;
	}
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);
	return (uint16_t)sum;
}


static void
header_timestamp(flash_header_t *h)
{
	time_t ut = time(0);
	struct tm *gmt = gmtime(&ut);

	h->timestamp.Year  = gmt->tm_year;
	h->timestamp.Month = gmt->tm_mon + 1;
	h->timestamp.Day   = gmt->tm_mday;
	h->timestamp.Hour  = gmt->tm_hour;
	h->timestamp.Year  = gmt->tm_year + 1900;
}


int
RacerConvert(char *buffer, int length)
{
	unsigned int i;

	/* Transfer data */
	for (i = 0; i < length; i++) {
		checksum_addto( &racer_checksum, buffer[i] );
		racer_nbytes++;
		data[i] = buffer[i];
	}

	data_length = length;
	record_address += length;

	return 0;
}

int
RacerRead(char *buffer, int length, int fd)
{
	if (length > 0)
		return read(fd, buffer, length);
	return 0;
}

int
RacerClose(int fd)
{
	return 0;
}

int
RacerSetVersion(int vers)
{
	version = vers;
}

/*
 * RPROM specific routines
 *
 * an RPROM has a magic cookie and version information as the last
 * 64 bytes of an RPROM image.
 */

int
RacerRpromInitialize(int record_size)
{
	data_record = BINARY;
	return 0;
}

int
RacerRpromWrite(void)
{
	fwrite(data, data_length, 1, stdout);
	nwritten += data_length;		/* track how many written */
	return 0;
}


int
RacerRpromTerminate(void)
{
	char pad = 0xff;
	int end_offset;
	int datalen = nwritten;
	flash_header_t trailer;

	end_offset = SFLASH_RPROM_NSEGS * SFLASH_SEG_SIZE;
	/*
	 * fill the holes with -1's
	 */
	while (nwritten < (end_offset - FLASH_HEADER_SIZE)) {
		fwrite( &pad, 1, 1, stdout );
		nwritten++;
	}

	trailer.magic = FLASH_PROM_MAGIC;
	trailer.version = version;
	trailer.dcksum = racer_checksum;
	trailer.datalen = datalen;
	trailer.nflashed = 1;
	header_timestamp(&trailer);
	/*
	 * Although we re-calculate a new check at flash time
	 * with nflashed and timestamp values determined then
	 * we still need to protect the datacksum just calculated.
	 */
	trailer.hcksum = 0;
	trailer.hcksum = ~in_cksum((char *)&trailer, sizeof(trailer), 0);

	fwrite((char *)&trailer, sizeof(flash_header_t), 1, stdout );
	nwritten += sizeof(flash_header_t);
	while (nwritten < (end_offset)) {
		fwrite( &pad, 1, 1, stdout );
		nwritten++;
	}

	return 0;
}

/*
 * FPROM specific routines
 */

FILE * tfile;
char tmpfilename[L_tmpnam];

int
RacerFpromInitialize(int record_size)
{
	data_record = BINARY;

	if (tmpnam(tmpfilename) == NULL) {
		fprintf(stderr,"tmpnam failed\n");
		return -1;
	}

	tfile = fopen( tmpfilename, "w+" );
	if (tfile == NULL) {
		fprintf(stderr,"open of tmp file %s failed\n",tmpfilename);
		return -1;
	}

	return 0;
}

RacerFpromWrite(void)
{
	fwrite(data, data_length, 1, tfile);
	return 0;
}


int
RacerFpromTerminate(void)
{
	flash_header_t header;
	char buffer[128];
	char pad = 0xff;
	int rc;
	int i;

	header.magic = FLASH_PROM_MAGIC;
	header.version = version;
	header.dcksum = racer_checksum;
	header.datalen = racer_nbytes;
	header.nflashed = 1;
	header_timestamp(&header);
	/*
	 * Although we re-calculate a new check at flash time
	 * with nflashed and timestamp values determined then
	 * we still need to protect the datacksum just calculated.
	 */
	header.hcksum = 0;
	header.hcksum = ~in_cksum((char *)&header, sizeof(header), 0);

	/*
	 * write out the header, and fill out the FLASH_HEADER_SIZE bytes
	 * with zeros
	 */
	fwrite((char *)&header, sizeof(header), 1, stdout);

	for (i=sizeof(header); i<FLASH_HEADER_SIZE; i++) {
		fwrite(&pad, 1, 1, stdout);
	}

	/*
	 * get back to the beginning of the temporary data file,
	 * and dump that puppie out 
	 */
	rewind(tfile);

	while (!feof(tfile)) {
		rc = fread( buffer, 1, 128, tfile );
		if (rc > 0) fwrite( buffer, 1, rc, stdout );
	}

	fclose( tfile );
	unlink( tmpfilename );

	return 0;
}
