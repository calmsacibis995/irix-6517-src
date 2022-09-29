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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/bsd/lpc/RCS/topq.c,v 1.1 1992/12/14 11:27:30 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include "lp.h"
#include "printers.h"
#include "class.h"
#include "msgs.h"
#include "requests.h"
#define WHO_AM_I	I_AM_OZ
#include "oam_def.h"
#include "oam.h"

extern char	*Printer;

#if defined(__STDC__)
static	char	* start_change(char *);
static	int	  end_change(char *);
#else
static	char	* start_change();
static	int	  end_change();
#endif

#if defined(__STDC__)
topq_reqid(char *reqid, char *machine)
#else
topq_reqid(reqid, machine)
char	*reqid;
char	*machine;
#endif
{

	/* 
	** This is similar to: lp -i request-id -H IMMEDIATE
	*/ 

	char	*reqfile;			/* Name of request file */
	REQUEST	*oldrqp;
	char	 buf[50];

	if (machine) {
		(void)sprintf(buf, "%s!%s", machine, reqid);	/*???*/
		reqid = buf;
	}
	if (!(reqfile = start_change(reqid)))
		return(0);

	if (!(oldrqp = getrequest(reqfile))) 
		return (0);

	oldrqp->actions |= ACT_IMMEDIATE;

	if (putrequest(reqfile, oldrqp) == -1) {	/* write request file */
	    switch(errno) {
	    default:
		lp_fatal(E_LPP_FPUTREQ, NOLOG); 
		/*NOTREACHED*/
	    }
	}
	free(reqfile);
	(void)end_change(reqid);

	printf("\tmoved %s\n", reqid);
	return(1);
}


/* start_change -- start change request */
static char *
#if defined(__STDC__)
start_change(char *rqid)
#else
start_change(rqid)
char	*rqid;
#endif
{
    	short	 status;
	char	*rqfile;

	snd_msg(S_START_CHANGE_REQUEST, rqid);
	rcv_msg(R_START_CHANGE_REQUEST, &status, &rqfile);

    	switch (status) {
    	case MOK:
		return((char *)strdup(rqfile));
	default:
		return(NULL);
	}
}

static
#if defined(__STDC__)
end_change(char *rqid)
#else
end_change(rqid)
char	*rqid;
#endif
{
    	long	chkbits;
    	short	status;

    	snd_msg(S_END_CHANGE_REQUEST, rqid);
	rcv_msg(R_END_CHANGE_REQUEST, &status, &chkbits);

    	switch (status) {
    	case MOK:
		return(1);
	default:
		return(0);
	}
}

/*
**	topq command handler if user name is specified
**	Find request-ids of all jobs submitted by the user.
**	Save the request-ids
** 	Follow the same method as in topq_reqid for each if the ids.
*/	
#if defined(__STDC__)
topq_user(char *user, char *machine)
#else
topq_user(user, machine)
char	*user;
char	*machine;
#endif
{
	char	 *request_id, *form, *character_set;
	char	  buf[50];
	char	**rqlist = NULL, **pp;
	short	  state, status;
	long	  size, date;
	int	  count = 0;


	if (machine) {
		(void)sprintf(buf, "%s!%s", machine, user);	/*???*/
		user = buf;
	}
	snd_msg(S_INQUIRE_REQUEST, "", Printer, "", user, "");
	do {
		rcv_msg(R_INQUIRE_REQUEST, &status,
					   &request_id,
					   &user,
					   &size,
					   &date,
					   &state,
					   &Printer,
					   &form,
					   &character_set);
		switch (status) {
		case MOK:
		case MOKMORE:
			appendlist(&rqlist, request_id);
			break;
		default:
			return(0);
		}
	} while (status == MOKMORE);
	for (pp = rqlist; *pp; pp++)
		count += topq_reqid(*pp, machine);
	freelist(rqlist);
	return(count);	/* Number of jobs moved to topq */
}
