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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpadmin/RCS/output.c,v 1.1 1992/12/14 11:29:42 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "stdio.h"
#include "string.h"
#include "sys/types.h"

#include "lp.h"
#include "printers.h"
#include "msgs.h"
#include "requests.h"

#define	WHO_AM_I	I_AM_LPADMIN
#include "oam.h"

#include "lpadmin.h"


/**
 ** output() - (MISNOMER) HANDLE MESSAGES BACK FROM SPOOLER
 **/

int			output (type)
	int			type;
{
	char			buffer[MSGMAX];

	int			rc;

	short			status;
	char			*dummy;


	if (!scheduler_active)
		switch (type) {

		case R_MOUNT:
		case R_UNMOUNT:
		case R_QUIET_ALERT:
		case R_INQUIRE_PRINTER_STATUS:
		case R_ALLOC_FILES:
		case R_PRINT_REQUEST:
		case R_REJECT_DEST:
		case R_ACCEPT_DEST:
		case R_DISABLE_DEST:
		case R_ENABLE_DEST:
		case R_CANCEL_REQUEST:
		default:
			LP_ERRMSG (ERROR, E_LP_NEEDSCHED);
			done (1);

		case R_UNLOAD_PRINTER:
		case R_UNLOAD_CLASS:
		case R_UNLOAD_PRINTWHEEL:
			if (anyrequests()) {
				LP_ERRMSG (ERROR, E_LP_HAVEREQS);
				done (1);
			}
			/* fall through */

		case R_LOAD_PRINTER:
		case R_LOAD_CLASS:
		case R_LOAD_PRINTWHEEL:
			return (MOK);

		}

	status = MOKMORE;
	while (status == MOKMORE) {

		if ((rc = mrecv(buffer, MSGMAX)) != type) {
			LP_ERRMSG (ERROR, E_LP_MRECV);
			done (1);
		}
			
		switch(type) {

		case R_MOUNT:
		case R_UNMOUNT:
		case R_LOAD_PRINTER:
		case R_UNLOAD_PRINTER:
		case R_LOAD_CLASS:
		case R_UNLOAD_CLASS:
		case R_LOAD_PRINTWHEEL:
		case R_UNLOAD_PRINTWHEEL:
		case R_QUIET_ALERT:
		case R_REJECT_DEST:
		case R_ACCEPT_DEST:
		case R_ENABLE_DEST:
		case R_CANCEL_REQUEST:
			rc = getmessage(buffer, type, &status);
			goto CheckRC;

		case R_DISABLE_DEST:
			rc = getmessage(buffer, type, &status, &dummy);
CheckRC:		if (rc != type) {
				LP_ERRMSG1 (ERROR, E_LP_BADREPLY, rc);
				done (1);
			}
			break;

		case R_INQUIRE_PRINTER_STATUS:
		case R_ALLOC_FILES:
		case R_PRINT_REQUEST:
			return (0);	/* handled by caller */
		}

	}

	return (status);
}
