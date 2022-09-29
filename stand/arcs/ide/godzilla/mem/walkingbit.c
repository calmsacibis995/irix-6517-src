/*
 * walkingbit.c
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
 *
 * walkingbit.c - IDE walking bit data path test
 *
 * The data bus is tested by doing a "walking 1" or "walking 0" test over
 * a given range of memory, loading and storing in byte, half-word, word
 * or double-word units as specified by ra->ra_size.
 *
 * NOTE: for IP30, only the CPUCTRL0 register was changed to HEART_MODE 
 * 	What's left to do:  XXX
 *		- delete non-IP30 code 
 */
#ident	"ide/godzilla/mem/walkingbit.c:  $Revision: 1.7 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include "setjmp.h"
#include <libsk.h>
#include "d_mem.h"
#include "d_godzilla.h"	

#undef BUSY_COUNT

#if EMULATION || SABLE
#define BUSY_COUNT      0x80
#else
#define BUSY_COUNT      0x4000
#endif

/* forward declarations */
bool_t WALKING_ONE(volatile *ptr);	/* XXX */

#define	WALKING_ONE(TYPE)						\
{									\
	volatile TYPE *ptr;						\
	TYPE walk_mask;							\
	heartreg_t      sync_value;					\
									\
	ptr = (TYPE *)ra->ra_base;					\
	walk_mask = sense == BIT_INVERT ? ~0x0 : 0x0;			\
	while (--count >= 0) {						\
		register TYPE	walking_1s = 0x1;			\
									\
		do {							\
			register TYPE	value;				\
									\
			value = walking_1s ^ walk_mask;			\
			*ptr = value;					\
                        /* reading the cpuctrl0 register flushes */	\
 			/*  the write buffer, to be sure that the */	\
			/*  data was written to memory */		\
			/* was:		*cpuctrl0;	*/		\
			/* for IP30, read the SYNC register: when */	\
			/* the read completes, the flushing is done */	\
			PIO_REG_RD_64(HEART_SYNC, ~0x0, sync_value);	\
			if (*ptr != value) {				\
				passflag = FALSE;			\
				if (errfun) 				\
					(*errfun)((char *)ptr,		\
						value,			\
						*ptr, sizeof(TYPE));	\
				if (till == RUN_UNTIL_ERROR)		\
					return passflag;		\
			}						\
			walking_1s <<= 1;				\
		} while (walking_1s);					\
									\
		ptr++;							\
		if (!busy_count--) {					\
			busy_count = BUSY_COUNT;			\
			busy(1);					\
		}							\
	}								\
	msg_printf(DBG," sync value was %x (must be 0)\n",sync_value);	\
	break;								\
}

bool_t
memwalkingbit(struct range *ra, enum bitsense sense, enum runflag till,
	void (*errfun)(char *, __psunsigned_t, __psunsigned_t, int))
{
	register int	busy_count = 0;
	register int	count = (int)ra->ra_count;
	jmp_buf		faultbuf;
	bool_t		passflag = TRUE;

	msg_printf(VRB, "CPU local memory walking bit test\n");

	/* to prevent write-back since ide is cached */
	flush_cache();

	if (setjmp(faultbuf)) {
		busy(0);
		show_fault();
		return FALSE;
	}
	nofault = faultbuf;

	busy(0);
	switch (ra->ra_size) {
#if _MIPS_SIM == _ABI64
	case sizeof(__psunsigned_t):
		WALKING_ONE(__psunsigned_t)

#endif
	case sizeof(u_int):
		WALKING_ONE(u_int)

	case sizeof(u_short):
		WALKING_ONE(u_short)

	case sizeof(u_char):
		WALKING_ONE(u_char)
	}

	busy(0);
	nofault = 0;
	return passflag;
}
