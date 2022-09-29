/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.1 $"

/*
 *			f i l l m e m W _ l( )
 *
 * fillmemW- fills memory from the starting address to ending address with the
 * data pattern supplied. Or if the user chooses, writes are done from ending
 * address to the starting address. Writes are done in multiples of 8 to reduce
 * the overhead. The disassembled code takes 12 cycles for the 8 writes verses
 * 32 cycles if done 1 word at a time.
 *
 * inputs:
 *	begadr = starting address
 *	endadr = ending address
 *	pat1   = pattern 1
 *	direction = 0= write first to last, 1= write last to first
 */

#include <sys/types.h>
#define REVERSE 1
#define FORWARD 0


/*fillmemW(register u_int *begadr, register u_int *endadr, register u_int pat1,register u_int direct)
*/
fillmemW(begadr, endadr, pat1, direct)
register u_int *begadr;
register u_int *endadr;
register u_int pat1;
register u_int direct;
{
	/* This code is run in cached mode so a switch statement is not
	 * used because it will pop the code into uncached space since the
	 * code was linked in K1 space.
	 */
	if( direct == 0) {
		/*
	 	 * Do the writes in multiples of 8 for speed
	 	 */
		for( ;begadr <= (endadr -  8);) {
			*begadr = pat1;
			*(begadr + 1) = pat1;
			*(begadr + 2) = pat1;
			*(begadr + 3) = pat1;
			*(begadr + 4) = pat1;
			*(begadr + 5) = pat1;
			*(begadr + 6) = pat1;
			*(begadr + 7) = pat1;
			begadr += 8;
		}
		/*
	 	 * Finish up remaining writes 
	 	 */
		for ( ;begadr <= endadr; ) {
			*begadr++ = pat1;
		}
	}
	else {
		/*
	 	 * Do the writes in multiples of 8 for speed, from last-> first 
	 	 */
		for( ;endadr >= (begadr -  8);) {
			*endadr = pat1;
			*(endadr - 1) = pat1;
			*(endadr - 2) = pat1;
			*(endadr - 3) = pat1;
			*(endadr - 4) = pat1;
			*(endadr - 5) = pat1;
			*(endadr - 6) = pat1;
			*(endadr - 7) = pat1;
			endadr -= 8;
		}
		/*
	 	 * Finish up remaining writes 
	 	 */
		for ( ;endadr >= begadr; ) {
			*endadr-- = pat1;
		}
	}

}
