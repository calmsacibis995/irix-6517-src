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
	#pragma weak setpgrp = _lbsd_setpgrp
	#pragma weak getpgrp = _lbsd_getpgrp
#endif
#include "synonyms.h"

/*
 * Wrappers for BSD version of get/set pgrp
 * These BSD version are in libc
 */
int
setpgrp(int pid, int pgrp)
{
	return(BSDsetpgrp(pid, pgrp));
}

int
getpgrp(int pid)
{
	return(BSDgetpgrp(pid));
}
