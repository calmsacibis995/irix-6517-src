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
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/cmd/lpstat/RCS/parse.c,v 1.1 1992/12/14 11:36:14 suresh Exp $"
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include "string.h"
#include "sys/types.h"
#include "stdlib.h"

#include "lp.h"
#include "printers.h"

#define	WHO_AM_I	I_AM_LPSTAT
#include "oam.h"

#include "lpstat.h"


typedef struct execute {
	char			*list;
	void			(*func)();
	int			inquire_type;
	struct execute		*forward;
}			EXECUTE;

int			D		= 0;

static int		r		= 0;

unsigned int		verbosity	= 0;

extern char		*optarg;

extern int		getopt(),
			optind,
			opterr,
			optopt;


#if	defined(__STDC__)
static void		usage ( void );
#else
static void		usage();
#endif

#if	defined(CAN_DO_MODULES)
#define OPT_LIST	"a:c:do:p:rstu:v:f:DS:lLRH"
#else
#define OPT_LIST	"a:c:do:p:rstu:v:f:DS:lLR"
#endif

#define	QUEUE(LIST, FUNC, TYPE) \
	{ \
		next->list = LIST; \
		next->func = FUNC; \
		next->inquire_type = TYPE; \
		next->forward = (EXECUTE *)Malloc(sizeof(EXECUTE)); \
		(next = next->forward)->forward = 0; \
	}

/**
 ** parse() - PARSE COMMAND LINE OPTIONS
 **/

/*
 * This routine parses the command line, builds a linked list of
 * function calls desired, then executes them in the order they 
 * were received. This is necessary as we must apply -l to all 
 * options. So, we could either stash the calls away, or go
 * through parsing twice. I chose to build the linked list.
 */

void
#if	defined(__STDC__)
parse (
	int			argc,
	char **			argv
)
#else
parse (argc, argv)
	int			argc;
	char			**argv;
#endif
{
	int			optsw;
	int			ac;
	int			need_mount	= 0;

	char **			av;
	char *			p;
	char **			list;

	EXECUTE			linked_list;

	register EXECUTE *	next		= &linked_list;


	next->forward = 0;

	/*
	 * Add a fake value to the end of the "argv" list, to
	 * catch the case that a valued-option comes last.
	 */
	av = (char **)Malloc((argc + 2) * sizeof(char *));
	for (ac = 0; ac < argc; ac++)
		av[ac] = argv[ac];
	av[ac++] = "--";

	opterr = 0;

	while ((optsw = getopt(ac, (char * const *)av, OPT_LIST)) != -1) {

		switch(optsw) {

		/*
		 * These option letters MAY take a value. Check the value;
		 * if it begins with a '-', assume it's really the next
		 * argument.
		 */
		case 'a':
		case 'c':
		case 'o':
		case 'p':
		case 'u':
		case 'v':
		case 'f':
		case 'S':
			if (*optarg == '-') {
				/*
				 * This will work if we were given
				 *
				 *	-x -foo
				 *
				 * but would fail if we were given
				 *
				 *	-x-foo
				 */
				optind--;
				optarg = NAME_ALL;
			}
			break;
		}
	
		switch(optsw) {
		case 'a':	/* acceptance status */
			QUEUE (optarg, do_accept, INQ_ACCEPT);
			break;

		case 'c':	/* class to printer mapping */
			QUEUE (optarg, do_class, 0);
			break;

		case 'd':	/* default destination */
			QUEUE (0, def, 0);
			break;

		case 'D':	/* Description of printers */
			D = 1;
			break;

		case 'f':	/* do forms */
			QUEUE (optarg, do_form, 0);
			need_mount = 1;
			break;

#if	defined(CAN_DO_MODULES)
		case 'H':	/* show modules pushed for printer */
			verbosity |= V_MODULES;
			break;
#endif

		case 'l':	/* verbose output */
			verbosity |= V_LONG;
			verbosity &= ~V_BITS;
			break;

		case 'L':	/* special debugging output */
			verbosity |= V_BITS;
			verbosity &= ~V_LONG;
			break;

		case 'o':	/* output for destinations */
			QUEUE (optarg, do_request, 0);
			break;

		case 'p':	/* printer status */
			QUEUE (optarg, do_printer, INQ_PRINTER);
			break;

		case 'R':	/* show rank in queue */
			verbosity |= V_RANK;
			break;

		case 'r':	/* is scheduler running? */
			QUEUE (0, running, 0);
			r = 1;
			break;

		case 's':	/* configuration summary */
			QUEUE (0, running, 0);
			QUEUE (0, def, 0);
			QUEUE (NAME_ALL, do_class, 0);
			QUEUE (NAME_ALL, do_device, 0);
			QUEUE (NAME_ALL, do_form, 0);
			QUEUE (NAME_ALL, do_charset, 0);
			r = 1;
			need_mount = 1;
			break;

		case 'S':	/* character set info */
			QUEUE (optarg, do_charset, 0);
			need_mount = 1;
			break;

		case 't':	/* print all info */
			QUEUE (0, running, 0);
			QUEUE (0, def, 0);
			QUEUE (NAME_ALL, do_class, 0);
			QUEUE (NAME_ALL, do_device, 0);
			QUEUE (NAME_ALL, do_accept, INQ_ACCEPT);
			QUEUE (NAME_ALL, do_printer, INQ_PRINTER);
			QUEUE (NAME_ALL, do_form, 0);
			QUEUE (NAME_ALL, do_charset, 0);
			QUEUE (NAME_ALL, do_request, 0);
			r = 1;
			need_mount = 1;
			break;

		case 'u':	/* output by user */
			QUEUE (optarg, do_user, INQ_USER);
			break;

		case 'v':	/* printers to devices mapping */
			QUEUE (optarg, do_device, 0);
			break;

		default:
			if (optopt == '?') {
				usage ();
				done (0);
			}

			(p = "-X")[1] = optopt;

			if (strchr(OPT_LIST, optopt))
				LP_ERRMSG1 (ERROR, E_LP_OPTARG, p);
			else
				LP_ERRMSG1 (ERROR, E_LP_OPTION, p);
			done(1);
			break;
		}

	}

	/*
	 * Note: "argc" here, not "ac", to skip our fake option.
	 * We could use either "argv" or "av", since for the range
	 * of interest they're the same.
	 */
	list = 0;
	while (optind < argc)
		if (addlist(&list, av[optind++]) == -1) {
			LP_ERRMSG (ERROR, E_LP_MALLOC);
			done(1);
		}
	if (list)
		QUEUE (sprintlist(list), do_request, 0);

	if (argc == 1 || (verbosity & V_RANK) && argc == 2)
		QUEUE (getname(), do_user, INQ_USER);

	startup ();

	/*
		Linked list is completed, load up mount info if
		needed then do the requests
	*/

	if (need_mount) {
		inquire_type = INQ_STORE;
		do_printer (alllist);
	}

	for (next = &linked_list; next->forward; next = next->forward) {
		inquire_type = next->inquire_type;
		if (!next->list)
			(*next->func) ();
		else if (!*next->list)
			(*next->func) (alllist);
		else
			(*next->func) (getlist(next->list, LP_WS, LP_SEP));
	}

	return;
}

/**
 ** usage() - PRINT USAGE MESSAGE
 **/

static void
#if	defined(__STDC__)
usage (
	void
)
#else
usage ()
#endif
{
      LP_OUTMSG(INFO, E_STAT_USAGE);
      LP_OUTMSG(INFO|MM_NOSTD, E_STAT_USAGE1);
      LP_OUTMSG(INFO|MM_NOSTD, E_STAT_USAGE2);
#if	defined(CAN_DO_MODULES)
      LP_OUTMSG(INFO|MM_NOSTD, E_STAT_USAGE3);
#else
      LP_OUTMSG(INFO|MM_NOSTD, E_STAT_USAGE4);
#endif
      LP_OUTMSG(INFO|MM_NOSTD, E_STAT_USAGE5);
      LP_OUTMSG(INFO|MM_NOSTD, E_STAT_USAGE6);

	return;
}
