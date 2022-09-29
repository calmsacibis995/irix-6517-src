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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/model/RCS/lp.page.c,v 1.1 1992/12/14 13:35:23 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "stdio.h"
#include "signal.h"

#include "lp.h"

void			flush_page(),
			sighup(),
			sigint(),
			sigquit(),
			sigpipe(),
			sigterm();

int			lines,
			n;

/**
 ** main()
 **/

int			main (argc, argv)
	int			argc;
	char			*argv[];
{
	char			buf[BUFSIZ];


	signal (SIGHUP, sighup);
	signal (SIGINT, sigint);
	signal (SIGQUIT, sigint);
	signal (SIGPIPE, sigpipe);
	signal (SIGTERM, sigterm);

	if (argc != 2)
		lines = 66;

	else if ((lines = atoi(argv[1])) < 1)
		lines = 66;

	n = 0;
	while (gets(buf)) {
		puts (buf);
		if (++n > lines)
			n = 1;
	}

	flush_page ();

	return (0);
}

/**
 ** flush_page()
 **/

void			flush_page ()
{
	while (n++ < lines)
		putchar ('\n');
	fflush (stdout);
	return;
}

/**
 ** sighup() - CATCH A HANGUP (LOSS OF CARRIER)
 **/

void			sighup ()
{
	signal (SIGHUP, SIG_IGN);
	fprintf (stderr, HANGUP_FAULT);
	exit (1);
}

/**
 ** sigint() - CATCH AN INTERRUPT
 **/

void			sigint ()
{
	signal (SIGINT, SIG_IGN);
	fprintf (stderr, INTERRUPT_FAULT);
	exit (1);
}

/**
 ** sigpipe() - CATCH EARLY CLOSE OF PIPE
 **/

void			sigpipe ()
{
	signal (SIGPIPE, SIG_IGN);
	fprintf (stderr, PIPE_FAULT);
	exit (1);
}

/**
 ** sigterm() - CATCH A TERMINATION SIGNAL AND FORCE FULL PAGE
 **/

void			sigterm ()
{
	signal (SIGTERM, SIG_IGN);
	flush_page ();
	exit (0);
}
