/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 252.227-7013.  |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Avenue                               |
 * |         Sunnyvale, California 94088-3650, USA             |
 * |-----------------------------------------------------------|
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/printers/RCS/printwheels.c,v 1.1 1992/12/14 13:33:38 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "string.h"
#include "errno.h"
#include "sys/types.h"
#include "stdlib.h"

#include "lp.h"
#include "printers.h"

/**
 ** getpwheel() - GET PRINT WHEEL INFO FROM DISK
 **/

PWHEEL *
#if	defined(__STDC__)
getpwheel (
	char *			name
)
#else
getpwheel (name)
	char			*name;
#endif
{
	static long		lastdir		= -1;

	static PWHEEL		pwheel;

	register FALERT		*pa;


	if (!name || !*name) {
		errno = EINVAL;
		return (0);
	}

	/*
	 * Getting ``all''? If so, jump into the directory
	 * wherever we left off.
	 */
	if (STREQU(NAME_ALL, name)) {
		if (!(name = next_dir(Lp_A_PrintWheels, &lastdir)))
			return (0);
	} else
		lastdir = -1;

	/*
	 * Get the information for the alert.
	 */
	if (!(pa = getalert(Lp_A_PrintWheels, name))) {

		/*
		 * Unless the world has turned weird, we shouldn't
		 * get ENOTDIR if we're doing the ``all'' case--because
		 * getting here in the all case meant the printwheel
		 * directory exists, but ENOTDIR means it doesn't!
		 */
		if (errno == ENOTDIR)
			errno = ENOENT; /* printwheel doesn't exist */

		return (0);
	}

	pwheel.alert = *pa;
	pwheel.name = Strdup(name);

	return (&pwheel);
}

/**
 ** putpwheel() - PUT PRINT WHEEL INFO TO DISK
 **/

int
#if	defined(__STDC__)
putpwheel (
	char *			name,
	PWHEEL *		pwheelp
)
#else
putpwheel (name, pwheelp)
	char			*name;
	PWHEEL			*pwheelp;
#endif
{
	register char		*path;

	struct stat		statbuf;


	if (!name || !*name) {
		errno = EINVAL;
		return (-1);
	}

	if (STREQU(name, NAME_ALL)) {
		errno = ENOENT;
		return (-1);
	}

	/*
	 * Create the parent directory for this printer
	 * if it doesn't yet exist.
	 */
	if (!(path = makepath(Lp_A_PrintWheels, name, (char *)0)))
		return (-1);
	if (Stat(path, &statbuf) == 0) {
		if (!(statbuf.st_mode & S_IFDIR)) {
			Free (path);
			errno = ENOTDIR;
			return (-1);
		}
	} else if (errno != ENOENT || mkdir_lpdir(path, MODE_DIR) == -1) {
		Free (path);
		return (-1);
	}
	Free (path);

	/*
	 * Now write out the alert condition.
	 */
	if (putalert(Lp_A_PrintWheels, name, &(pwheelp->alert)) == -1)
		return (-1);

	return (0);
}

/**
 ** delpwheel() - DELETE PRINT WHEEL INFO FROM DISK
 **/

#if	defined(__STDC__)
static int		_delpwheel ( char * );
#else
static int		_delpwheel();
#endif

int
#if	defined(__STDC__)
delpwheel (
	char *			name
)
#else
delpwheel (name)
	char			*name;
#endif
{
	long			lastdir;


	if (!name || !*name) {
		errno = EINVAL;
		return (-1);
	}

	if (STREQU(NAME_ALL, name)) {
		lastdir = -1;
		while ((name = next_dir(Lp_A_PrintWheels, &lastdir)))
			if (_delpwheel(name) == -1)
				return (-1);
		return (0);
	} else
		return (_delpwheel(name));
}

/**
 ** _delpwheel()
 **/

static int
#if	defined(__STDC__)
_delpwheel (
	char *			name
)
#else
_delpwheel (name)
	char			*name;
#endif
{
	register char		*path;

	if (delalert(Lp_A_PrintWheels, name) == -1)
		return (-1);
	if (!(path = makepath(Lp_A_PrintWheels, name, (char *)0)))
		return (-1);
	if (Rmdir(path)) {
		Free (path);
		return (-1);
	}
	Free (path);
	return (0);
}

/**
 **  freepwheel() - FREE MEMORY ALLOCATED FOR PRINT WHEEL STRUCTURE
 **/

void
#if	defined(__STDC__)
freepwheel (
	PWHEEL *		ppw
)
#else
freepwheel (ppw)
	PWHEEL			*ppw;
#endif
{
	if (!ppw)
		return;
	if (ppw->name)
		Free (ppw->name);
	if (ppw->alert.shcmd)
		Free (ppw->alert.shcmd);
	return;
}
