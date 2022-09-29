/*
 * patterns.c - tests the memory with mixed patterns
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
 * memory patterns test:
 *	1. Check data bus with a few locations
 *	2. Check address bus 
 *	3. Filled the whole memory with mixed patterns first, then read back
 *	and verify
 *	
 *	NOTE: new for IP30
 */
#ident "ide/godzilla/mem/patterns.c $Revision: 1.1 $"

#include <sys/types.h>
#include <libsc.h>
#include "d_mem.h"
#include "d_godzilla.h"

/*
 * Name:	Patterns	
 * Description:	runs a fast memory mixed patterns test
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
Patterns_test_l(__psunsigned_t begadr, __psunsigned_t endadr,
	void (*errfun)(char *, __psunsigned_t, __psunsigned_t, int))
{
	register volatile __psunsigned_t *ptr;
	__uint64_t data_pattern[NO_PATTERNS*2] =\
		{0x5a5a5a5a5a5a5a5a, 0x3c3c3c3c3c3c3c3c, 
		 0xf0f0f0f0f0f0f0f0, 0xa5a5a5a5a5a5a5a5, 
		 0xc3c3c3c3c3c3c3c3, 0x0f0f0f0f0f0f0f0f,
		 0x5a5a5a5a5a5a5a5a, 0x3c3c3c3c3c3c3c3c, 
		 0xf0f0f0f0f0f0f0f0, 0xa5a5a5a5a5a5a5a5, 
		 0xc3c3c3c3c3c3c3c3, 0x0f0f0f0f0f0f0f0f,
		 };

	register volatile __uint64_t	pattern;
	register volatile __uint64_t	big_ptr_incr=EVERY_1M;
	register volatile __uint64_t	small_ptr_incr=EVERY_4K; 
	register volatile uchar_t	index;
	register volatile __uint64_t	i,imax;

	msg_printf(INFO,"Patterns_l( %x, %x)\n", begadr, endadr);
	d_errors = 0;

	/* data bus test: at a couple of addresses, write a set of data */
 	/*              values and read them back			*/
	for (index=0; index < NO_PATTERNS; index ++) {
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
			msg_printf(ERR,"Patterns - data bus: failed in data short\n");
			d_errors ++;
			if (errfun)
				(*errfun)((char *)ptr, ~data_pattern [ index ],
					pattern, sizeof(ptr));
			goto _error;
		   }
		   msg_printf(DBG,"data bus test: index= %d, pattern= %x, addr= %x\n",index,data_pattern [ index ],(__psunsigned_t)ptr);
		   ptr += big_ptr_incr+1; /* slightly off */
		}
	}

	
	/* address bus test: addruniq test on a few address locations, */
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
			msg_printf(ERR,"Patterns - addr bus: failed in address short\n");
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
			msg_printf(ERR,"Patterns - addr bus: failed in address short\n");
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
			msg_printf(ERR,"MEMTEST: failed in sparse aduniq\n");
			d_errors ++;
			if (errfun)
				(*errfun)((char *)ptr, ~(__psunsigned_t)ptr,
					pattern, sizeof(ptr));
			goto _error;
		}
		msg_printf(DBG,"read after write: *ptr = %x, (__psunsigned_t)ptr = %x\n",*ptr,(__psunsigned_t)ptr);
	   }
	}
	else { 	msg_printf(ERR,"Patterns_l - addr bus: begadr >= endadr \n");
		d_errors++;
		goto _error;
	}


	/* data patterns test: Filled the whole memory with mixed */
 	/*              values and read them back			*/
	for (index=0; index < NO_PATTERNS; index ++) {
		ptr = (__psunsigned_t *)begadr;
		while (ptr < (__psunsigned_t *)((endadr+1) - NO_PATTERNS*sizeof(__psunsigned_t) )) { 
		   *ptr = data_pattern [ index ];
		   *(ptr+1) = data_pattern [ index + 1];
		   *(ptr+2) = data_pattern [ index + 2];
		   *(ptr+3) = data_pattern [ index + 3];
		   *(ptr+4) = data_pattern [ index + 4];
		   *(ptr+5) = data_pattern [ index + 5];
		   ptr += NO_PATTERNS; /* increment pointer */
		}
		/* Read back and verify */
		ptr = (__psunsigned_t *)begadr;
		while (ptr < (__psunsigned_t *)((endadr+1) - NO_PATTERNS*sizeof(__psunsigned_t) )) { 
		   pattern = *ptr ;
		   if (pattern != data_pattern [ index ]) {
			d_errors ++;
			if (errfun)
				(*errfun)((char *)ptr, data_pattern [ index ],
					pattern, sizeof(ptr));
			goto _error;
		   }
		   pattern = *(ptr+1);
		   if (pattern != data_pattern [ index +1]) {
			d_errors ++;
			if (errfun)
				(*errfun)((char *)(ptr+1), data_pattern [ index +1],
					pattern, sizeof(ptr));
			goto _error;
		   }
		   pattern = *(ptr+2);
		   if (pattern != data_pattern [ index +2]) {
			d_errors ++;
			if (errfun)
				(*errfun)((char *)(ptr+2), data_pattern [ index +2],
					pattern, sizeof(ptr));
			goto _error;
		   }
		   pattern = *(ptr+3);
		   if (pattern != data_pattern [ index +3]) {
			d_errors ++;
			if (errfun)
				(*errfun)((char *)(ptr+3), data_pattern [ index +3],
					pattern, sizeof(ptr));
			goto _error;
		   }
		   pattern = *(ptr+4);
		   if (pattern != data_pattern [ index +4]) {
			d_errors ++;
			if (errfun)
				(*errfun)((char *)(ptr+4), data_pattern [ index +4],
					pattern, sizeof(ptr));
			goto _error;
		   }
		   pattern = *(ptr+5);
		   if (pattern != data_pattern [ index +5]) {
			d_errors ++;
			if (errfun)
				(*errfun)((char *)(ptr+5), data_pattern [ index +5],
					pattern, sizeof(ptr));
			goto _error;
		   }

		   msg_printf(DBG,"Patterns test: index= %d, pattern= %x, addr= %x\n",index,data_pattern [ index ],(__psunsigned_t)ptr);
		   ptr += NO_PATTERNS; /* increment pointer */
		}
	}
	
	return 0;

_error:
	msg_printf(ERR,"Patterns_l failed \n");
	return 1;
}
