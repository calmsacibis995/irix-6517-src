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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/lib/printers/RCS/freeprinter.c,v 1.1 1992/12/14 13:33:20 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* EMACS_MODES: !fill, lnumb, !overwrite, !nodelete, !picture */

#include "sys/types.h"
#include "stdlib.h"

#include "lp.h"
#include "printers.h"

/**
 **  freeprinter() - FREE MEMORY ALLOCATED FOR PRINTER STRUCTURE
 **/

void			freeprinter (pp)
	PRINTER			*pp;
{
	if (!pp)
		return;
	if (pp->name)
		Free (pp->name);
	if (pp->char_sets)
		freelist (pp->char_sets);
	if (pp->input_types)
		freelist (pp->input_types);
	if (pp->device)
		Free (pp->device);
	if (pp->dial_info)
		Free (pp->dial_info);
	if (pp->fault_rec)
		Free (pp->fault_rec);
	if (pp->interface)
		Free (pp->interface);
	if (pp->printer_type)
		Free (pp->printer_type);
	if (pp->remote)
		Free (pp->remote);
	if (pp->speed)
		Free (pp->speed);
	if (pp->stty)
		Free (pp->stty);
	if (pp->description)
		Free (pp->description);
	if (pp->fault_alert.shcmd)
		Free (pp->fault_alert.shcmd);
#if	defined(CAN_DO_MODULES)
	if (pp->modules)
		freelist (pp->modules);
#endif
	if (pp->printer_types)
		freelist (pp->printer_types);
	return;
}
