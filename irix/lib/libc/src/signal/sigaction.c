 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/
#ident "$Revision: 1.7 $"
#ifdef __STDC__
	#pragma weak sigaction = _sigaction
#endif
#include "synonyms.h"
#include <sys/signal.h>
#include "sig_extern.h"
#include "mplib.h"

/* Wrappers for the POSIX system call sigaction. Needed to pass sigtramp */
int
sigaction(int sig, const struct sigaction *newaction,
	  struct sigaction *oldaction)
{
	MTLIB_RETURN( (MTCTL_SIGACTION, sig, newaction, oldaction) );
        return(_ksigaction(sig, newaction, oldaction, _sigtramp));
}
