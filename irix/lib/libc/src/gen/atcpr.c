/*	Copyright (c) 1996 Silicon Graphics, Inc.		*/
/*	  All Rights Reserved  					*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	Silicon Graphics, Inc.                     		*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/atcpr.c	1.0"
/*LINTLIBRARY*/

#include "synonyms.h"
#include "mplib.h"
#include "gen_extern.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>

/*
 * Rountines that help to get rid of checkpoint-unsafe objects such as sockets
 * The interfaces are mp-safe.
 */
#ifdef __STDC__
	#pragma weak atcheckpoint = _atcheckpoint
	#pragma weak atrestart = _atrestart
#endif

#define MAXFNS	32

struct atcpr {
	pid_t		at_pid;
	struct atcpr	*at_nextp;
	int		at_num;
	void		(*at_func[MAXFNS])(void);
};

static void _ckpthandle(int, siginfo_t *, void *);
static void _resthandle(int, siginfo_t *, void *);
static int atcpr(void (*)(), struct atcpr **);

static struct atcpr *ckptlist = 0, *restlist = 0;

int
atcheckpoint(void (*func)())
{
	return (atcpr(func, &ckptlist));
}

int
atrestart(void (*func)())
{
	return (atcpr(func, &restlist));
}

static int
atcpr(void (*func)(), struct atcpr **list)
{
	struct atcpr *cp, *prevp = NULL;
	pid_t mpid = getpid();
	sigaction_t act;
	int sig;
	LOCKDECLINIT(l, LOCKMISC);

	for (cp = *list; cp; prevp = cp, cp = cp->at_nextp) {
		if (cp->at_pid == mpid) {
#ifdef ATCPRDEBUG
			if (list == &ckptlist)
				printf("%d: atchekpoint callback (num %d)\n",
					mpid, cp->at_num);
			else
				printf("%d: atrestart callback (num %d)\n",
					mpid, cp->at_num);
#endif
			if (cp->at_num >= MAXFNS) {
				errno = ENOMEM;
				perror("atcheckpoint/atrestart out of buffers");
				UNLOCKMISC(l);
				return (-1);
			}
			cp->at_func[cp->at_num++] = func;
			UNLOCKMISC(l);
			return(0);
		}
	}
	/*
	 * This is a new one
	 */
	if ((cp = malloc(sizeof(struct atcpr))) == NULL) {
		UNLOCKMISC(l);
		perror("atcheckpoint/atrestart failed to malloc");
		return(-1);
	}
#ifdef ATCPRDEBUG
	if (list == &ckptlist)
		printf("PID %d: add atchekpoint callback (num 0)\n", mpid);
	else
		printf("PID %d: add atrestart callback (num 0)\n", mpid);
#endif
	cp->at_pid = mpid;
	cp->at_num = 0;
	cp->at_nextp = NULL;
	cp->at_func[cp->at_num++] = func;

	act.sa_flags = SA_SIGINFO|SA_RESTART;
	act.sa_handler = NULL;
	sigemptyset(&act.sa_mask);
	if (list == &ckptlist) {
		sig = SIGCKPT;
		act.sa_sigaction = _ckpthandle;	
	}
	else {
		sig = SIGRESTART;
		act.sa_sigaction = _resthandle;	
	}
	if (sigaction(sig, &act, NULL) < 0) {
		perror("atcheckpoint/atrestart fails on sigaction");
		UNLOCKMISC(l);
		return (-1);
	}
	if (prevp != NULL)
		prevp->at_nextp = cp;
	else
		*list = cp;

	UNLOCKMISC(l);
	return(0);
}

static void
__cprhandle(struct atcpr **list)
{
	struct atcpr *cp;
	pid_t mpid = getpid();
	LOCKDECLINIT(l, LOCKMISC);

	for (cp = *list; cp; cp = cp->at_nextp) {
		if (cp->at_pid == mpid) {
			int i;
#ifdef ATCPRDEBUG
			printf("%d: found %d atcpr callbacks\n", mpid, cp->at_num);
#endif
			UNLOCKMISC(l);
			for (i = 0; i < cp->at_num; i++)
				(*cp->at_func[i])();
			return;
		}
	}
	UNLOCKMISC(l);
}

/* ARGSUSED */
static void
_ckpthandle(int sig, siginfo_t *sip, void *p)
{
	assert(sig == SIGCKPT);
	__cprhandle(&ckptlist);
}

/* ARGSUSED */
static void
_resthandle(int sig, siginfo_t *sip, void *p)
{
	assert(sig == SIGRESTART);
	__cprhandle(&restlist);
}
