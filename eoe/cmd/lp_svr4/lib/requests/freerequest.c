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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/requests/RCS/freerequest.c,v 1.1 1992/12/14 13:33:50 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "sys/types.h"
#include "stdlib.h"

#include "lp.h"
#include "requests.h"

/**
 ** freerequest() - FREE STRUCTURE ALLOCATED FOR A REQUEST STRUCTURE
 **/

void
#if	defined(__STDC__)
freerequest (
	REQUEST *		reqbufp
)
#else
freerequest (reqbufp)
	register REQUEST	*reqbufp;
#endif
{
	if (!reqbufp)
		return;
	if (reqbufp->destination)
		Free (reqbufp->destination);
	if (reqbufp->file_list)
		freelist (reqbufp->file_list);
	if (reqbufp->form)
		Free (reqbufp->form);
	if (reqbufp->alert)
		Free (reqbufp->alert);
	if (reqbufp->options)
		Free (reqbufp->options);
	if (reqbufp->pages)
		Free (reqbufp->pages);
	if (reqbufp->charset)
		Free (reqbufp->charset);
	if (reqbufp->modes)
		Free (reqbufp->modes);
	if (reqbufp->title)
		Free (reqbufp->title);
	if (reqbufp->input_type)
		Free (reqbufp->input_type);
	if (reqbufp->user)
		Free (reqbufp->user);
	return;
}
