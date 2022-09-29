/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/abort.c	1.18"
/*	3.0 SID #	1.4	*/
/*LINTLIBRARY*/
/*
 *	abort() - terminate current process with dump via SIGABRT
 */

#include "synonyms.h"
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "gen_extern.h"

/*
 * ANSI claims that atexit functions are to be called only on normal
 * process exit, not abnormal, so we don't call them here.
 * XPG says that ignored/blocked SIGABRT will be overridden, but we should
 * only produce a core if it was DFL (XXhow can we do this??).
 * XPG says that fcloses should be done (_cleanup) but this is only part of
 * abnormal termination which occurs only if SIGABRT isn't caught AND
 * doesn't return. The implication is that we must send them the SIGABRT
 * before cleanup().
 */

void
abort(void)
{
	struct sigaction osa;

	sigaction(SIGABRT, NULL, &osa);
	
	/* give signal to user if they are catching it */
	if (osa.sa_handler != SIG_DFL && osa.sa_handler != SIG_IGN) {
		sigrelse(SIGABRT);		/* Unblock SIGABRT. */
		raise(SIGABRT);
	}

	/* close fd's as per XPG */
	_cleanup();

	signal(SIGABRT, SIG_DFL);
	sigrelse(SIGABRT);		/* Unblock SIGABRT. */
	raise(SIGABRT);
	/**NOTREACHED**/
	_exit(EXIT_FAILURE);
}
