
 /*************************************************************************
 #									  *
 # 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 #									  *
 #  These coded instructions, statements, and computer programs  contain  *
 #  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 #  are protected by Federal copyright law.  They  may  not be disclosed  *
 #  to  third  parties  or copied or duplicated in any form, in whole or  *
 #  in part, without the prior written consent of Silicon Graphics, Inc.  *
 #									  *
 #************************************************************************/

#ident "$Revision: 1.1 $"

#ifdef __STDC__
	#pragma weak newarraysess = _newarraysess
#endif

#include "synonyms.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/syssgi.h>

int
newarraysess(void)
{
	return((int) syssgi(SGI_NEWARRAYSESS, -1));
}
