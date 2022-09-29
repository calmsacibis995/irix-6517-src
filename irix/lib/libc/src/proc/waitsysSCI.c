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

/* Wrapper for blocking system call - see include files for details */

#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
#ifdef __STDC__
	#pragma weak waitsys = _waitsys
#endif

#include "synonyms.h"
#include "mplib.h"
#include <wait.h>
#include <unistd.h>


int
waitsys(idtype_t idtype, id_t id, siginfo_t *info, int options,
	struct rusage *ru)
{
	extern int __waitsys(idtype_t, id_t, siginfo_t *, int, struct rusage *);
	MTLIB_BLOCK_CNCL_RET( int, __waitsys(idtype, id, info, options, ru) );
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
