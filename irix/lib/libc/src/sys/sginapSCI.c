
 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

/* Wrapper for modified system call - see include files for details */

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
#ifdef __STDC__
	#pragma weak sginap = _sginap
#endif

#include "synonyms.h"
#include "mplib.h"


long
sginap(long ticks)
{
	extern long __sginap(long);
	extern int sched_yield(void);

	if (ticks == 0 && MTLIB_ACTIVE()) {
		 return ((long)sched_yield());
	}
	MTLIB_BLOCK_CNCL_RET_1( long, __sginap(ticks) );
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
