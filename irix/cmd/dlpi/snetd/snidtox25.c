/******************************************************************
 *
 *  SpiderX25 - Spider X.25 Utility
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *  SNIDTOX25.C
 *
 *
 *******************************************************************
 *
 *
 *       /projects/common/PBRAIN/SCCS/pbrainF/dev/src/lib/sx25/0/s.snidtox25.c
 *      @(#)snidtox25.c	1.6
 *
 *      Last delta created      10:55:32 12/10/91
 *      This file extracted     14:53:07 11/13/92
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/snet/uint.h>

/*
 *  snidtox25 - Takes between a 1 and 4 character string
 *              identifier and converts it to a unsigned 32bit
 */
uint32 snidtox25(snid)

unsigned char *snid;
{
	uint8	posn = 0;
	uint32  new_snid = 0;

	if (snid == (unsigned char *) NULL)
		return(0);

	/* 
	 * Only use one character snids at present.
	 * While loop left in for future expansion to 4 chars 
	 */
	while (*snid && isalnum(*snid) && posn < 1)
	{
		/*
		 * First char should be in LSB, 
		 * Next char should be bit-shifted 8 bits left,
		 * Next char should be bit-shifted 16 bits left etc.
		 */
		new_snid += (((uint32) *snid) << (posn * 8));
		posn++;
		snid++;
	}
	return (new_snid);
}
