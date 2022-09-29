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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpadmin/RCS/rmdest.c,v 1.1 1992/12/14 11:29:45 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "stdio.h"
#include "ctype.h"
#include "errno.h"
#include "sys/types.h"

#include "lp.h"
#include "msgs.h"
#include "access.h"
#include "class.h"
#include "printers.h"

#define	WHO_AM_I	I_AM_LPADMIN
#include "oam.h"

#include "lpadmin.h"

extern void		fromallclasses();

/**
 ** rmdest() - REMOVE DESTINATION
 **/

void			rmdest (aclass, dest)
	int			aclass;
	char			*dest;
{
	int			rc,
				type;


	if (!aclass)
		type = S_UNLOAD_PRINTER;
	else
		type = S_UNLOAD_CLASS;


	send_message(type, dest, "", "");
	rc = output(type + 1);

	switch (rc) {
	case MOK:
	case MNODEST:
		BEGIN_CRITICAL
			if (
				aclass && delclass(dest) == -1
			     || !aclass && delprinter(dest) == -1
			) {
				if (rc == MNODEST && errno == ENOENT)
					LP_ERRMSG1 (
						ERROR,
						E_ADM_NODEST,
						dest
					);

				else
fprintf(stderr,"lpadmin: error\n");
/*
					LP_ERRMSG2 (
						ERROR,
(rc == MNODEST? (aclass? E_LP_DELCLASS : E_LP_DELPRINTER) : E_ADM_DELSTRANGE),
						dest,
						PERROR
					);
*/

				done(1);
			}
		END_CRITICAL

		/*
		 * S_UNLOAD_PRINTER tells the Spooler to remove
		 * the printer from all classes (in its internal
		 * tables, of course). So it's okay for us to do
		 * the same with the disk copies.
		 */
		if (!aclass)
			fromallclasses (dest);

		if (STREQU(getdflt(), dest))
			newdflt (NAME_NONE);

		break;

	case MBUSY:
		LP_ERRMSG1 (ERROR, E_ADM_DESTBUSY, dest);
		done (1);

	case MNOPERM:	/* taken care of up front */
	default:
		LP_ERRMSG1 (ERROR, E_LP_BADSTATUS, rc);
		done (1);
		break;

	}
	return;
}
