/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)chroot:chroot.c	1.6"	*/
/* #ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/chroot/RCS/chroot.c,v 1.7 1998/04/03 23:52:13 bitbug Exp $" */

/*
 * Internationalization by frank@ceres.esd.sgi.com
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <locale.h>
#include <fmtmsg.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>
#include <sys/capability.h>

/* locals */

char	cmd_label[] = "UX:chroot";

/*
 * print a system error message
 */
static void
printsyserr()
{
	_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
	    "%s", strerror(oserror()));
}

/*
 * Procedure:     main
 */
main(argc, argv)
int argc;
char **argv;
{
	const cap_value_t cv = CAP_CHROOT;
	cap_t ocap, ecap;
	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	if(argc < 3) {
	    (void)_sgi_nl_usage(SGINL_USAGE, cmd_label,
		gettxt(_SGI_DMMX_usage_chroot,
		    "chroot rootdir command [arg ... ]"));
	    exit(1);
	}

	ecap = cap_get_proc();
	if (cap_envl(0, cv, 0) == -1) {
	    (void) cap_free(ecap);
	    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		gettxt(_SGI_DMMX_notsupuser, "not super-user"));
	    exit(1);
	}

	ocap = cap_acquire(1, &cv);
	if(chroot(argv[1]) < 0) {
	    cap_surrender(ocap);
	    (void) cap_free(ecap);
	    (void)printsyserr();
	    exit(1);
	}
	(void) cap_free(ocap);

	if(chdir("/") < 0) {
	    (void) cap_free(ecap);
	    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		gettxt(_SGI_DMMX_cantchdir2root,
		    "can't chdir to new root"));
	    exit(1);
	}

	if (cap_set_proc(ecap) < 0) {
	    (void) cap_free(ecap);
	    (void) printsyserr();
	    exit(1);
	}

	if (cap_free(ecap) < 0) {
	    (void) printsyserr();
	    exit(1);
	}

	execv(argv[2], &argv[2]);
	(void)printsyserr();
	exit(1);
}
