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
	#pragma weak dmi = _dmi
#endif

#include "synonyms.h"
#include "mplib.h"


int     
dmi(int opcode, void *arg1, void *arg2, void *arg3,
      void *arg4, void *arg5, void *arg6, void *arg7)
{
	extern int __dmi(int, ...);
	MTLIB_BLOCK_NOCNCL_RET( int, __dmi(opcode, arg1, arg2, arg3,
					   arg4, arg5, arg6, arg7) );
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
