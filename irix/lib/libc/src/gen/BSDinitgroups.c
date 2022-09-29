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

#ident "$Revision: 1.9 $"

/*
 * initgroups.c: 4.3 BSD initgroups() routine.
 */

#ifdef __STDC__
	#pragma weak BSDinitgroups = _BSDinitgroups
#endif
#include "synonyms.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h> 		/* for prototyping */
#include <errno.h>
#include <grp.h>

int
#ifdef LIBBSD
initgroups(char *uname, int agroup)
#else
BSDinitgroups(char *uname, int agroup)
#endif
{
	int ngroups = 0;
	int groups[NGROUPS_UMAX];
	register struct group *grp;
	register int i, ngroups_max;

	ngroups_max = (int)sysconf(_SC_NGROUPS_MAX); /* fetch actual system max */
	if (ngroups_max < 0) {                  /* *SHOULDN'T* ever occur!! */
		_setoserror(EINVAL);
		return(-1);
	}

	if (agroup >= 0)
		groups[ngroups++] = agroup;
	setgrent();
	while (grp = getgrent()) {
		if (grp->gr_gid == (gid_t)agroup)
			continue;
		for (i = 0; grp->gr_mem[i]; i++)
			if (!strcmp(grp->gr_mem[i], uname)) {
				if (ngroups == ngroups_max)
					goto toomany;
				groups[ngroups++] = (int)grp->gr_gid;
			}
	}
toomany:
	endgrent();
	return(BSDsetgroups(ngroups, groups));
}
