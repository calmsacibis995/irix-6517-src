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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/bsd/lprm/RCS/lprm.c,v 1.1 1992/12/14 11:27:49 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <pwd.h>
#include "oam_def.h"
#include "oam.h"
#if	defined(__STDC__)
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "lpd.h"

extern char   *gethostname();

/*
 * (SunOS )
 * Stuff for handling job specifications
 */
char		*User[MAXUSERS];	/* users to process */
int		 Nusers;		/* # of users in user array */
char		*Request[MAXREQUESTS];	/* job number of spool entries */
int		 Nrequests;		/* # of spool requests */
char		*Person;		/* name of person doing lprm */

#if defined(__STDC__)
static	void	 startup(void);
static	void	 usage(void);
	void	 done(int);
#else
static	void	 startup();
static	void	 usage();
	void	 done();
#endif

/**
 ** main()
 **/
main (argc, argv)
int	 argc;
char	*argv[];
{

	register char	*arg;
	struct passwd   *p;

	Name = argv[0];
	Lhost = gethostname();
	if (getuid() == 0 || 
	   !(Person = getlogin()) || 
	   !(p = getpwnam(Person))) {
		if (!(p = getpwuid(getuid())))
			fatal("Who are you?");
		Person = p->pw_name;
	}
	setuid(p->pw_uid);	/* so lpsched knows who this really is */

	/* set printer name now in case we have to construct request-ids */
	if (!(Printer = getenv("PRINTER")))
                Printer = DEFLP;
        while (--argc) {
                if ((arg = *++argv)[0] == '-')
                        switch (arg[1]) {      
                        case 'P':         
                                if (arg[2])
                                        Printer = &arg[2];
                                else if (argc > 1) {       
                                        argc--;      
                                        Printer = *++argv;
                                }                          
                                break;
			case '\0':     
                                if (!Nusers) {
                                        Nusers = -1;
                                        break;      
                                }              
                        default:
                                usage();
                        } 
 		else {    
                       	if (Nusers < 0)
                                usage();
			/*
			** Allow for LPD's job# and S5's request-ids
			*/
                        if (isdigit(arg[0])) {
				/*
				** Handle the case of: lprm -P printer job#
				** If lprm job# is given construct req-id
				*/
                                if (Nrequests >= MAXREQUESTS)
                                        fatal("Too many requests");
                                Request[Nrequests++] = mkreqid(Printer, arg);
			} else if(isrequest(arg)) {
                                if (Nrequests >= MAXREQUESTS)
                                        fatal("Too many requests");
                                Request[Nrequests++] = arg;       
                        } else {                              
                                if (Nusers >= MAXUSERS)
                                        fatal("Too many users");
                                User[Nusers++] = arg;             
                        }                            
                }         
        }                        
	startup();
        rmjob();
	done(0);
	/*NOTREACHED*/
}
static void
#if defined(__STDC__)
usage(void)
#else
usage()
#endif
{       
        printf("usage: lprm [-] [-Pprinter] [[request-id] [job#] [user] ...]\n");
        exit(2);                                                      
}                

/**
 ** catch() - CATCH INTERRUPT, HANGUP, ETC.
 **/

static void
#if defined(__STDC__)
catch(int sig)
#else
catch(sig)
int	sig;
#endif
{
	done(2);
}

static void
#if defined(__STDC__)
startup(void)
#else
startup()
#endif
{
	register int	try = 0;

	if(sigset(SIGHUP, SIG_IGN) != SIG_IGN)
		sigset(SIGHUP, catch);
	if(sigset(SIGINT, SIG_IGN) != SIG_IGN)
		sigset(SIGINT, catch);
	if(sigset(SIGQUIT, SIG_IGN) != SIG_IGN)
		sigset(SIGQUIT, catch);
	if(sigset(SIGTERM, SIG_IGN) != SIG_IGN)
		sigset(SIGTERM, catch);

	/*
	 * Open a private queue for messages to the Spooler.
	 */
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
 ** done() - CLOSE THE MESSAGE QUEUE TO THE SPOOLER
 **/
void
#if defined(__STDC__)
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
