/*
 *  SpiderTCP Network Daemon
 *
 *  Copyright 1991  Spider Systems Limited
 *
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/etc/netd/0/s.netd.c
 *	@(#)netd.c	1.22
 *
 *	Last delta created	18:01:03 7/6/92
 *	This file extracted	14:52:17 11/13/92
 *
 */

#ident "@(#)netd.c	1.22	11/13/92"

#include <streamio.h>
#include <stdio.h>
#include <errno.h>
#include "debug.h"
#include "utils.h"

#ifdef DEBUG
#define USAGE	"[-D] [-nt] [config_file | -]"
#else /* ~DEBUG */
#define USAGE	"[-nt] [config_file | -]"
#endif /* DEBUG */

#ifndef NETCONF
#define NETCONF	"/etc/config/snetd.options"		/* default configuration file */
#endif

char	*config = NETCONF;		/* network configuration file */

int	trace = 0;			/* trace streams building */
int	nobuild = 0;			/* don't build stream */


char	*prog, *basename();		/* program name */

extern	link();
extern	void configure();
extern	void build();

/****************************  M A I N  *******************************/

main(argc, argv)
int	argc;
char	*argv[];
{
	extern FILE	*freopen();

	prog = basename(argv[0]);


	/*
	 *  Parse the command line arguments.
	 */

	while (--argc, *++argv)
	{
		if (argv[0][0] == '-')
		{
			if (argv[0][1] == '\0')	/* use stdin */
				break;

		more:
			switch (*++argv[0])
			{

			case '\0':
				break;
#ifdef DEBUG
			case 'D':

				debug = 1;
				goto more;
#endif /* DEBUG */


			case 'n':		/* no build */

				/*
				 *  This option implies tracing.
				 */

				nobuild = 1;
				trace = 1;
				goto more;

			case 't':

				trace = 1;
				goto more;

			default:
				exit(usage(prog, USAGE));
			}
		}
		else
			break;
	}

	if (argc > 1) exit(usage(prog, USAGE));

	/*
	 *  Remaining argument is alternate configuration file.
	 */

	if (argc > 0) config = *argv;

	/*
	 *  Detach ourselves from the controlling tty...
	 */
	{
		if (streq(config, "-"))
			config = "standard input";
		else if (freopen(config, "r", stdin) == NULL)
			exit(syserr("failed to open %s", config));

		configure();	/* configure from file */
	}

#ifdef SGI
	if (_daemonize(0,2,trace ? 1 : -1,-1) < 0)
	    exit(1);
#else

#ifndef DEBUG
	setpgrp();
#endif /*DEBUG*/
#endif

	fclose(stdin);		/* finished with input */
	if (!trace) fclose(stdout);	/* bin this too */

	/*
	 *  Build the STREAMS structure as specified.
	 */


	build();

	if (nobuild) exit(0);

	/*
	 *  Fork and return in the parent.
	 *  Sleep forever in the child.
	 */

#ifndef SGI
	if (S_FORK()) exit(0);
#endif  /* SGI */

	pause();

	/*
	 *  Something's wrong if we get out of that alive!
	 *    (Maybe we were kissed by a frog...)
	 */

	exit(error("unexpected system error (signal handling): network going down"));
}

char *
basename(path)
char	*path;
{
	char	*base = path;

	while (*base)
		if (*base++ == '/')
			path = base;
	
	return path;
}

usage(prog, usage)
char	*prog;
char	*usage;
{
	fprintf(stderr, "usage: %s %s\n", prog, usage);
	return(2);
}

error(format, arg1, arg2)
char	*format;
char	*arg1, *arg2;
{
	fprintf(stderr, "%s: ", prog);
	fprintf(stderr, format, arg1, arg2);
	fprintf(stderr, "\n");
	return(1);
}

extern int errno;
extern char *sys_errlist[];
extern int sys_nerr;

syserr(format, arg1, arg2)
char	*format;
char	*arg1, *arg2;
{
	fprintf(stderr, "%s: ", prog);
	fprintf(stderr, format, arg1, arg2);
	if (errno < 0 || errno >= sys_nerr) fprintf(stderr, " (%d)", errno);
	else fprintf(stderr, " (%s [%d])", sys_errlist[errno], errno);
	fprintf(stderr, "\n");
	return(1);
}
