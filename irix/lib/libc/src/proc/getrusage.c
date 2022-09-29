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
#ident "$Revision: 1.4 $"
/*
 * getrusage --
 * We now have most of the information that is returned by BSD's
 * getrusage() syscall. All rusage statistics that are not maintained
 * are set to zero. See the man page for details.
 */
#ifdef __STDC__
	#pragma weak getrusage = _getrusage
#endif
#include "synonyms.h"
#include <sys/syssgi.h>
#include <sys/resource.h>

int
getrusage(int who, register struct rusage *ru)
{
	return((int)syssgi(SGI_RUSAGE, who, ru));
}
