/*
 * addruniq.c - IDE address uniqueness test
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
 * An address uniqueness test is done over a specified range of memory by
 * writing addresses to themselves and reading them back.
 */
/* NOTE: for IP30, no major change as no register is written to directly 
 * 	 the cache is flushed
 */
#ident	"ide/godzilla/mem/addruniq.c:  $Revision: 1.4 $"

#include <sys/types.h>
#include <fault.h>
#include "d_mem.h"
#include "setjmp.h"
#include "libsk.h"
#include "libsc.h"

bool_t
memaddruniq(struct range *ra, enum bitsense sense, enum runflag till,
	void (*errfun)(char *, __psunsigned_t, __psunsigned_t, int))
{
	register int		busy_count = 0;
	register long		count;
	jmp_buf			faultbuf;
	bool_t			passflag = TRUE;
	register volatile __psunsigned_t *ptr;

	msg_printf(VRB, "CPU local memory address uniqueness test\n");

	if (setjmp(faultbuf)) {
		busy(0);
		show_fault();
		return FALSE;
	}
	nofault = faultbuf;

	busy(0);
	count = ra->ra_count;
	/* scale count to double words */
	if (ra->ra_size != sizeof(__psunsigned_t))
		count = (ra->ra_count*ra->ra_size)/(int)sizeof(__psunsigned_t);
	/* use 2 loops such that we don't have to test sense within loop */
	if (sense == BIT_TRUE) {
		for (ptr = (__psunsigned_t *)ra->ra_base; --count >= 0; ptr++) {
			*ptr = (__psunsigned_t)ptr;
			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			}
		}
	} else {
		for (ptr = (__psunsigned_t *)ra->ra_base; --count >= 0; ptr++) {
			*ptr = ~(__psunsigned_t)ptr;
			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			}
		}
	}

	count = ra->ra_count;
	/* scale count to double words */
	if (ra->ra_size != sizeof(__psunsigned_t))
		count = (ra->ra_count*ra->ra_size)/(int)sizeof(__psunsigned_t);
	/* use 2 loops such that we don't have to test sense within loop */
	if (sense == BIT_TRUE) {
		for (ptr = (__psunsigned_t *)ra->ra_base; --count >= 0; ptr++) {
			if (*ptr != (__psunsigned_t)ptr) {
				passflag = FALSE;
				if (errfun)
					(*errfun)((char *)ptr,
						(__psunsigned_t)ptr,
						*ptr, sizeof(ptr));
				if (till == RUN_UNTIL_ERROR)
					break;
			}

			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			}
		}
	} else {
		for (ptr = (__psunsigned_t *)ra->ra_base; --count >= 0; ptr++) {
			if (*ptr != ~(__psunsigned_t)ptr) {
				passflag = FALSE;
				if (errfun)
					(*errfun)((char *)ptr,
						(~(__psunsigned_t)ptr),
						*ptr, sizeof(ptr));
				if (till == RUN_UNTIL_ERROR)
					break;
			}

			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			}
		}
	}

	busy(0);
	nofault = 0;
	return passflag;
}
