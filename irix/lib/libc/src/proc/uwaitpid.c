/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.3 $"
#ifdef __STDC__
	#pragma weak waitpid = _waitpid
#endif
#include "synonyms.h"
#include "mplib.h"
#include <wait.h>
#include <unistd.h>
#include "proc_extern.h"

pid_t
waitpid(pid_t pid, int *stat_loc, int options)
{
	idtype_t idtype;
	id_t id;
	siginfo_t info;
	int error;
	extern int __waitsys(idtype_t, id_t, siginfo_t *, int, struct rusage *);

	if (pid > 0) {
		idtype = P_PID;
		id = pid;
	} else if (pid < -1) {
		idtype = P_PGID;
		id = -pid;
	} else if (pid == -1) {
		idtype = P_ALL;
		id = 0;
	} else {
		idtype = P_PGID;
		id = getpgid(0);
	}

	options |= (WEXITED|WTRAPPED);

	MTLIB_BLOCK_CNCL_VAL( error,
		__waitsys(idtype, id, &info, options, 0) );
	if (error < 0)
		return error;

	if (stat_loc) 
		*stat_loc = _wstat(info.si_code, info.si_status & 0377);
	return info.si_pid;
}
