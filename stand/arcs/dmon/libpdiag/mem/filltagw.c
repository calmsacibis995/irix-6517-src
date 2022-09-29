static char rcsid[] = "$Header: /proj/irix6.5.7m/isms/stand/arcs/dmon/libpdiag/mem/RCS/filltagw.c,v 1.1 1994/07/21 00:58:03 davidl Exp $";

/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */

#include <sys/types.h>
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

/*filltag(register u_int *begadr, register u_int *endadr, u_int notdata)*/
filltagW_l(begadr, endadr, notdata, direction)
register u_int *begadr;
register u_int *endadr;
u_int direction;
u_int notdata;
{
	register u_int pat1;

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
