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

#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include "ide_msg.h"
#include "prototypes.h"
#define	TRUEDATA 0
#define NOTDATA 1
/*
 *		f i l l t a g _ 1( )
 *
 * filltag_1- fills memory from the starting address to ending address using 
 * the address as the data pattern. The writes are done in multiples of 8 to reduce
 * the overhead. User has the option of the data, i.e. the address as data or
 * the compliment of the address as data. 
 *
 * inputs:
 *	begadr = starting address
 *	endadr = ending address
 *	notdata= 0, use address as data; 1, use ~address as data
 */

void filltagW_l(u_int *begadr, u_int *endadr, uint notdata, uint direction)
{

	/* Switch statement will put code into uncached space since code
	 * was linked in k1 space.
	 */
	if( !direction) {
		if( !notdata) {
			/*
	 	 	 * Do the writes in multiples of 8 for speed
	 	 	 */
			for( ;begadr <= (endadr -  8);) {
				*begadr = (u_int)begadr;
				*(begadr + 1) = (u_int)(begadr + 1);
				*(begadr + 2) = (u_int)(begadr + 2);
				*(begadr + 3) = (u_int)(begadr + 3);
				*(begadr + 4) = (u_int)(begadr +4);
				*(begadr + 5) = (u_int)(begadr +5);
				*(begadr + 6) = (u_int)(begadr +6);
				*(begadr + 7) = (u_int)(begadr +7);
				begadr += 8;
			}
			/*
	 		 * Finish up remaining writes 
	 		 */
			for ( ;begadr <= endadr; ) {
				*begadr++ = (u_int)begadr;
			}

		}
		else {
			/*
	 	 	 * compliment address,do the writes in multiples of 8 for speed
	 	 	 */
			for( ;begadr <= (endadr -  8);) {
				*begadr = ~(u_int)begadr;
				*(begadr + 1) = ~(u_int)(begadr + 1);
				*(begadr + 2) = ~(u_int)(begadr + 2);
				*(begadr + 3) = ~(u_int)(begadr + 3);
				*(begadr + 4) = ~(u_int)(begadr +4);
				*(begadr + 5) = ~(u_int)(begadr +5);
				*(begadr + 6) = ~(u_int)(begadr +6);
				*(begadr + 7) = ~(u_int)(begadr +7);
				begadr += 8;
			}
			/*
	 		 * Finish up remaining writes 
	 		 */
			for ( ;begadr <= endadr; ) {
				*begadr++ = ~(u_int)begadr;
			}
	msg_printf(DBG, "begadr - 0x%x *begadr - 0x%x\n", begadr, *(uint *)begadr);
		}
	}
	else {
		if( !notdata) {
			/*
	 	 	 * Do the writes in multiples of 8 for speed
	 	 	 */
			for( ;endadr >= (begadr +  8);) {
				*endadr = (u_int)endadr;
				*(endadr - 1) = (u_int) (endadr - 1);
				*(endadr - 2) = (u_int) (endadr - 2);
				*(endadr - 3) = (u_int) (endadr - 3);
				*(endadr - 4) = (u_int) (endadr - 4);
				*(endadr - 5) = (u_int) (endadr - 5);
				*(endadr - 6) = (u_int) (endadr - 6);
				*(endadr - 7) = (u_int) (endadr - 7);
				endadr -= 8;
			}
			/*
	 	 	 * Finish up remaining writes 
	 	 	 */
			for ( ;endadr >= begadr; ) {
				*endadr-- = (u_int) endadr;
			}
		}
		else {
			/*
	 	 	 * Do the writes in multiples of 8 for speed
	 	 	 */
			for( ;endadr >= (begadr +  8);) {
				*endadr = ~(u_int)endadr;
				*(endadr - 1) = ~(u_int) (endadr - 1);
				*(endadr - 2) = ~(u_int) (endadr - 2);
				*(endadr - 3) = ~(u_int) (endadr - 3);
				*(endadr - 4) = ~(u_int) (endadr - 4);
				*(endadr - 5) = ~(u_int) (endadr - 5);
				*(endadr - 6) = ~(u_int) (endadr - 6);
				*(endadr - 7) = ~(u_int) (endadr - 7);
				endadr -= 8;
			}
			/*
	 	 	 * Finish up remaining writes 
	 	 	 */
			for ( ;endadr >= begadr; ) {
				*endadr-- = ~(u_int) endadr;
			}

		}
	}
}
