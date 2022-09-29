 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

#ident "$Revision: 1.4 $"

#ifdef __STDC__
	#pragma weak sigaltstack = _sigaltstack
#endif

#include "synonyms.h"

#include <sys.s>
#include <sys/signal.h>
#include "../sys/sys_extern.h"

/* Wrappers for the SVR4 flavor of sigstack. */
int
sigaltstack(stack_t *nss, stack_t *oss)
{
	return((int)syscall(SYS_sigaltstack, nss, oss));	
}
