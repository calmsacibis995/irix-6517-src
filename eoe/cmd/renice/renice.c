/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)renice.c	5.1 (Berkeley) 5/28/85";
#endif /* not lint */

#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <pwd.h>

/* prototype */
int donice(int, int, int, int);
int getargnum(char *str);
void usage(void);
/*
 * Change the priority (nice) of processes
 * or groups of processes which are already
 * running.
 */
main(argc, argv)
	char **argv;
{
	int which = PRIO_PROCESS;
	int who = -1, prio , errs = 0;
	int rel = 0, nflag = 0 , valuearg = 0;

	argc--, argv++;
	if (argc < 2)
		usage();

	if ( ( prio = getargnum(*argv) ) !=  -1 ) {
		argc--, argv++;
	}

	for (; argc > 0; argc--, argv++) {
		if (strcmp(*argv, "-g") == 0) {
			which = PRIO_PGRP;
			continue;
		}
		if (strcmp(*argv, "-u") == 0) {
			which = PRIO_USER;
			continue;
		}
		if (strcmp(*argv, "-n") == 0) {
			rel   = 1;
			nflag = 1;
			continue;
                }
		if (strcmp(*argv, "-p") == 0) {
			which = PRIO_PROCESS;
			continue;
		}
		if (strcmp(*argv, "--") == 0) {
			continue;
                }
		if ( ( valuearg = getargnum(*argv) )  != -1 ) {
			if ( nflag ) {
				prio = valuearg;
				nflag = 0;
				continue;
			}
			else
				who = valuearg;
		}
		else {
			if (which == PRIO_USER) {
				register struct passwd *pwd = getpwnam(*argv);

				if (pwd == NULL) {
					fprintf(stderr, 
						"renice: %s: unknown user\n",
							*argv);
					continue;
				}
				who = pwd->pw_uid;
			}
		}
	}
	errs += donice(which, who, prio, rel);
	if ( who == -1 ) {
		usage();
	}
	exit(errs != 0);

}

void
usage() {
	fprintf(stderr, "usage: renice [-n increment | priority] [ [ -p ] "
		"pids ] [ [ -g ] pgrps ] [ [ -u ] users ]\n");
	exit(1);
}


int
donice(which, who, prio, relative)
	int which, who, prio, relative;
{
	int oldprio;
	extern int errno;
	errno = 0, oldprio = getpriority(which, who);
	if (oldprio == -1 && errno) {
		fprintf(stderr, "renice: %d: ", who);
		perror("getpriority");
		return (1);
	}
	/* if a relative increment, add in current value */
	if (relative)
		prio += oldprio;
	if (prio > PRIO_MAX)
		prio = PRIO_MAX;
	if (prio < PRIO_MIN)
		prio = PRIO_MIN;

	if (setpriority(which, who, prio) < 0) {
		fprintf(stderr, "renice: %d: ", who);
		perror("setpriority");
		return (1);
	}
	return (0);
}

int
getargnum(char *str) {
	int i = 0, pre = 0;
	int prio = 0;
	int neg = 1; 
	if (str[0] == '\0') {
		usage();
	}
	if (str[0] == '-') 
		neg = -1;	
	if (str[0] == '-' || str[0] == '+')
		i++, pre = 1;
	prio = 0;	
	while (str[i] != '\0') {
		if (!isdigit(str[i]))
			return(-1);
		prio = prio * 10 + str[i] - '0';
		i++;
	}
	/* just in case the argument is a single + or - */
	if (pre && i == 1)
		usage();
	return(prio * neg);
}
