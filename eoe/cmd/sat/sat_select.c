/*
 * Copyright 1990, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 */

/*
 * sat_select -	selects events for auditing in the kernel
 */

#ident	"$Revision: 1.14 $"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syssgi.h>
#include <sat.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <string.h>

int local = 0;

static int audit_on(int);
static int audit_off(int);
static int audit_query(int);
static cap_value_t cap_audit_control[] = {CAP_AUDIT_CONTROL};
static void check_satd(void);
static void sat_select_usage(void);

#define EQ(a,b) (strcmp(a,b)==0)

static int	ap;
static int	ac;
static char	**av;

/* this function is from test(1) */
char *
nxtarg(int noerrflg) {
	if (ap >= ac) {
		if (noerrflg) {
			ap++;
			return NULL;
		}
		fprintf(stderr, "%s: %s option requires an argument\n",
		    av[0], av[ap-1]);
		exit(1);
	}
	return av[ap++];
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int	i;
	int	found = SAT_FALSE;
	int	state;
	char *	eventstr;
	char *	arg;
	char *	optarg;

	/*
	 * Match -h or -help
	 */
	if (argc == 2 && !strncmp(argv[1], "-h", 2)) {
		sat_select_usage ();
		printf ("Choose events from this list:\n");
		for (i=1; i<SAT_NTYPES; i++) {
			eventstr = sat_eventtostr(i);
			if (eventstr)
				printf ("\t%s\n", eventstr);
		}
		exit(0);
	}

	/*
	 * usage
	 */
	if (argc == 2 && EQ(argv[1], "-?")) {
		sat_select_usage ();
		exit(0);
	}

	if (sysconf(_SC_AUDIT) <= 0) {
		fprintf(stderr, "Audit not configured.\n");
		exit(1);
	}

	ac = argc;
	av = argv;
	ap = 1;

	while ((arg=nxtarg(1)) != NULL) {
		if (EQ(arg, "-l")) {
			local++;
			continue;
		}

		if (EQ(arg, "-out")) {
			printf ("-off all\n");
			for (i=1; i<SAT_NTYPES; i++) {
				if ((state=audit_query(i)) < 0) {
					if (errno == EDOM)
						continue;
					perror(argv[0]);
					exit(1);
				}
				if (state)
					printf("-on %s\n", sat_eventtostr(i));
			}
			exit(0);
		}

		if (EQ(arg, "-on") || EQ(arg, "-e")) {
			check_satd();	/* Make sure there's a sat daemon. */
			optarg = nxtarg(0);
			if (EQ(optarg, "all")) { 
				for (i=1; i<SAT_NTYPES; i++) {
					if (sat_eventtostr(i) == NULL)
						continue;
					if (audit_on(i) < 0) {
						if (errno == EDOM)
							continue;
						perror(argv[0]);
						exit(1);
					}
				}
				continue;
			}
			if ((i = sat_strtoevent(optarg)) < 0) {
				fprintf(stderr, "Unrecognized event: %s\n",
					optarg);
				continue;
			}
			if (audit_on(i) < 0) {
				perror(argv[0]);
				exit(2);
			}
			continue;
		}

		if (EQ(arg, "-off") || EQ(arg, "-E")) {
			optarg = nxtarg(0);
			if (EQ(optarg, "all")) {
				for (i=1; i<SAT_NTYPES; i++) {
					if (sat_eventtostr(i) == NULL)
						continue;
					if (audit_off(i) < 0) {
						if (errno == EDOM)
							continue;
						perror(argv[0]);
						exit(3);
					}
				}
				continue;
			}
			if ((i = sat_strtoevent(optarg)) < 0) {
				fprintf(stderr, "Unrecognized event: %s\n",
					optarg);
				continue;
			}
			if (audit_off(i) < 0) {
				perror(argv[0]);
				exit(4);
			}
			continue;
		}

		/* no recognized args */
		sat_select_usage ();
		exit (5);
	}

	printf ("These events have been selected to be audited%s:\n",
	    (local)? " (this process only)": "");

	for (i=1; i<SAT_NTYPES; i++) {
		if ((state=audit_query(i)) < 0) {
			if (errno == EDOM)
				continue;
			if (errno == ENODATA && local) {
				fprintf(stderr,
				    "No process local event mask.\n");
				exit(1);
			}
			perror(argv[0]);
			exit(1);
		}
		if (state) {
			printf("\t%s\n", sat_eventtostr(i));
			found = SAT_TRUE;
		}
	}
	if (!found)
		printf("\t(none)\n");

	return(0);
}

static void
sat_select_usage(void) {
    fprintf(stderr, "usage: sat_select [-h] [(-on|-off) (all|event ...) ] \n");
    fprintf(stderr, "\t-h   help - list event names.\n");
    fprintf(stderr, "\t-out list event names in sat_select options format.\n");
    fprintf(stderr, "\t-on  selects events to be collected.\n");
    fprintf(stderr, "\t-off selects events to be ignored;\n\n");
}


/*
 * Make sure there's a sat daemon.
 * (But only if there's a user on the other end (stdin and stderr are ttys)).
 */
static void
check_satd(void)
{
	cap_t ocap;

	if (! isatty(0) || ! isatty(2))
		return;

	/* 
	 * If there's alread a daemon running, this should
	 * return -1, errno EBUSY.  If not, there's something
	 * wrong!
	 */
	ocap = cap_acquire (1, cap_audit_control);
	if (syssgi(SGI_SATCTL, SATCTL_REGISTER_SATD) == 0) {
		cap_surrender (ocap);
		fprintf(stderr,
		    "Cannot continue -- No sat daemon running.\n");
		exit(1);
	} else {
		if (errno != EBUSY) {
			cap_surrender (ocap);
			perror("Error finding sat daemon");
			exit(1);
		}
		cap_surrender (ocap);
	}
}

static int
audit_on(int type)
{
	cap_t ocap;
	int r;

	ocap = cap_acquire (1, cap_audit_control);
	if (local)
		r = syssgi(SGI_SATCTL, SATCTL_LOCALAUDIT_ON, type, getppid());
	else
		r = syssgi(SGI_SATCTL, SATCTL_AUDIT_ON, type);
	cap_surrender (ocap);
	return r;
}

static int
audit_off(int type)
{
	cap_t ocap;
	int r;

	ocap = cap_acquire (1, cap_audit_control);
	if (local)
		r = syssgi(SGI_SATCTL, SATCTL_LOCALAUDIT_OFF, type, getppid());
	else
		r = syssgi(SGI_SATCTL, SATCTL_AUDIT_OFF, type);
	cap_surrender (ocap);
	return r;
}

static int
audit_query(int type)
{
	cap_t ocap;
	int r;

	ocap = cap_acquire (1, cap_audit_control);
	if (local)
		r = syssgi(SGI_SATCTL, SATCTL_LOCALAUDIT_QUERY, type, getppid());
	else
		r = syssgi(SGI_SATCTL, SATCTL_AUDIT_QUERY, type);
	cap_surrender (ocap);
	return r;
}
