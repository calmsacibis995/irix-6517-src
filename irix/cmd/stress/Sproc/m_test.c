/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.5 $ $Author: jwag $"

#include "stdlib.h"
#include "unistd.h"
#include "stdio.h"
#include "task.h"
#include "stress.h"
#include "signal.h"

char *Cmd;
extern void slave(), slave2();
int mids[10];
int mpids[10];

/*
 * m_test - simply check main m_* routines
 */

int
main(int argc, char **argv)
{
	int nthreads = 6;
	int rv, i, j;
	pid_t pid, tid;
	pid_t me;

	Cmd = errinit(argv[0]);
	me = get_pid();
	m_set_procs(4);
	if (m_fork(slave) < 0) {
		fprintf(stderr, "m_test:m_fork failed\n");
		exit(-1);
	}
	m_kill_procs();

	printf("m_test:finished basic m_fork test\n");

	m_set_procs(nthreads);
	if (m_fork(slave2) < 0) {
		fprintf(stderr, "m_test:m_fork of %d threads  failed\n", nthreads);
		exit(-1);
	}
	for (i = 0; i < nthreads; i++) {
		tid = mids[i];
		pid = mpids[i];
		for (j = 0; j < nthreads; j++) {
			if (i != j) {
				if (mids[j] >= nthreads || mids[j] == tid)
					errprintf(0, "bad tid %d", mids[j]);
					/* NOTREACHED */
				if (mpids[j] == pid)
					errprintf(0, "bad pid %d", mpids[j]);
					/* NOTREACHED */
			}
		}
	}

	/* now park them */
	m_park_procs();
	for (i = 0; i < nthreads; i++) {
		if (mpids[i] == me)
			continue;
		rv = (int)prctl(PR_ISBLOCKED, mpids[i]);
		if (rv < 0)
			errprintf(ERR_ERRNO_EXIT, "ISBLOCK failed for pid %d",
					mpids[i]);
		else if (rv == 0)
			errprintf(ERR_EXIT, "pid %d still active", mpids[i]);
	}

	m_rele_procs();
	m_kill_procs();
	/* make sure none exist */
	for (i = 0; i < nthreads; i++) {
		if (mpids[i] == me)
			continue;
		if (kill(mpids[i], 0) >= 0) {
			errprintf(0, "pid %d still alive", mpids[i]);
			/* NOTREACHED */
		}
	}
	return 0;
}

void
slave()
{
	m_lock();
	printf("m_test:slave %d here\n", getpid());
	m_unlock();
}

void
slave2()
{
	int i;
	/* have each slave enter their id */
	m_lock();
	mids[i = m_next()] = m_get_myid();
	mpids[i] = getpid();
	printf("m_test:pid %d id %d\n", mpids[i], mids[i]);
	m_unlock();
}
