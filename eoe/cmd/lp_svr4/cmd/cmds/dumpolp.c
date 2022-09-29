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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/cmds/RCS/dumpolp.c,v 1.1 1992/12/14 11:28:22 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "stdio.h"
#include "sys/types.h"

#include "lp.h"

#define	QSTATUS	"qstatus"
#define	PSTATUS	"pstatus"

#define DESTMAX	14

#define Q_RSIZE	81

struct qstat {
	char	q_dest[DESTMAX+1];	/* destination */
	short	q_accept;		/* 1 iff lp accepting requests */
	time_t	q_date;
	char	q_reason[Q_RSIZE];	/* reason for reject */
};

#define	P_RSIZE	81

struct pstat {
	char	p_dest[DESTMAX+1];	/* destination name of printer */
	int	p_pid;
	char	p_rdest[DESTMAX+1];
	int	p_seqno;
	time_t	p_date;
	char	p_reason[P_RSIZE];	/* reason for being disable */
	short	p_flags;
};

#define	P_ENAB	1		/* printer enabled */

extern char		*getenv();

/**
 ** main()
 **/

int			main ()
{
	char			*spooldir,
				*pstatus,
				*qstatus;

	struct pstat		pbuf;

	struct qstat		qbuf;

	register struct pstat	*p	= &pbuf;

	register struct qstat	*q	= &qbuf;

	register FILE		*fp;


	if (!(spooldir = getenv("SPOOLDIR")))
		spooldir = SPOOLDIR;

	pstatus = makepath(spooldir, PSTATUS, (char *)0);
	if ((fp = fopen(pstatus, "r"))) {
		while (fread((char *)p, sizeof(*p), 1, fp) == 1) {
			printf (
				"PRTR %*.*s %c %.*s\n",
				DESTMAX,
				DESTMAX,
				p->p_dest,
				(p->p_flags & P_ENAB? 'E' : 'D'),
				P_RSIZE,
				p->p_reason
			);
		}
		fclose (fp);
	}

	qstatus = makepath(spooldir, QSTATUS, (char *)0);
	if ((fp = fopen(qstatus, "r"))) {
		while (fread((char *)q, sizeof(*q), 1, fp) == 1) {
			printf (
				"DEST %*.*s %c %.*s\n",
				DESTMAX,
				DESTMAX,
				q->q_dest,
				(q->q_accept? 'A' : 'R'),
				Q_RSIZE,
				q->q_reason
			);
		}
		fclose (fp);
	}
}
