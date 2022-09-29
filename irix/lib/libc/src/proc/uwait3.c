/**************************************************************************
 *									  *
 * 		 Copyright (C) 1991, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.4 $"
#ifdef __STDC__
	#pragma weak wait3 = _wait3
#endif
#include "synonyms.h"
#include "mplib.h"
#include <wait.h>
#include "proc_extern.h"

pid_t
wait3(int *stat_loc, int options, struct rusage *rup)
{
        register int error;
        siginfo_t info;
	extern int __waitsys(idtype_t, id_t, siginfo_t *, int, struct rusage *);

	options |= (WEXITED|WTRAPPED);

	MTLIB_BLOCK_CNCL_VAL( error,
		__waitsys(P_ALL, (id_t)0, &info, options, rup) );
	if (error < 0)
		return error;

	if (stat_loc) 
		*stat_loc = _wstat(info.si_code, info.si_status & 0377);
	return info.si_pid;
}

