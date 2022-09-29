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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/requests/RCS/r_head.c,v 1.1 1992/12/14 13:34:02 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "sys/types.h"

#include "lp.h"
#include "requests.h"

struct {
	char			*v;
	short			len;
}			reqheadings[RQ_MAX] = {

#define	ENTRY(X)	X, sizeof(X)-1

	ENTRY("C "),	/* RQ_COPIES */
	ENTRY("D "),	/* RQ_DEST */
	ENTRY("F "),	/* RQ_FILE */
	ENTRY("f "),	/* RQ_FORM */
	ENTRY("H "),	/* RQ_HANDL */
	ENTRY("N "),	/* RQ_NOTIFY */
	ENTRY("O "),	/* RQ_OPTS */
	ENTRY("P "),	/* RQ_PRIOR */
	ENTRY("p "),	/* RQ_PGES */
	ENTRY("S "),	/* RQ_CHARS */
	ENTRY("T "),	/* RQ_TITLE */
	ENTRY("Y "),	/* RQ_MODES */
	ENTRY("t "),	/* RQ_TYPE */
	ENTRY("U "),	/* RQ_USER */
	ENTRY("r "),	/* RQ_RAW */
	ENTRY("a "),	/* RQ_FAST */
	ENTRY("s "),	/* RQ_STAT */
/*	ENTRY("x "),	/* reserved (slow filter) */
/*	ENTRY("y "),	/* reserved (fast filter) */
/*	ENTRY("z "),	/* reserved (printer name) */

#undef	ENTRY

};
