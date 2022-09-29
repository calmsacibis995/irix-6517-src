#ident	"$Id: sload.c,v 1.1 1994/07/21 01:16:09 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


/*
 * sdownload/sload command using SIO PORT B 
 *
 * On prom side: sload [-b]	
 *
 * On Unix side: sdownload [-b] /dev/ttyX < file.s3rec
 *
 * where	-b = binary sdownload
 *
 */

#define DIGIT(c)	((c)>'9'?((c)>='a'?(c)-'a':(c)-'A')+10:(c)-'0')


extern char _getchar_b(), _putchar_b();
extern int  date();

char ticks[] = {"|/-\\"};

/*
 * in-line implementation of get_pair for speed
 */
#define	get_pair() \
	( \
		c1 = _getchar_b() & 0x7f, \
		c2 = _getchar_b() & 0x7f, \
		byte = (DIGIT(c1) << 4) | DIGIT(c2) \
	)


int getc_csum(do_binary)
int do_binary;
{
	register c1, c2, byte;

	if (do_binary)
		byte = _getchar_b();
	else
		get_pair();
	return(byte);
}
	

#define	ACK	0x6
#define	NAK	0x15

#define srec_type byte	/* register variable aliase */

/*
 * BUG Warning: The "sdownload" command in spp currently sends "S000FF" as the
 * 		header srec record which gives a length of 0 rather that 1.
 *		That is, the correct header srec record should be "S001FE".
 *		But since length is signed, everything works just fine now.
 *		Don't ever change length to UNSIGNED!!! (2/15/90)
 */

int sload(do_binary)
register int do_binary; /* flag for binary sload, use sdownload -b /dev/ttyx < file.s3rec */
{
  register 	length, address;
  register	i, reccount, csum, byte, done;
  /* int		fd; */
  unsigned 	client_pc;     	/* initial client pc, set on load or promexec */
  /* int 		ack = 1; * always ack */

	reccount = 1;
	for (done = 0; !done; reccount++) {
		while ((i = (_getchar_b() & 0x7f)) != 'S') {
			if (i == 0) {
				printf("Error or EOF.\nsload aborted.\n");
				return(-1);
			}
		}
		csum = 0;
		srec_type = _getchar_b() & 0x7f;	/* read srec type */
/*
 * put the following if(){} back if diag.s3rec has "S0xxxxxx" in the beginning.
 *
 */
		if (reccount == 1) {
			if (srec_type != '0') {
				printf("\nMissed initial s-record.\n");
				return (-1);
			}
			date();
			printf("\n");
		}

		csum += (length = getc_csum(do_binary));
		if (length < 0 || length >= 256) {
			printf("record: invalid length.\n");
			return (-1);
		}

		length--;	/* save checksum for later */

		if (srec_type == '0') { /* initial record, ignored */
			while (length-- > 0)
				csum += getc_csum(do_binary);
		} else
		if (srec_type == '3') { /* data with 32 bit address */
			address = 0;
			for (i = 0; i < 4; i++) {
				address <<= 8;
				csum += (byte = getc_csum(do_binary));
				address |= byte;
				length--;
			}
			while (length-- > 0) {
				csum += (byte = getc_csum(do_binary));
				*(char *)address++ = byte;
			}
		} else
		if (srec_type == '7') { /* end of data, 32 bit initial pc */
			address = 0;
			for (i = 0; i < 4; i++) {
				address <<= 8;
				csum += (byte = getc_csum(do_binary));
				address |= byte;
				length--;
			}
			client_pc = address;
			if (length)
				printf("record: type 7 record with unexpected data.\n");
			done = 1;
		}
		else {
			printf("record: unknown record type.\n");
		}

		csum = (~csum) & 0xff;
		byte = getc_csum(do_binary);
		if (csum != byte) {
			printf("record: checksum error.\n");
			/* if (ack) ----- always ack */
				_putchar_b(NAK);
		} else /* if (ack)  ----- always ack */
			_putchar_b(ACK);

		i = reccount & 0x7F;
		if ((i & 0x1F) == 0) { /* print tick at every 32's count */
			printf("\r%c", ticks[i >> 5]);
		}
	}
	reccount -= 2;		/* minus header and last records */
	printf("\rSload done: %5d records.         ", reccount);
	date();
	printf("\nLoaded program pc: 0x%8x\n", client_pc);
	return(0);
}

/* end */
