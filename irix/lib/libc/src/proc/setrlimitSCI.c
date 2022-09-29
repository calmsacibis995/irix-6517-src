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
	#pragma weak  setrlimit =	_setrlimit
#if _MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32
	#pragma weak  setrlimit64 =	_setrlimit
	#pragma weak  _setrlimit64 =	_setrlimit
#endif
#endif

#include "synonyms.h"
#include "mplib.h"
#undef _LFAPI
#include <sys/resource.h>


int
setrlimit(int resource, const struct rlimit *rlp)
{
	extern int __setrlimit(int, const struct rlimit *);

	if (resource == RLIMIT_PTHREAD && MTLIB_ACTIVE()) {
		if (MTLIB_VAL( (MTCTL_SETRLIMIT, resource, rlp), 0) ) {
			return (-1);
		}
	}
	return (__setrlimit(resource, rlp));
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
