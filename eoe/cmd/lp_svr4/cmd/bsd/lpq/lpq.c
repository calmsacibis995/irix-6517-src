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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/bsd/lpq/RCS/lpq.c,v 1.1 1992/12/14 11:27:36 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Spool Queue examination program
 *
 * lpq [+[n]] [-Pprinter] [user...] [job...]
 *
 * + means continually scan queue until empty
 * -P used to identify printer as per lpr/lprm
 */

#include <errno.h>
#include <signal.h>
#if defined (__STDC__)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include "msgs.h"
#include "lpd.h"
#include "oam_def.h"
#include "oam.h"

char		*User[MAXUSERS];	/* users to process */
int		 Nusers;		/* # of users in user array */
char		*Request[MAXREQUESTS];	/* job number of spool entries */
int		 Nrequests;		/* # of spool requests */
static int	 repeat;		/* + flag indicator */
static int	 slptime = 30;		/* pause between screen refereshes */
int		 lflag;			/* long output option */

#ifdef TERMCAP
int		 dumb;
static char	 PC;
static char	*CL;
static char	*CM;
static char	*SO;
static char	*SE;
static char	*TI;
static char	*TE;

char	*tgetstr();
#endif

#if defined (__STDC__)
static	void	 startup(void);
static	void	 usage(void);
	void	 done(int);
static  int      termcap();
#else
static	void	 startup();
static	void	 usage();
	void	 done();
#undef putchar
extern int      putchar();
#endif

extern char     *gethostname();

main(argc, argv)
int	 argc;
char	*argv[];
{
	register char	*arg;
	register int 	 n;

	Name = argv[0];
	Lhost = gethostname();
	if (!(Printer = getenv("PRINTER")))
		Printer = DEFLP;
	while (--argc) {
		if ((arg = *++argv)[0] == '+') {
			if (arg[1] != '\0')
				if ((n = atoi(&arg[1])) > 0)
					slptime = n;
			repeat++;
		} else if (arg[0] == '-')
			switch (arg[1]) {
			case 'P':		/* printer name */
				if (arg[2])
					Printer = &arg[2];
				else if (argc > 1) {
					argc--;
					Printer = *++argv;
				}
				break;

			case 'l':		/* long output */
				lflag++;
				break;

			default:
				usage();
		} else {
			if(isdigit(arg[0])) {
				if (Nrequests >= MAXREQUESTS)
					fatal("too many requests");
				/*
				** If lpq job# is given construct req-id
				*/
				Request[Nrequests++] = mkreqid(Printer, arg);
			} else if (isrequest(arg)) {
				if (Nrequests >= MAXREQUESTS)
					fatal("too many requests");
				Request[Nrequests++] = arg;
			} else {
				if (Nusers >= MAXUSERS)
					fatal("too many users");
				User[Nusers++] = arg;
			}
		}
	}
	startup();
	if (repeat) {
#ifdef TERMCAP
		dumb = termcap();
		if (TI)
			tputs(TI, 0, putchar);
#endif
		do {
#ifdef TERMCAP
			if (!dumb) {
				tputs(CL, 0, putchar);
				tputs(tgoto(CM, 0, 0), 0, putchar);
			}
#endif
			if ((n = displayq(lflag)) > 0)
				sleep(slptime);
		} while (n > 0);
#ifdef TERMCAP
		if (!dumb) {
			if (SO)
				tputs(SO, 0, putchar);
			printf("Hit return to continue");
			if (SO && SE)
				tputs(SE, 0, putchar);
			while (getchar() != '\n')
				;
			if (TE)
				tputs(TE, 0, putchar);
		}
#endif
	} else
		displayq(lflag);

	done(0);
	/* NOTREACHED */
}

static void
#if defined (__STDC__)
usage(void)
#else
usage()
#endif
{
	printf("usage: lpq [-Pprinter] [-l] [+[n]] [[request-id] [user] [job#] ...]\n");
	exit(1);
}

void
#if defined (__STDC__)
catch(int s)
#else
catch(s)
int	s;
#endif
{
	done(2);
}

static void
#if defined (__STDC__)
startup(void)
#else
startup()
#endif
{
	register int	try = 0;

	if (sigset(SIGHUP, SIG_IGN) != SIG_IGN)
		(void)sigset(SIGHUP, catch);
	if (sigset(SIGINT, SIG_IGN) != SIG_IGN)
		(void)sigset(SIGINT, catch);
	if (sigset(SIGQUIT, SIG_IGN) != SIG_IGN)
		(void)sigset(SIGQUIT, catch);
	if (sigset(SIGTERM, SIG_IGN) != SIG_IGN)
		(void)sigset(SIGTERM, catch);

    	for (;;) {
		if (mopen() == 0) 
			break;
		else if (errno == ENOSPC && try++ < 5) {
			sleep(3);
			continue;
		} else {
	    		lp_fatal(E_LP_MOPEN, NOLOG);
			/*NOTREACHED*/
		}
	}
}

/**
 ** done() - CLEAN UP AND EXIT
 **/
void
#if defined (__STDC__)
done(int rc)
#else
done(rc)
int	rc;
#endif
{
	(void)mclose();
	exit(rc);
	/*NOTREACHED*/
}

/*VARARGS2*/
/*ARGSUSED*/
void
#if defined (__STDC__)
logit(int type, char *msg, ...)
#else
logit(type, msg, va_alist)
int	 type;
char	*msg;
va_dcl
#endif
{
#ifdef DEBUG
	va_list		 argp;

	if (!(type & LOG_MASK))
		return;
#if defined (__STDC__)
	va_start(argp, msg);
#else
	va_start(argp);
#endif
	(void)vfprintf(stderr, msg, argp);
	va_end(argp);
	fputc('\n', stderr);
#endif
}

#ifdef TERMCAP
static char *
#if 0
capstrings[] = { "clear", "cup", "smso", "rmso", "smcup", "rmcup", 0 };
#else
capstrings[] = { "cl", "cm", "so", "se", "ti", "te", 0 };
#endif

static char **
caps[] = { &CL, &CM, &SO, &SE, &TI, &TE };

static int
termcap()
{
	char		*term;
	char		 tbuf[2096];
	static char	 buf[1024];
	char		*bp = buf;
	register char	*cp, **p, ***q;

	if (isatty(1) && (term = getenv("TERM")) && tgetent(tbuf, term) > 0) {
		for (p = capstrings, q = caps; *p; p++, q++)
			**q = tgetstr(*p, &bp);
		if (cp = tgetstr("pad", &bp))
			PC = *cp;
	}
	return(!CL || !CM);
}
#endif
