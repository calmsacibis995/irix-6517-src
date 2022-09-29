/*
 * fasttest.c - tests the memory for shorts and stuck-at's
 *		no pattern dependency test
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
 * Fast memory test:
 *	There is a need to test only a few locations, as IP30 memory can go
 *	up to 2GB, and the IP30 emulation is much slower (/100?) than the 
 * 	real machine
 * sequence:
 * 	data short test: at a couple of addresses, write a set of data
 *		values and read them back
 *	address short test: addruniq test on a few address locations, repeat
 *		with data complement
 *	sparse addruniq test: addruniq on sparse locations throughout 
 *		the memory range
 *	
 *	NOTE: new for IP30
 *		-debugged on sable 9-29-95
 */
#ident "ide/godzilla/mem/fasttest.c $Revision: 1.4 $"

#include <sys/types.h>
#include <libsc.h>
#include "d_mem.h"
#include "d_godzilla.h"

/*
 * Name:	Fastmemtest_l
 * Description:	runs a fast memory connectivity test
 * Input:	begin and end addresses
 * Output:	Returns 0 if no error, 1, if error
 *		the diagnostic error counter, d_errors, is first initialized and
 *		 then set to the # of errors
 * Error Handling:
 * Side Effects:
 * Remarks:
 * Debug Status: compiles, not simulated, not emulated, not debugged
 */
bool_t
Fastmemtest_l(__psunsigned_t begadr, __psunsigned_t endadr,
	void (*errfun)(char *, __psunsigned_t, __psunsigned_t, int))
{
	register volatile __psunsigned_t *ptr;
	__uint64_t data_pattern[FASTMEMTEST_PATTERN_MAX] =\
		{0xffffffff00000000, 0xffff0000ffff0000, \
		 0xff00ff00ff00ff00, 0xf0f0f0f0f0f0f0f0, \
		 0xcccccccccccccccc, 0xaaaaaaaaaaaaaaaa};
	register volatile __uint64_t	pattern;
	register volatile __uint64_t	big_ptr_incr=EVERY_1M;
	register volatile __uint64_t	small_ptr_incr=EVERY_4K; 
	register volatile uchar_t	index;
	register volatile __uint64_t	i,imax;

	msg_printf(VRB,"Fastmemtest_l( %x, %x)\n", begadr, endadr);

	d_errors = 0;

#if SKIP
#endif /* SKIP */
	/* data short test: at a couple of addresses, write a set of data */
 	/*              values and read them back			*/
	for (index=0; index < FASTMEMTEST_PATTERN_MAX; index ++) {
		ptr = (__psunsigned_t *)begadr;
		while (ptr < (__psunsigned_t *)endadr) {
		   *ptr = data_pattern [ index ];
		   pattern = *ptr ;
		   if (pattern != data_pattern [ index ]) {
			d_errors ++;
			if (errfun)
				(*errfun)((char *)ptr, data_pattern [ index ],
					pattern, sizeof(ptr));
			goto _error;
		   }
		   *ptr = ~data_pattern [ index ]; /* complement */
		   pattern = *ptr ;
		   if (pattern != ~data_pattern [ index ]) {
			msg_printf(ERR,"Fasttest: failed in data short\n");
			d_errors ++;
			if (errfun)
				(*errfun)((char *)ptr, ~data_pattern [ index ],
					pattern, sizeof(ptr));
			goto _error;
		   }
		   msg_printf(DBG,"data short test: index= %d, pattern= %x, addr= %x\n",index,data_pattern [ index ],(__psunsigned_t)ptr);
		   ptr += big_ptr_incr+1; /* slightly off */
		}
	}
	
	/* address short test: addruniq test on a few address locations, */
	/*			repeat with data complement		*/
	/* the addresses are "walking one" addresses */
#if SKIP /* SKIP */
#endif /* SKIP */
	msg_printf(DBG,"address short test: begin.\n");
	/* *work_ptr_ptr = work_ptr + 20000000; EXCEPTION!*/
	msg_printf(DBG," begadr = %x, endadr = %x\n",(__psunsigned_t *)begadr,(__psunsigned_t *)endadr);
	imax =1 ;
	for (ptr = (__psunsigned_t *)begadr+1; 
	     ptr <= (__psunsigned_t *)endadr; ) {
		*ptr = (__psunsigned_t)ptr;
		msg_printf(DBG,"in loop, after: *ptr =%x\n",*ptr);
	   	ptr += imax;
		imax *=2;
		/* msg_printf(DBG,"imax =%d\n",imax); */
	}
	/* read after writing all */
	msg_printf(DBG,"address short test: read after writing all\n");
	imax =1 ;
        for (ptr = (__psunsigned_t *)begadr+1;
             ptr <= (__psunsigned_t *)endadr; ) {
		pattern = *ptr;
		if (pattern != (__psunsigned_t)ptr) {
			msg_printf(ERR,"Fasttest: failed in address short\n");
			d_errors ++;
			if (errfun)
				(*errfun)((char *)ptr, (__psunsigned_t)ptr,
					pattern, sizeof(ptr));
			goto _error;
		}
		msg_printf(DBG,"read after write: *ptr = %x, (__psunsigned_t)ptr = %x\n",*ptr,(__psunsigned_t)ptr);
	   	ptr += imax;
		imax *=2;
	}
	/* repeat with data complement */
	msg_printf(DBG,"address short test: repeat with data complement\n");
	imax =1 ;
        for (ptr = (__psunsigned_t *)begadr+1;
             ptr <= (__psunsigned_t *)endadr; ) {
		*ptr = ~(__psunsigned_t)ptr; /* complement */
		ptr += imax;
		imax *=2;
	}
	/* read after writing all (data complement) */
	imax =1 ;
        for (ptr = (__psunsigned_t *)begadr+1;
             ptr <= (__psunsigned_t *)endadr; ) {
		pattern = *ptr;
		if (pattern != ~(__psunsigned_t)ptr) {
			msg_printf(ERR,"Fasttest: failed in address short\n");
			d_errors ++;
			if (errfun)
				(*errfun)((char *)ptr, ~(__psunsigned_t)ptr,
					pattern, sizeof(ptr));
			goto _error;
		}
		msg_printf(DBG,"read after write: *ptr = %x, (__psunsigned_t)ptr = %x\n",*ptr,(__psunsigned_t)ptr);
	   	ptr += imax;
		imax *=2;
	}

#if SKIP 
#endif /* SKIP */
	/* sparse addruniq test: addruniq on sparse locations throughout */
 	/*              the memory range				*/
	/* NOTE: 100*small_ptr_incr XXX */
	msg_printf(DBG,"sparse addruniq test: begin\n");
	if ((__psunsigned_t *)begadr < (__psunsigned_t *)endadr) {
	   for (ptr = (__psunsigned_t *)begadr; 
		ptr < (__psunsigned_t *)endadr; ptr += 100*small_ptr_incr+1) {
		*ptr = ~(__psunsigned_t)ptr; /* use complement for this test*/
		}
	   /* read after writing all */
	   for (ptr = (__psunsigned_t *)begadr; 
		ptr < (__psunsigned_t *)endadr; ptr += 100*small_ptr_incr+1) {
		pattern = *ptr ;
		if (pattern != ~(__psunsigned_t)ptr) {
			msg_printf(ERR,"Fasttest: failed in sparse aduniq\n");
			d_errors ++;
			if (errfun)
				(*errfun)((char *)ptr, ~(__psunsigned_t)ptr,
					pattern, sizeof(ptr));
			goto _error;
		}
		msg_printf(DBG,"read after write: *ptr = %x, (__psunsigned_t)ptr = %x\n",*ptr,(__psunsigned_t)ptr);
	   }
	}
	else { 	msg_printf(ERR,"Fastmemtest_l: begadr >= endadr \n");
		d_errors++;
		goto _error;
	}
	return 0;

_error:
	msg_printf(ERR,"Fastmemtest_l failed \n");
	return 1;
}
