/*
 * initgroups.c: POSIX/BSD routine.
 *
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * Copyright 1989,1990, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.14 $"
/*
 * initgroups
 */
#ifdef __STDC__
	#pragma weak initgroups = _initgroups
#endif
#include "synonyms.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <unistd.h>
#include <grp.h>
#include <string.h>		/* for prototyping */

int
initgroups(const char *uname, gid_t agroup)
{
	int ngroups = 0;
	gid_t groups[NGROUPS_UMAX];
	register struct group *grp;
	register int i, ngroups_max;
	int	ngbymem;

	ngroups_max = (int)sysconf(_SC_NGROUPS_MAX);/* fetch actual system max */
	if (ngroups_max < 0) {			/* *SHOULDN'T* ever occur!! */
		_setoserror(EINVAL);
		return(-1);
	}

	if (agroup >= 0)
		groups[ngroups++] = (gid_t)agroup;

	if ((ngbymem = getgrmember(uname, groups, ngroups_max, ngroups)) == -1) {
		setgrent();
		while (grp = getgrent()) {
			if (grp->gr_gid == (gid_t)agroup)
				continue;
			for (i = 0; grp->gr_mem[i]; i++)
				if (!strcmp(grp->gr_mem[i], uname)) {
					if (ngroups == ngroups_max)
						goto toomany;
					groups[ngroups++] = (gid_t)grp->gr_gid;
				}
		}
toomany:
		endgrent();
	} else
		ngroups = ngbymem;
	return(setgroups(ngroups, groups));
}
