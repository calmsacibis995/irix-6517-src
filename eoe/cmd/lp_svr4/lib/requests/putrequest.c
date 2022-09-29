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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/requests/RCS/putrequest.c,v 1.1 1992/12/14 13:33:59 suresh Exp $"
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
#include "sys/utsname.h"
#include "stdlib.h"

#include "lp.h"
#include "requests.h"

extern struct {
	char			*v;
	short			len;
}			reqheadings[];

/**
 ** putrequest() - WRITE REQUEST STRUCTURE TO DISK FILE
 **/

int
#if	defined(__STDC__)
putrequest (
	char *			file,
	REQUEST *		reqbufp
)
#else
putrequest (file, reqbufp)
	char			*file;
	REQUEST			*reqbufp;
#endif
{
	char			**pp,
				*path;

	FILE			*fp;

	int			fld;

	/*
	 * First go through the structure and see if we have
	 * anything strange.
	 */
	if (
		reqbufp->copies <= 0
	     || !(reqbufp->destination)
	     || !reqbufp->file_list || !*(reqbufp->file_list)
	     || (reqbufp->actions & (ACT_MAIL|ACT_WRITE))
			&& (reqbufp->alert && *(reqbufp->alert))
	     || reqbufp->priority < -1 || 39 < reqbufp->priority
	) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Now open the file and write out the request.
	 */

	/*
	 * Full pathname? If so the file must lie in LP's
	 * regular temporary directory.
	 */
	if (*file == '/') {
		if (!STRNEQU(file, Lp_Tmp, strlen(Lp_Tmp))) {
			errno = EINVAL;
			return (-1);
		}
		path = Strdup(file);

	/*
	 * A relative pathname (such as system/name)?
	 * If so we'll locate it under LP's regular temporary
	 * directory.
	 */
	} else if (strchr(file, '/')) {
		if (!(path = makepath(Lp_Tmp, file, (char *)0)))
			return (-1);

	/*
	 * If must be a simple name. Locate this under the
	 * special temporary directory that is linked to the
	 * regular place for the local system.
	 */
	} else if (!(path = makepath(Lp_Temp, file, (char *)0)))
		return (-1);

	if (!(fp = open_lpfile(path, "w", MODE_NOREAD))) {
		Free (path);
		return (-1);
	}
	Free (path);

	for (fld = 0; fld < RQ_MAX; fld++)  switch (fld) {

#define HEAD	reqheadings[fld].v

	case RQ_COPIES:
		(void)fprintf (fp, "%s%d\n", HEAD, reqbufp->copies);
		break;

	case RQ_DEST:
		(void)fprintf (fp, "%s%s\n", HEAD, reqbufp->destination);
		break;

	case RQ_FILE:
		for (pp = reqbufp->file_list; *pp; pp++)
			(void)fprintf (fp, "%s%s\n", HEAD, *pp);
		break;

	case RQ_FORM:
		if (reqbufp->form)
			(void)fprintf (fp, "%s%s\n", HEAD, reqbufp->form);
		break;

	case RQ_HANDL:
		if ((reqbufp->actions & ACT_SPECIAL) == ACT_IMMEDIATE)
			(void)fprintf (fp, "%s%s\n", HEAD, NAME_IMMEDIATE);
		else if ((reqbufp->actions & ACT_SPECIAL) == ACT_RESUME)
			(void)fprintf (fp, "%s%s\n", HEAD, NAME_RESUME);
		else if ((reqbufp->actions & ACT_SPECIAL) == ACT_HOLD)
			(void)fprintf (fp, "%s%s\n", HEAD, NAME_HOLD);
		break;

	case RQ_NOTIFY:
		if (reqbufp->actions & ACT_MAIL)
			(void)fprintf (fp, "%sM\n", HEAD);
		else if (reqbufp->actions & ACT_WRITE)
			(void)fprintf (fp, "%sW\n", HEAD);
		else if (reqbufp->actions & ACT_NOTIFY)
			(void)fprintf (fp, "%sN\n", HEAD);
		else if (reqbufp->alert && *(reqbufp->alert))
			(void)fprintf (fp, "%s%s\n", HEAD, reqbufp->alert);
		break;

	case RQ_OPTS:
		if (reqbufp->options)
			(void)fprintf (fp, "%s%s\n", HEAD, reqbufp->options);
		break;

	case RQ_PRIOR:
		if (reqbufp->priority != -1)
			(void)fprintf (fp, "%s%d\n", HEAD, reqbufp->priority);
		break;

	case RQ_PAGES:
		if (reqbufp->pages)
			(void)fprintf (fp, "%s%s\n", HEAD, reqbufp->pages);
		break;

	case RQ_CHARS:
		if (reqbufp->charset)
			(void)fprintf (fp, "%s%s\n", HEAD, reqbufp->charset);
		break;

	case RQ_TITLE:
		if (reqbufp->title)
			(void)fprintf (fp, "%s%s\n", HEAD, reqbufp->title);
		break;

	case RQ_MODES:
		if (reqbufp->modes)
			(void)fprintf (fp, "%s%s\n", HEAD, reqbufp->modes);
		break;

	case RQ_TYPE:
		if (reqbufp->input_type)
			(void)fprintf (fp, "%s%s\n", HEAD, reqbufp->input_type);
		break;

	case RQ_USER:
		(void)fprintf (fp, "%s%s\n", HEAD, reqbufp->user);
		break;

	case RQ_RAW:
		if (reqbufp->actions & ACT_RAW)
			(void)fprintf (fp, "%s\n", HEAD);
		break;

	case RQ_FAST:
		if (reqbufp->actions & ACT_FAST)
			(void)fprintf (fp, "%s\n", HEAD);
		break;

	case RQ_STAT:
		(void)fprintf (fp, "%s%#6.4x\n", HEAD, reqbufp->outcome);
		break;
	}

	close_lpfile (fp);
	return (0);
}
