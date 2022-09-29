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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/printers/RCS/p_head.c,v 1.1 1992/12/14 13:33:35 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "sys/types.h"

#include "lp.h"
#include "printers.h"

struct {
	char			*v;
	short			len,
				okremote;
}			prtrheadings[PR_MAX] = {

#define	ENTRY(X)	X, sizeof(X)-1

	ENTRY("Banner:"),	   0,	/* PR_BAN */
	ENTRY("CPI:"),		   0,	/* PR_CPI */
	ENTRY("Character sets:"),  1,	/* PR_CS */
	ENTRY("Content types:"),   1,	/* PR_ITYPES */
	ENTRY("Device:"),	   0,	/* PR_DEV */
	ENTRY("Dial:"),		   0,	/* PR_DIAL */
	ENTRY("Fault:"),	   0,	/* PR_RECOV */
	ENTRY("Interface:"),	   0,	/* PR_INTFC */
	ENTRY("LPI:"),		   0,	/* PR_LPI */
	ENTRY("Length:"),	   0,	/* PR_LEN */
	ENTRY("Login:"),	   0,	/* PR_LOGIN */
	ENTRY("Printer type:"),    1,	/* PR_PTYPE */
	ENTRY("Remote:"),	   1,	/* PR_REMOTE */
	ENTRY("Speed:"),	   0,	/* PR_SPEED */
	ENTRY("Stty:"),		   0,	/* PR_STTY */
	ENTRY("Width:"),	   0,	/* PR_WIDTH */
#if	defined(CAN_DO_MODULES)
	ENTRY("Modules:"),	   0,	/* PR_MODULES */
#endif

#undef	ENTRY

};
