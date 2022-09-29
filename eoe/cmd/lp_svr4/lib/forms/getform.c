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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/forms/RCS/getform.c,v 1.1 1992/12/14 13:27:18 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "stdio.h"
#include "string.h"
#include "errno.h"
#include "sys/types.h"
#include "stdlib.h"

#include "lp.h"
#include "form.h"

/**
 ** getform() - EXTRACT FORM STRUCTURE FROM DISK FILE
 **/

int
#if	defined(__STDC__)
getform (
	char *			name,
	FORM *			formp,
	FALERT *		alertp,
	FILE **			align_fp
)
#else
getform (name, formp, alertp, align_fp)
	char *			name;
	FORM *			formp;
	FALERT *		alertp;
	FILE **			align_fp;
#endif
{
	static long		lastdir		= -1;

	FILE *			fp;

	register char *		path;


	if (!name || !*name) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Getting ``all''? If so, jump into the directory
	 * wherever we left off.
	 */
	if (STREQU(NAME_ALL, name)) {
		if (!(name = next_dir(Lp_A_Forms, &lastdir)))
			return (-1);
	} else
		lastdir = -1;


	/*
	 * Get the form configuration information (?)
	 */
	if (formp) {
		path = getformfile(name, DESCRIBE);
		if (!path)
			return (-1);
		if (!(fp = open_lpfile(path, "r", 0))) {
			Free (path);
			return (-1);
		}
		Free (path);

		if (rdform(name, formp, fp, 0, (int *)0) == -1) {
			close_lpfile (fp);
			return (-1);
		}
		close_lpfile (fp);
	}

	/*
	 * Get the alert information (?)
	 */
	if (alertp) {

		FALERT *		pa = getalert(Lp_A_Forms, name);


		/*
		 * Don't fail if we can't read it because of access
		 * permission UNLESS we're "root" or "lp"
		 */
		if (!pa) {

			if (errno == ENOENT) {
				alertp->shcmd = 0;
				alertp->Q = alertp->W = -1;

			} else if (errno == ENOTDIR) {
				freeform (formp);
				errno = ENOENT;	/* form doesn't exist */
				return (-1);

			} else if (
				errno != EACCES
			     || !getpid()		  /* we be root */
			     || STREQU(getname(), LPUSER) /* we be lp   */
			) {
				freeform (formp);
				return (-1);
			}

		} else
			*alertp = *pa;
	}

	/*
	 * Get the alignment pattern (?)
	 */
	if (align_fp) {
		path = getformfile(name, ALIGN_PTRN);
		if (!path) {
			freeform (formp);
			errno = ENOMEM;
			return (-1);
		}
		if (
			!(*align_fp = open_lpfile(path, "r", 0))
		     && errno != ENOENT
		) {
			Free (path);
			freeform (formp);
			return (-1);
		}
		Free (path);
	}

	return (0);
}
