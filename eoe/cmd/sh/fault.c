/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)sh:fault.c	1.13.27.1"
/*
 * UNIX shell
 */
#include	<pfmt.h>
#include	<unistd.h>
#include	"defs.h"
#include	<sys/procset.h>

void done();
void stdsigs();
void oldsigs();
void chktrap();

extern int str2sig();

static void fault();
static BOOL sleeping = 0;
static unsigned char *trapcom[MAXTRAP];
/*static*/ BOOL trapflg[MAXTRAP] =
{
	0,
	0,	/* 1:  hangup */
	0,	/* 2:  interrupt */
	0,	/* 3:  quit */
	0,	/* 4:  illegal instr */
	0,	/* 5:  trace trap */
	0,	/* 6:  IOT */
	0,	/* 7:  EMT */
	0,	/* 8:  float pt. exp */
	0,	/* 9:  kill */
	0, 	/* 10: bus error */
	0,	/* 11: memory faults */
	0,	/* 12: bad sys call */
	0,	/* 13: bad pipe call */
	0,	/* 14: alarm */
	0, 	/* 15: software termination */
	0,	/* 16: unassigned */
	0,	/* 17: unassigned */
	0,	/* 18: death of child */
	0,	/* 19: power fail */
	0,	/* 20: window size changes */
	0,	/* 21: urgent condition on IO channel */
	0,	/* 22: pollable event occurred or i/o possible signal */
	0,	/* 23: sendable stop signal not from tty */
	0,	/* 24: stop signal from tty */
	0,	/* 25: continue a stopped process */
	0,	/* 26: to readers pgrp upon background tty read */
	0,	/* 27: like TTIN for output if (tp->t_local&LTOSTOP) */
	0,	/* 28: virtual time alarm */
	0,	/* 29: profiling alarm */
	0,	/* 30: Cpu time limit exceeded */
	0,	/* 31: Filesize limit exceeded */
	0,	/* 32: not used in SVR4; leave for SIGPOLL or SIGIO */
	0,      /* User defined signal 33 */
	0,      /* User defined signal 34 */
	0,      /* User defined signal 35 */
	0,      /* User defined signal 36 */
	0,      /* User defined signal 37 */
	0,      /* User defined signal 38 */
	0,      /* User defined signal 39 */
	0,      /* User defined signal 40 */
	0,      /* User defined signal 41 */
	0,      /* User defined signal 42 */
	0,      /* User defined signal 43 */
	0,      /* User defined signal 44 */
	0,      /* User defined signal 45 */
	0,      /* User defined signal 46 */
	0,      /* User defined signal 47 */
	0,      /* User defined signal 48 */
	0,      /* Real-time signal RTMIN */
	0,      /* Real-time signal RT2 */
	0,      /* Real-time signal RT3 */
	0,      /* Real-time signal RT4 */
	0,      /* Real-time signal RT5 */
	0,      /* Real-time signal RT6 */
	0,      /* Real-time signal RT7 */
	0,      /* Real-time signal RT8 */
	0,      /* Real-time signal RT9 */
	0,      /* Real-time signal RT10 */
	0,      /* Real-time signal RT11 */
	0,      /* Real-time signal RT12 */
	0,      /* Real-time signal RT13 */
	0,      /* Real-time signal RT14 */
	0,      /* Real-time signal RT15 */
	0       /* Real-time signal RTMAX */
};

static void (* const(
sigval[]))() = 
{
	0,
	done, 	/* 1:  hangup */
	fault,	/* 2:  interrupt */
	fault,	/* 3:  quit */
	done,	/* 4:  illegal instr */
	done,	/* 5:  trace trap */
	done,	/* 6:  IOT */
	done,	/* 7:  EMT */
	done,	/* 8:  floating pt. exp */
	0,	/* 9:  kill */
	done, 	/* 10: bus error */
	fault,	/* 11: memory faults */
	done, 	/* 12: bad sys call */
	done,	/* 13: bad pipe call */
	done,	/* 14: alarm */
	fault,	/* 15: software termination */
	done,	/* 16: unassigned */
	done,	/* 17: unassigned */
	0,	/* 18: death of child */
	done,	/* 19: power fail */
	0,	/* 20: window size changes */
	done,	/* 21: urgent condition on IO channel */
	done,	/* 22: pollable event occurred or i/o possible signal */
	0,	/* 23: sendable stop signal not from tty */
	0,	/* 24: stop signal from tty */
	0,	/* 25: continue a stopped process */
	0,	/* 26: to readers pgrp upon background tty read */
	0,	/* 27: like TTIN for output if (tp->t_local&LTOSTOP) */
	done,	/* 28: virtual time alarm */
	done,	/* 29: profiling alarm */
	done,	/* 30: Cpu time limit exceeded */
	done,	/* 31: Filesize limit exceeded */
	0,	/* 32: not used in SVR4; needed for SIGPOLL/SIGIO in IRIX4 */
	0,   /* User defined signal 33 */
	0,   /* User defined signal 34 */
	0,   /* User defined signal 35 */
	0,   /* User defined signal 36 */
	0,   /* User defined signal 37 */
	0,   /* User defined signal 38 */
	0,   /* User defined signal 39 */
	0,   /* User defined signal 40 */
	0,   /* User defined signal 41 */
	0,   /* User defined signal 42 */
	0,   /* User defined signal 43 */
	0,   /* User defined signal 44 */
	0,   /* User defined signal 45 */
	0,   /* User defined signal 46 */
	0,   /* User defined signal 47 */
	0,   /* User defined signal 48 */
	0,   /* Real-time signal RTMIN */
	0,   /* Real-time signal RT2 */
	0,   /* Real-time signal RT3 */
	0,   /* Real-time signal RT4 */
	0,   /* Real-time signal RT5 */
	0,   /* Real-time signal RT6 */
	0,   /* Real-time signal RT7 */
	0,   /* Real-time signal RT8 */
	0,   /* Real-time signal RT9 */
	0,   /* Real-time signal RT10 */
	0,   /* Real-time signal RT11 */
	0,   /* Real-time signal RT12 */
	0,   /* Real-time signal RT13 */
	0,   /* Real-time signal RT14 */
	0,   /* Real-time signal RT15 */
	0    /* Real-time signal RTMAX */
};

static int
ignoring(i)
register int i;
{
	struct sigaction act;
	if (trapflg[i] & SIGIGN)
		return 1;
	/*
	** O isn't a real signal
	*/
	if(i == 0)	{
		return 0;
	}
	(void)sigaction(i, 0, &act);
	if (act.sa_handler == SIG_IGN) {
		trapflg[i] |= SIGIGN;
		return 1;
	}
	return 0;
}

static void
clrsig(i)
int	i;
{
	if (trapcom[i] != 0) {
		free(trapcom[i]);
		trapcom[i] = 0;
	}
	if (trapflg[i] & SIGMOD) {
		trapflg[i] &= ~SIGMOD;
		handle(i, sigval[i]);
	}
}

void
done(sig)
{
	register unsigned char	*t = trapcom[0];

	if (t)
	{
		trapcom[0] = 0;
		execexp(t, 0);
		free(t);
	}
	else
		chktrap();

	rmtemp(0);
	rmfunctmp();

#ifdef ACCT
	doacct();
#endif
	(void)endjobs(0);
	if (sig) {
		sigset_t set;
		(void)sigemptyset(&set);
		(void)sigaddset(&set, sig);
		(void)sigprocmask(SIG_UNBLOCK, &set, 0);
		handle(sig, SIG_DFL);
		kill(mypid, sig);
	}
	exit(exitval);
}

static void 
fault(sig, code, scp)
register int	sig;
struct sigcontext *scp;
{
	register int flag;
	
	switch (sig) {
		case SIGSEGV:
			if (scp->sc_badvaddr == 0)
				exitsh(ERROR);
			if (setbrk(brkincr) == (unsigned char *)-1)
				error(0, nospace, nospaceid);
			return;
		case SIGALRM:
			if (sleeping)
				return;
			break;
	}

	if (trapcom[sig])
		flag = TRAPSET;
	else if (flags & subsh)
		done(sig);
	else
		flag = SIGSET;

	/* ignore SIGTERM, SIGQUIT, and SIGINT when "-i" option is used. */
	if ( (flags&intflg)!=intflg || (sig!=SIGTERM && sig!=SIGQUIT && sig!=SIGINT))
		trapnote |= flag;
	trapflg[sig] |= flag;
}

int
handle(sig, func)
	int sig; 
	void (*func)();
{
	struct sigaction act, oact;
	
	if (func == SIG_IGN) {
		if (trapflg[sig] & SIGIGN) {
			return 0;
		} else {
			trapflg[sig] |= SIGIGN;
		}
	} else {
		trapflg[sig] &= ~SIGIGN;
	}
	
	if (sig != 0) {
		(void)sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = func;
		(void)sigaction(sig, &act, &oact);
		return (func != oact.sa_handler);
	} else {
		return 1;
	}
}

#ifdef sgi
/*
 * unwind so that we can do only 1 sigaction call per signal
 * this is called on each shell startup.
 */
void
stdsigs()
{
	register int	i;
	sigset_t set, oset;
	sigaction_t act, oact;

	/*
	 * block all signals so we can change to catch them - but
	 * if they were ignored we can change them back without
	 * getting a stray signal
	 */
	(void)sigfillset(&set);
	sigprocmask(SIG_SETMASK, &set, &oset);

	(void)sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	for (i = 1; i < MAXTRAP; i++) {
		if (sigval[i] == 0)
			continue;
		act.sa_handler = sigval[i];
		(void)sigaction(i, &act, &oact);
		if (i != SIGSEGV && oact.sa_handler == SIG_IGN) {
			act.sa_handler = SIG_IGN;
			(void)sigaction(i, &act, NULL);
			trapflg[i] |= SIGIGN;
		}
	}
	sigprocmask(SIG_SETMASK, &oset, NULL);
}
#else
void
stdsigs()
{
	register int	i;

	for (i = 1; i < MAXTRAP; i++) {
		if (sigval[i] == 0)
			continue;
		if (i != SIGSEGV && ignoring(i))
			continue;
		handle(i, sigval[i]);
	}
}
#endif

void
oldsigs()
{
	register int	i;
	register unsigned char	*t;

	i = MAXTRAP;
	while (i--)
	{
		t = trapcom[i];
		if (t == 0 || *t)
			clrsig(i);
		trapflg[i] = 0;
	}
	trapnote = 0;
}

/*
 * check for traps
 */

void
chktrap()
{
	register int	i = MAXTRAP;
	register unsigned char	*t;

	trapnote &= ~TRAPSET;
	while (--i)
	{
		if (trapflg[i] & TRAPSET)
		{
			trapflg[i] &= ~TRAPSET;
			t = trapcom[i];
			if (t)
			{
				int	savxit = exitval;
				execexp(t, 0);
				exitval = savxit;
				exitset();
			}
		}
	}
}

void
systrap(argc,argv)
int argc;
char **argv;
{
	int sig;

	if (argc == 1) {
		for (sig = 0; sig < MAXTRAP; sig++) {
			if (trapcom[sig]) {
				prn_buff(sig);
				prs_buff(gettxt(colonid, colon));
				prs_buff(trapcom[sig]);
				prc_buff(NL);
			}
		}
	} else {
		char *a1 = *(argv+1);
		BOOL noa1;
		noa1 = (str2sig(a1,&sig) == 0);
		if (noa1 == 0)
			++argv;
		while (*++argv) {
			if (str2sig(*argv,&sig) < 0 ||
			  sig >= MAXTRAP || sig < MINTRAP || 
			  sig == SIGSEGV || sig == SIGCHLD)
				error_fail(SYSTRAP, badtrap, badtrapid);
			else if (noa1)
				clrsig(sig);
                        else if (*a1) {
				if (trapflg[sig] & SIGMOD || !ignoring(sig)) {
					handle(sig, fault);
					trapflg[sig] |= SIGMOD;
					replace(&trapcom[sig], a1);
				}
			} else if (handle(sig, SIG_IGN)) {
				trapflg[sig] |= SIGMOD;
				replace(&trapcom[sig], a1);
			}
		}
	}
}

void
mysleep(ticks)
int ticks;
{
	sigset_t set, oset;
	struct sigaction act, oact;


	/*
	 * add SIGALRM to mask
	 */

	(void)sigemptyset(&set);
	(void)sigaddset(&set, SIGALRM);
	(void)sigprocmask(SIG_BLOCK, &set, &oset);

	/*
	 * catch SIGALRM
	 */

	(void)sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = fault;
	(void)sigaction(SIGALRM, &act, &oact);

	/*
	 * start alarm and wait for signal
	 */

	(void)alarm(ticks);
	sleeping = 1;
	(void)sigsuspend(&oset);
	sleeping = 0;

	/*
	 * reset alarm, catcher and mask
	 */

	(void)alarm(0); 
	(void)sigaction(SIGALRM, &oact, NULL);
	(void)sigprocmask(SIG_SETMASK, &oset, 0);
}
