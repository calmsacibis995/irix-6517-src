/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uadmin:uadmin.c	1.4.3.1"

/***************************************************************************
 * Command: uadmin
 *
 * Inheritable Privileges: P_SYSOPS
 *       Fixed Privileges: None
 *
 ***************************************************************************/

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#ifndef sgi
#include <priv.h>
#endif
#include <sys/uadmin.h>
#include <sys/capability.h>

static char *Usage = "Usage: %s cmd fcn\n";

static int isnumber(char *);


/*
 * Procedure:     main
 *
 * Restrictions:
 *                uadmin(2): none
*/

int
main(int argc, char *argv[])
{
	register int cmd, fcn;
	sigset_t set, oset;
	int tmperr;
	cap_t ocap;
	cap_value_t cap_shutdown = CAP_SHUTDOWN;

	if (argc != 3) {
		fprintf(stderr, Usage, argv[0]);
		exit(1);
	}

	sigfillset(&set);
	sigprocmask(SIG_BLOCK, &set, &oset);

        if (isnumber(argv[1]) && isnumber(argv[2])) {
                cmd = atoi(argv[1]);
                fcn = atoi(argv[2]);
        }
        else {
                fprintf(stderr, "%s: cmd and fcn must be integers\n",argv[0]);
                exit(1);
        }

	ocap = cap_acquire(1, &cap_shutdown);
	if (uadmin(cmd, fcn, 0) < 0) {
		tmperr=errno;
		errno=tmperr;
		perror("uadmin");
	}
	cap_surrender(ocap);

	sigprocmask(SIG_BLOCK, &oset, (sigset_t *)0);
	return(0);
}


/*
 * Procedure:     isnumber
 *
 * Restrictions:  none
*/

static int
isnumber(char *s)
{
        register int c;

        while(c = *s++)
                if(!isdigit(c))
                        return(0);
        return(1);
}
