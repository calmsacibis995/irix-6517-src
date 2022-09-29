/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/getmntent.c	1.5"

/*LINTLIBRARY*/
#ifdef __STDC__
	#pragma weak getmntany = _getmntany
	#pragma weak getmntent = _getmntent
#endif
#include	"synonyms.h"
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/mnttab.h>
#include	<string.h>

#define	DIFF(xx)\
	(mrefp->xx != NULL && (mgetp->xx == NULL ||\
	 strcmp(mrefp->xx, mgetp->xx) != 0))
#define	SDIFF(xx, typem, typer)\
	(mgetp->xx == NULL || stat64(mgetp->xx, &statb) == -1 ||\
	(statb.st_mode & S_IFMT) != typem ||\
	 statb.st_rdev != typer)

static char	line[MNT_LINE_MAX];
static const char	sepstr[] = " \t\n";
static const char	dash[] = "-";

static int	getline();

int
getmntany(fd, mgetp, mrefp)
	register FILE	*fd;
	register struct	mnttab	*mgetp;
	register struct mnttab	*mrefp;
{
	register int	ret, bstat;
	register mode_t	bmode;
	register dev_t	brdev;
	struct stat64	statb;

	if (mrefp->mnt_special && stat64(mrefp->mnt_special, &statb) == 0 &&
	  ((bmode = (statb.st_mode & S_IFMT)) == S_IFBLK || bmode == S_IFCHR)) {
		bstat = 1;
		brdev = statb.st_rdev;
	} else
		bstat = 0;

	while ((ret = getmntent(fd, mgetp)) == 0 &&
	      ((bstat == 0 && DIFF(mnt_special)) ||
	       (bstat == 1 && SDIFF(mnt_special, bmode, brdev)) ||
	       DIFF(mnt_mountp) ||
	       DIFF(mnt_fstype) ||
	       DIFF(mnt_mntopts) ||
	       DIFF(mnt_time)))
		;
	return	ret;
}

int
getmntent(fd, mp)
	register FILE	*fd;
	register struct mnttab	*mp;
{
	register int	ret;
	char *lasttok = NULL;

#define	GETTOK(xx, ll)\
	if ((mp->xx = strtok_r(ll, sepstr, &lasttok)) == NULL)\
		return	MNT_TOOFEW;\
	if (strcmp(mp->xx, dash) == 0)\
		mp->xx = NULL

	/* skip leading spaces and comments */
	if ((ret = getline(line, fd)) != 0)
		return	ret;

	/* split up each field */
	GETTOK(mnt_special, line);
	GETTOK(mnt_mountp, NULL);
	GETTOK(mnt_fstype, NULL);
	GETTOK(mnt_mntopts, NULL);
	GETTOK(mnt_time, NULL);

	/* check for too many fields */
	if (strtok_r(NULL, sepstr, &lasttok) != NULL)
		return	MNT_TOOMANY;
#undef	GETTOK

	return	0;
}

static int
getline(lp, fd)
	register char	*lp;
	register FILE	*fd;
{
	register char	*cp;

	while ((lp = fgets(lp, MNT_LINE_MAX, fd)) != NULL) {
		if (strlen(lp) == MNT_LINE_MAX-1 && lp[MNT_LINE_MAX-2] != '\n')
			return	MNT_TOOLONG;

		for (cp = lp; *cp == ' ' || *cp == '\t'; cp++)
			;

		if (*cp != '#' && *cp != '\n')
			return	0;
	}
	return	-1;
}
