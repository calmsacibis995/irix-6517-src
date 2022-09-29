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
	#pragma weak putpmsg = _putpmsg
#endif

#include "synonyms.h"
#include "mplib.h"
#include <stropts.h>


int
putpmsg(int fd, const struct strbuf *ctlptr, const struct strbuf *dataptr,
	int band, int flags)
{
	extern int __putpmsg(int, const struct strbuf *, const struct strbuf *,
			    int, int);
	MTLIB_BLOCK_CNCL_RET( int, __putpmsg(fd, ctlptr, dataptr, band, flags) );
}

#endif /* !_LIBC_ABI && !_LIBC_NOMP */
