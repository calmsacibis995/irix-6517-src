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

#ident "$Revision: 1.7 $"

/*
 * Library routine supplying the entry point for a BSD 4.3-compatible
 * getgroups() routine.  This wrapper executes a syssgi sys call, (the
 * entry point for getgroups()), then unpacks the returned array of short
 * (gid_t) gids into the array of integers that the BSD getgroups() 
 * caller expects.
 */

#ifdef __STDC__
#ifdef LIBBSD
	#pragma weak getgroups = _lbsd_getgroups
#else
	#pragma weak BSDgetgroups = _BSDgetgroups
#endif
#endif
#include "synonyms.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/syssgi.h>

int
#ifdef LIBBSD
getgroups(gidsetlen,gidset)
#else
BSDgetgroups(gidsetlen,gidset)
#endif
	int gidsetlen;
	int *gidset;
{
	int num, loop;
	gid_t shortarray[NGROUPS];

	if ((num = (int)syssgi(SGI_GETGROUPS,gidsetlen,shortarray)) == -1)
		return(-1);	/* sys call set errno */
	if (gidsetlen == 0)	/* caller only wants # of groups, not list */
		return(num);
	/* sys call already checked for set sizes */
	for (loop = 0; loop < num; loop++)
		gidset[loop] = (int)shortarray[loop];
	return(num);

} /* end getgroups() */


