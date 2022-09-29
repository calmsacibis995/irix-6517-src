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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/secure/RCS/secure.c,v 1.1 1992/12/14 13:34:11 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "string.h"
#include "sys/param.h"
#include "stdlib.h"

#include "lp.h"
#include "secure.h"

/**
 ** getsecure() - EXTRACT SECURE REQUEST STRUCTURE FROM DISK FILE
 **/

SECURE *
#if	defined(__STDC__)
getsecure (
	char *			file
)
#else
getsecure (file)
	char			*file;
#endif
{
	static SECURE		secbuf;

	char			buf[BUFSIZ],
				*path;

	FILE			*fp;

	int			fld;


	if (*file == '/')
		path = Strdup(file);
	else
		path = makepath(Lp_Requests, file, (char *)0);
	if (!path)
		return (0);

	if (!(fp = open_lpfile(path, "r", MODE_NOREAD))) {
		Free (path);
		return (0);
	}
	Free (path);

	secbuf.user = 0;
	for (
		fld = 0;
		fld < SC_MAX && fgets(buf, BUFSIZ, fp);
		fld++
	) {
		buf[strlen(buf) - 1] = 0;
		switch (fld) {

		case SC_REQID:
			secbuf.req_id = Strdup(buf);
			break;

		case SC_UID:
			secbuf.uid = (uid_t)atol(buf);
			break;

		case SC_USER:
			secbuf.user = Strdup(buf);
			break;

		case SC_GID:
			secbuf.gid = (gid_t)atol(buf);
			break;

		case SC_SIZE:
			secbuf.size = (size_t)atol(buf);
			break;

		case SC_DATE:
			secbuf.date = (time_t)atol(buf);
			break;

		case SC_SYSTEM:
			secbuf.system = Strdup(buf);
			break;
		}
	}
	if (ferror(fp) || fld != SC_MAX) {
		int			save_errno = errno;

		freesecure (&secbuf);
		close_lpfile (fp);
		errno = save_errno;
		return (0);
	}
	close_lpfile (fp);

	/*
	 * Now go through the structure and see if we have
	 * anything strange.
	 */
	if (
	        secbuf.uid > MAXUID+1
	     || !secbuf.user
	     || secbuf.gid > MAXUID+1
	     || secbuf.size == 0
	     || secbuf.date <= 0
	) {
		freesecure (&secbuf);
		errno = EBADF;
		return (0);
	}

	return (&secbuf);
}

/**
 ** putsecure() - WRITE SECURE REQUEST STRUCTURE TO DISK FILE
 **/

int
#if	defined(__STDC__)
putsecure (
	char *			file,
	SECURE *		secbufp
)
#else
putsecure (file, secbufp)
	char			*file;
	SECURE			*secbufp;
#endif
{
	char			*path;

	FILE			*fp;

	int			fld;

	if (*file == '/')
		path = Strdup(file);
	else
		path = makepath(Lp_Requests, file, (char *)0);
	if (!path)
		return (-1);

	if (!(fp = open_lpfile(path, "w", MODE_NOREAD))) {
		Free (path);
		return (-1);
	}
	Free (path);

	if (
		!secbufp->req_id ||
		!secbufp->user
	)
		return (-1);

	for (fld = 0; fld < SC_MAX; fld++)

		switch (fld) {

		case SC_REQID:
			(void)fprintf (fp, "%s\n", secbufp->req_id);
			break;

		case SC_UID:
			(void)fprintf (fp, "%ld\n", secbufp->uid);
			break;

		case SC_USER:
			(void)fprintf (fp, "%s\n", secbufp->user);
			break;

		case SC_GID:
			(void)fprintf (fp, "%ld\n", secbufp->gid);
			break;

		case SC_SIZE:
			(void)fprintf (fp, "%lu\n", secbufp->size);
			break;

		case SC_DATE:
			(void)fprintf (fp, "%ld\n", secbufp->date);
			break;

		case SC_SYSTEM:
			(void)fprintf (fp, "%s\n", secbufp->system);
			break;
		}


	close_lpfile (fp);

	return (0);
}

/**
 ** freesecure() - FREE A SECURE STRUCTURE
 **/

void
#if	defined(__STDC__)
freesecure (
	SECURE *		secbufp
)
#else
freesecure (secbufp)
	SECURE *		secbufp;
#endif
{
	if (!secbufp)
		return;
	if (secbufp->req_id)
		Free (secbufp->req_id);
	if (secbufp->user)
		Free (secbufp->user);
	if (secbufp->system)
		Free (secbufp->system);
	return;
}
