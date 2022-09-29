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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/lp/RCS/dirs.c,v 1.1 1992/12/14 13:28:13 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "sys/types.h"
#include "errno.h"

#include "lp.h"

/**
 ** mkdir_lpdir()
 **/

int
#if	defined(__STDC__)
mkdir_lpdir (
	char *			path,
	int			mode
)
#else
mkdir_lpdir (path, mode)
	char			*path;
	int			mode;
#endif
{
	int			old_umask = umask(0);
	int			ret;
	int			save_errno;


	ret = Mkdir(path, mode);
	if (ret != -1)
		ret = chown_lppath(path);
	save_errno = errno;
	if (old_umask)
		umask (old_umask);
	errno = save_errno;
	return (ret);
}
