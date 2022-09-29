/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/sigsetops.c	1.5"

/*
 * POSIX signal manipulation functions. 
 */
#ifdef __STDC__
	#pragma weak sigfillset = _sigfillset
	#pragma weak sigemptyset = _sigemptyset
	#pragma weak sigaddset = _sigaddset
	#pragma weak sigdelset = _sigdelset
	#pragma weak sigismember = _sigismember
#endif
#include "synonyms.h"
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <sys/signal.h>
#include "sig_extern.h"

#define MAXBITNO (32)
#define sigword(n) ((n-1)/MAXBITNO)
/* bit mask position within a word, to be used with sigword() or word 0 */
#undef sigmask	/* make sure local sigmask is used */
#define sigmask(n) (1L<<((n-1)%MAXBITNO))

#define SIGSETSIZE	sizeof(sigset_t)

static sigset_t sigs = {0,};
static int sigsinit = 0;

static int
sigvalid(int sig)
{
	if (sig <= 0 || sig > (int)(CHAR_BIT * SIGSETSIZE))
		return 0;

	if (!sigsinit) {
		__sigfillset(&sigs);
		sigsinit++;
	}

	return (sigs.__sigbits[sigword(sig)] & (unsigned long)sigmask(sig)) != 0;
}

int
sigfillset(sigset_t *set)
{
	if (!sigsinit) {
		__sigfillset(&sigs);
		sigsinit++;
	}

	*set = sigs;
	return 0;
}

int
sigemptyset(sigset_t *set)
{
	set->__sigbits[0] = 0;
	set->__sigbits[1] = 0;
	set->__sigbits[2] = 0;
	set->__sigbits[3] = 0;
	return 0;
}

int 
sigaddset(sigset_t *set, register int sig)
{
	if (!sigvalid(sig)) {
		setoserror(EINVAL);
		return -1;
	}
	set->__sigbits[sigword(sig)] |= (unsigned long)sigmask(sig);
	return 0;
}

int 
sigdelset(sigset_t *set, register int sig)
{
	if (!sigvalid(sig)) {
		setoserror(EINVAL);
		return -1;
	}
	set->__sigbits[sigword(sig)] &= (unsigned long)~sigmask(sig);
	return(0);
}

int
sigismember(const sigset_t *set, register int sig)
{
	if (!sigvalid(sig)) {
		setoserror(EINVAL);
		return -1;
	}
	return (set->__sigbits[sigword(sig)] & (unsigned long)sigmask(sig)) != 0;
}
