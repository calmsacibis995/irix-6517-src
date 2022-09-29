/*
 * Copyright (c) 1995 Spider Systems Limited
 *
 * This Source Code is furnished under Licence, and may not be
 * copied or distributed without express written agreement.
 *
 * All rights reserved.  Made in Scotland.
 *
 * Authors: A. Robertson, P. Woodhouse, D. Walker, J. Stewart, G. Wilkie
 *
 * snid.c of snet module
 *
 * SpiderFRAME-RELAY
 * Release 1.0.3 95/06/15
 * 
 * 
 */

#include <stdio.h>
#include <ctype.h>
#include <sys/snet/uint.h>
#include "snid.h"

/*
 *  strtosnid - Takes between a 1 and 4 character string
 *              identifier and converts it to a unsigned long
 */
unsigned long strtosnid(char *snid)
{
	uint8	 posn = 0;
	unsigned long new_snid = 0;

	if (snid == NULL)
		return(0);

	while (*snid && isalnum((int)*snid) && posn < (uint8) SN_ID_LEN)
	{
		/*
		 * First char should be in LSB, 
		 * Next char should be bit-shifted 8 bits left,
		 * Next char should be bit-shifted 16 bits left etc.
		 */
		new_snid += (((unsigned long) *snid) << (posn * 8));
		posn++;
		snid++;
	}
	return (new_snid);
}

/*
 *  snidtostr - Takes an unsigned long identifier and converts it to 
 *              between a 1 and 4 character string
 */
int
snidtostr(unsigned long snid, char *str)
{
	int	 posn = 0;

	if (str == (char *) NULL)
		return(0);

	while (snid && isalnum((int)(snid & 0xff)) && posn < SN_ID_LEN)
	{
		/*
		 * First char comes from LSB, 
		 * Next char should be bit-shifted 8 bits right,
		 * Next char should be bit-shifted 16 bits right etc.
		 */
		*str++ = (snid & 0xff);
		snid >>= 8;
		posn++;
	}
	return (posn);
}
