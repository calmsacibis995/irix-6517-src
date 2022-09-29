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

#ident "$Revision: 1.13 $"

/*
 * Library routine supplying the entry point for a BSD 4.3-compatible
 * setgroups() routine.  This wrapper packs the incoming array of 
 * integer gids into a short (gid_t) array and calls syssgi (the entry
 * point for the setgroups() system call).
 */

#ifdef __STDC__
#ifdef LIBBSD
	#pragma weak setgroups = _lbsd_setgroups
#else
	#pragma weak BSDsetgroups = _BSDsetgroups
#endif
#endif
#include "synonyms.h"

#define _BSD_COMPAT
#undef signal
#undef sigpause
#include <sys/param.h>
#include <sys/syssgi.h>
#include <unistd.h>
#include <errno.h>

int
#ifdef LIBBSD
setgroups(ngroups,gidset)
#else
BSDsetgroups(ngroups,gidset)
#endif
	int ngroups;
	int *gidset;
{
	int loop;
	gid_t shortarray[NGROUPS_UMAX];

	if (ngroups > NGROUPS_UMAX) {	/* never more than this */
		setoserror(EINVAL);
		return(-1);
	}
	for (loop = 0; loop < ngroups; loop++)
		shortarray[loop] = (gid_t)gidset[loop];
	return((int)syssgi(SGI_SETGROUPS,ngroups,shortarray));

} /* end setgroups() */


