/*
 * marchxy.c
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 * marchxy.c -
 * Described in van de Goor's book, "Testing Semiconductor Memories".
 */
#ident "ide/godzilla/mem/marchxy.c: $Revision: 1.2 $	"

#include <sys/types.h>
#include "d_mem.h"

#define MARCHX_ERR	1
#define MARCHY_ERR	2

/* Described in van de Goor's book, "Testing Semiconductor Memories" and 
 * has the following flow:
 * 
 * (w0), u(r0,w1), d(r1,w0), (r0)
 *
 * Will detect address decoder faults, stuck-at-faults, transition faults,
 * coupling faults, and inversion coupling faults
 */
int
marchX(__psunsigned_t first_loc, __psunsigned_t last_loc)
{
	/*extern int TimerHandler();*/
	register __psunsigned_t *lomem = (__psunsigned_t *)first_loc;
	register __psunsigned_t *himem = (__psunsigned_t *)last_loc;
	register __psunsigned_t mask = FILL_F;
	register __psunsigned_t data, *ptr;
	register int fail = 0;
	__psunsigned_t *hiptr;

	msg_printf(INFO, "\nMarchX Test from 0x%x to 0x%x\n",
	    first_loc, last_loc);

	msg_printf(DBG,"\t\tfrom %x to %x\n", lomem, himem);
	/* from lomem to himem, write 0 */
	msg_printf(INFO, "Write 0's\n");
	for (ptr = lomem; ptr < himem; ptr++) {
		*ptr = 0;
		/*wcheck_mem(ptr);*/
	}
	msg_printf(DBG,"\t\tlast ptr = %x\n",ptr);
	msg_printf(DBG,"\tfrom %x to %x\n", lomem, himem);

	/* from lomem to himem read and verify 0's, then write 1's */
	msg_printf(INFO, "Verify 0's and then write 1's\n");
	for (ptr = lomem ; ptr < himem; ptr++){
		if ((data = *ptr & mask) != 0) {
			fail++;
			memerr((char *)ptr, 0, data, sizeof(__psunsigned_t));
		}
		*ptr = -1;
		/*wcheck_mem(ptr);*/
		/*hiptr = ptr;*/
	}
	msg_printf(DBG,"\t\tlast ptr = %x\n",ptr);
	hiptr = --ptr;

	msg_printf(DBG,"\tfrom %x to %x\n", hiptr, lomem);
	/* from himem to lomem read and verify 1's, then write 0 */
	/* himem--; */
	msg_printf(INFO, "Verify 1's and then write 0\n");
	for (ptr = hiptr ; ptr >= lomem; ptr--) {
		if ((data = *ptr & mask) != mask) {
			fail++;
			memerr((char *)ptr,mask,data,sizeof(__psunsigned_t));
		}
		*ptr = 0x0;
		/*wcheck_mem(ptr);*/
	}

	msg_printf(DBG,"\t\tlast ptr = %x\n",ptr);
	msg_printf(DBG,"\tfrom %x to %x\n", lomem,himem);

	/* from lomem to himem, verify 0 */
	/* himem = memptr->himem; */
	msg_printf(INFO, "Verify 0's\n");
	for (ptr = lomem ; ptr < himem; ptr++) {
		if ((data = *ptr & mask) != 0x0) {
			fail++;
			memerr((char *)ptr, 0, data, sizeof(__psunsigned_t));
		}
	}
	msg_printf(DBG,"fail %x\n", fail);
	msg_printf(DBG,"ptr %x\n", ptr);

	return (fail);
} /* marchX */


/* MarchY
 * Described in van de Goor's book, "Testing Semiconductor Memories" and 
 *
 * (w0), u(r0,w1,r1), d(r1,w0,r0), (r0)
 *
 * Will detect address decoder faults, stuck-at-faults, transition faults,
 * coupling faults, and linked transition faults
 */

int
marchY(__psunsigned_t first_loc, __psunsigned_t last_loc)
{
	register __psunsigned_t *lomem = (__psunsigned_t *)first_loc;
	register __psunsigned_t *himem = (__psunsigned_t *)last_loc;
	register __psunsigned_t mask = FILL_F;
	register __psunsigned_t data, *ptr;
	register int fail = 0;
	__psunsigned_t *hiptr;

	msg_printf(INFO,"\nMarchY test from 0x%x to 0x%x\n",
	    first_loc, last_loc);

	msg_printf(DBG,"\tfrom %x to %x\n",lomem, himem);
	/* from lomem to himem, write 0 */
	msg_printf(INFO,"Write 0's\n");
	for (ptr = lomem; ptr < himem; ptr++) {
		*ptr = 0;
		/*wcheck_mem(ptr);*/
	}
	/* from lomem to himem read and verify 0's, write 1's, read 1's */
	msg_printf(INFO,"Verify 0's, write 1's, then verify 1's\n");
	msg_printf(DBG,"\t\tfrom %x to %x\n",lomem,himem);
	for (ptr = lomem ; ptr < himem; ptr++ ) {
		if ( (*ptr & mask) != 0) {
			data = *ptr & mask;
			fail++;
			memerr((char *)ptr, 0, data, sizeof(__psunsigned_t));

		}
		*ptr = -1;
		/*wcheck_mem(ptr);*/
		if ((*ptr & mask) != mask) {
			data = *ptr & mask;
			fail++;
			memerr((char *)ptr, 0, data, sizeof(__psunsigned_t));
		}
	}
	msg_printf(DBG,"\t\tlast ptr = %x\n", ptr);
	hiptr = --ptr;
	msg_printf(DBG,"\tfrom %x to %x\n", hiptr,lomem);

	/* from himem to lomem read and verify 1's, write 0's, read 0's */
	/* himem--; */
	msg_printf(INFO,"Verify 1's and then write 0's\n");
	for (ptr = hiptr ; ptr >= lomem; ptr--) {
		if (( *ptr & mask) != mask) {
			data = *ptr & mask;
			fail++;
			memerr((char *)ptr, 0, data, sizeof(__psunsigned_t));
		}
		*ptr = 0;
		/*wcheck_mem(ptr);*/
		if ((*ptr & mask) != 0) {
			data = *ptr & mask;
			fail++;
			memerr((char *)ptr, 0, data, sizeof(__psunsigned_t));
		}
	}

	msg_printf(DBG,"\t\tlast ptr = %x\n",ptr);
	msg_printf(DBG,"\tfrom %x to %x\n",lomem,himem);

	/* himem = memptr->himem; */
	msg_printf(INFO,"Verify 0's\n");
	for (ptr = lomem ; ptr < himem; ptr++) {
		if (( *ptr & mask) != 0) {
			data = *ptr & mask;
			fail++;
			memerr((char *)ptr, 0, data, sizeof(__psunsigned_t));

		}
	}
	msg_printf(DBG,"fail %x\n", fail);
	return (fail);
} /* marchY */


