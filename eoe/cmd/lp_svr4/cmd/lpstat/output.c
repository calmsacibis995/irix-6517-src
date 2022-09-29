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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpstat/RCS/output.c,v 1.1 1992/12/14 11:36:07 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "stdio.h"
#include "errno.h"
#include "sys/types.h"

#include "lp.h"
#include "printers.h"
#include "class.h"
#include "msgs.h"

#define	WHO_AM_I	I_AM_LPSTAT
#include "oam.h"

#include "lpstat.h"

static char		time_buf[50];

/**
 ** output() - RECEIVE MESSAGE BACK FROM SPOOLER, PRODUCE OUTPUT
 **/

int
#if	defined(__STDC__)
output (
	int			type
)
#else
output (type)
	int			type;
#endif
{
	char			buffer[MSGMAX];

	int			rc;

	char			*class,
				*user,
				*reject_reason,
				*request_id,
				*printer,
				*form,
				*character_set,
				*disable_reason;

	short			printer_status,
				class_status,
				state,
				status,
				rank = 0;

	long			size,
				enable_date,
				reject_date,
				date;


status=0;
request_id=NULL;
user=NULL;
size=0;
date=0;
state=0;
printer=NULL;
form=NULL;
character_set=NULL;

	if (!scheduler_active) {
		static int		complained	= 0;

		if (!complained) {
			LP_ERRMSG (ERROR, E_LP_NEEDSCHED);
			exit_rc = 1;
			complained = 1;
		}
		return (MOK);
	}

	status = MOKMORE;
	while (status == MOKMORE) {

		if ((rc = mrecv(buffer, MSGMAX)) != type) {
			if (rc == -1 && errno == EIDRM)
				LP_ERRMSG (ERROR, E_LP_MRECV);
			else
				LP_ERRMSG1 (ERROR, E_LP_BADREPLY, rc);
			done (1);
		}
			
		switch(type) {

		case R_INQUIRE_REQUEST:
			if (getmessage(
				buffer,
				R_INQUIRE_REQUEST,
				&status,
				&request_id,
				&user,
				&size,
				&date,
				&state,
				&printer,
				&form,
				&character_set
			) == -1) {
				LP_ERRMSG1 (ERROR, E_LP_GETMSG, PERROR);
				done (1);
			}

			switch (status) {

			case MOK:
			case MOKMORE:
				cftime (time_buf, NULL, &date);
				putoline (
					request_id,
					user,
					size,
					time_buf,
					state,
					printer,
					form,
					character_set,
					++rank
				);
				break;

			}
			break;

		case R_INQUIRE_REQUEST_RANK:
			if (getmessage(
				buffer,
				R_INQUIRE_REQUEST_RANK,
				&status,
				&request_id,
				&user,
				&size,
				&date,
				&state,
				&printer,
				&form,
				&character_set,
				&rank
			) == -1) {
				LP_ERRMSG1 (ERROR, E_LP_GETMSG, PERROR);
				done (1);
			}

			switch (status) {

			case MOK:
			case MOKMORE:
				cftime (time_buf, NULL, &date);
				putoline (
					request_id,
					user,
					size,
					time_buf,
					state,
					printer,
					form,
					character_set,
					rank
				);
				break;

			}
			break;

		case R_INQUIRE_PRINTER_STATUS:
			if (getmessage(
				buffer,
				R_INQUIRE_PRINTER_STATUS,
				&status,
				&printer,
				&form,
				&character_set,
				&disable_reason,
				&reject_reason,
				&printer_status,
				&request_id,
				&enable_date,
				&reject_date
			) == -1) {
				LP_ERRMSG1 (ERROR, E_LP_GETMSG, PERROR);
				done (1);
			}

			switch (status) {

			case MOK:
			case MOKMORE:
				switch (inquire_type) {
				case INQ_ACCEPT:
					cftime (time_buf, NULL, &reject_date);
					putqline (
						printer,
						(printer_status & PS_REJECTED),
						time_buf,
						reject_reason
					);
					break;

				case INQ_PRINTER:
					cftime (time_buf, NULL, &enable_date);
					putpline (
						printer,
						printer_status,
						request_id,
						time_buf,
						disable_reason,
						form,
						character_set
					);
					break;

				case INQ_STORE:
					add_mounted (
						printer,
						form,
						character_set
					);
					break;

				}
				break;

			}
			break;

		case R_INQUIRE_CLASS:
			if (getmessage(
				buffer,
				R_INQUIRE_CLASS,
				&status,
				&class,
				&class_status,
				&reject_reason,
				&reject_date
			) == -1) {
				LP_ERRMSG1 (ERROR, E_LP_GETMSG,	PERROR);
				done (1);
			}

			switch (status) {

			case MOK:
			case MOKMORE:
				switch (inquire_type) {
				case INQ_ACCEPT:
					cftime (time_buf, NULL, &reject_date);
					putqline (
						class,
						(class_status & CS_REJECTED),
						time_buf,
						reject_reason
					);
					break;
				}
				break;

			}
			break;
		}
	}

	switch (status) {

	case MOK:	
	case MNOINFO:
	case MNODEST:
	case MUNKNOWN:
		return (status);

	default:	/* we are lost */
		LP_ERRMSG1 (ERROR, E_LP_BADSTATUS, status);
		done(1);

	}
	/*NOTREACHED*/
}
