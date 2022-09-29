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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/forms/RCS/f_head.c,v 1.1 1992/12/14 13:27:12 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "sys/types.h"

#include "lp.h"
#include "form.h"

struct {
	char			*v;
	short			len;
	short			infile;
}			formheadings[FO_MAX] = {

#define	ENTRY(X)	X, sizeof(X)-1

	ENTRY("page length:"),		1,	/* FO_PLEN */
	ENTRY("page width:"),		1,	/* FO_PWID */
	ENTRY("number of pages:"),	1,	/* FO_NP */
	ENTRY("line pitch:"),		1,	/* FO_LPI */
	ENTRY("character pitch:"),	1,	/* FO_CPI */
	ENTRY("character set choice:"),	1,	/* FO_CHSET */
	ENTRY("ribbon color:"),		1,	/* FO_RCOLOR */
	ENTRY("comment:"),		0,	/* FO_CMT */
	ENTRY("alignment pattern:"),	1,	/* FO_ALIGN */

#undef	ENTRY

};

/**
 ** _search_fheading()
 **/

int
#if	defined(__STDC__)
_search_fheading (
	char *			buf
)
#else
_search_fheading (buf)
	char *			buf;
#endif
{
	int			fld;


	for (fld = 0; fld < FO_MAX; fld++)
		if (
			formheadings[fld].v
		     && formheadings[fld].len
		     && CS_STRNEQU(
				buf,
				formheadings[fld].v,
				formheadings[fld].len
			)
		)
			break;

	return (fld);
}
