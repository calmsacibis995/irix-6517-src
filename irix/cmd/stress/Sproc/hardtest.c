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

#ident "$Revision: 1.16 $"

#include "stdlib.h"
#include "bstring.h"
#include "unistd.h"
#include "stdio.h"
#include "getopt.h"
#include "ulocks.h"
#include "malloc.h"
#include "errno.h"
#include "signal.h"
#include "sys/wait.h"
#include "sys/prctl.h"
#include "stress.h"

/*
 * hardtest - test software/hardware locks
 */
ulock_t *thelock;
int verbose = 0;
int ppid, spid;
int Nlocks, Type;
int trace = 0;
volatile int *Index;
int nloops;
int domem = 0;
int dounrel = 0;
char *Cmd, *Argv0;
usptr_t *Header;
unsigned Nusers;
char *afile;
/* ouch! a hack to keep hardtest from printing all those nasty unlock
 * messages
 */
extern int _utracefd;

#define ONE	0
#define RELATED 1
#define UNRELATED 2
struct t {
	int locktype;
	int multi;
} tests[] = {
	{_USNODEBUG, ONE },
	{_USDEBUG, ONE },
	{_USDEBUGPLUS, ONE },
	{_USNODEBUG, UNRELATED },
	{_USDEBUG, UNRELATED },
	{_USDEBUGPLUS, UNRELATED },
	{_USNODEBUG, RELATED },
	{_USDEBUG, RELATED },
	{_USDEBUGPLUS, RELATED },
};
char *tnames[] = {
	"NODEBUG SINGLE PROCESS",
	"DEBUG SINGLE PROCESS",
	"DEBUGPLUS SINGLE PROCESS",
	"NODEBUG UNRELATED PROCESS",
	"DEBUG UNRELATED PROCESS",
	"DEBUGPLUS UNRELATED PROCESS",
	"NODEBUG SPROC PROCESS",
	"DEBUG SPROC PROCESS",
	"DEBUGPLUS SPROC PROCESS",
};
static void slave(void *);
void unrel(void);
void doall(int type, int multi);
void dolocks(int nl, int type, int pass, int multi, usptr_t *u);
void playlock(int, pid_t, int, usptr_t *);
void dumpmi(register struct mallinfo *mi);

int
main(int argc, char **argv)
{
	int i, c;
	pid_t pid;
	auto int status;

	Cmd = errinit(argv[0]);
	Argv0 = argv[0];
	nloops = 100;
	Nlocks = 16;
	while ((c = getopt(argc, argv, "A:I:T:R:mvn:tl:")) != -1)
		switch(c) {
		case 'l':	/* nlocks */
			Nlocks = atoi(optarg);
			break;
		case 'n':	/* nloops */
			nloops = atoi(optarg);
			break;
		case 'v':	/* verbose */
			verbose = 1;
			break;
		case 'm':	/* memory dump */
			domem++;
			break;
		case 'I':	/* pointer to index */
			Index = (int *)atol(optarg);
			break;
		case 'T':	/* Type for unrelated proc */
			Type = atoi(optarg);
			break;
		case 't':	/* trace */
			/*trace = 1;*/
			putenv("USTRACE=1");
			unlink("hardtest.trace");
			putenv("USTRACEFILE=hardtest.trace");
			break;
		case 'R':	/* unrelated process */
			thelock = (ulock_t *) atol(optarg);
			dounrel++;
			break;
		case 'A':
			afile = optarg;
			break;
		default:
			fprintf(stderr, "Usage:hardtest [-l nlocks][-n nloops][-t][-v][-m]\n");
			exit(1);
		}

	setlinebuf(stdout);
	setlinebuf(stderr);

	if (dounrel) {
		unrel();
		exit(0);
	}
	if (Nlocks > 100) {
		if (usconfig(CONF_INITSIZE, 256*1024) < 0) {
			errprintf(ERR_EXIT, "usconfig size failed");
			/* NOTREACHED */
		}
	}

	afile = tempnam(NULL, "hardtest");
	/*
	 * since we cannot change locktype after a usinit, we always
	 * fork to execute another test
	 */
	for (i = 0; i < sizeof(tests)/sizeof(struct t); i++) {
		printf("%s:Starting Test %s\n", Cmd, tnames[i]);
		if ((pid = fork()) < 0) {
			perror("hardtest: can't fork");
			exit(-1);
		} else if (pid == 0) {
			ppid = get_pid();
			/* child */
			prctl(PR_SETEXITSIG, SIGTERM);
			doall(tests[i].locktype, tests[i].multi);
			exit(0);
		} else {
			while (wait(&status) != pid)
				;
			if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					fprintf(stderr, "hardtest:failed status %d\n",
						WEXITSTATUS(status));
					exit(-1);
				}
			} else if (WIFSIGNALED(status) && WTERMSIG(status) != SIGTERM) {
				fprintf(stderr, "hardtest:ERROR:caught signal %d\n", WTERMSIG(status));
				exit(-1);
			}
		}
	}
	printf("hardtest:done\n");

	return 0;
}

void
doall(int type, int multi)
{
	usptr_t *u;
	struct mallinfo mi;
	int i;

	if (usconfig(CONF_LOCKTYPE, type) != 0) {
		errprintf(ERR_ERRNO_EXIT, "usconfig failed");
	}
	/* set up arena */
	if ((u = usinit(afile)) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "usinit failed");
	}
	Nusers = (int)usconfig(CONF_GETUSERS, u);
	if ((thelock = (ulock_t *)usmalloc(Nlocks * sizeof(ulock_t), u)) == NULL) {
		errprintf(ERR_EXIT, "malloc failed for thelock");
	}
	if ((Index = (int *)usmalloc(sizeof(int *), u)) == NULL) {
		errprintf(ERR_EXIT, "malloc failed for Index");
	}
	bzero(thelock, Nlocks * (int)sizeof(ulock_t));
	if (multi == UNRELATED) {
		char nNlocks[32];
		char nType[32];
		char nThelock[32];
		char nIndex[32];
		char *sargv[20];

		sprintf(nNlocks, "-l %d", Nlocks);
		sprintf(nType, "-T %d", type);
		sprintf(nThelock, "-R %ld", thelock);
		sprintf(nIndex, "-I %ld", Index);
		i = 0;
		sargv[i++] = "hardtest";
		sargv[i++] = nNlocks;
		sargv[i++] = nType;
		sargv[i++] = nThelock;
		sargv[i++] = nIndex;
		if (trace)
			sargv[i++] = "-t";
		sargv[i++] = "-A";
		sargv[i++] = afile;
		sargv[i] = NULL;
		if ((spid = fork()) < 0) {
			perror("hardtest:can't fork unrelated");
			exit(1);
		} else if (spid == 0) {
			/* child - exec hardtest */
			execvp(Argv0, sargv);
			errprintf(ERR_ERRNO_EXIT, "exec failed");
		}
	} else if (multi == RELATED) {
		/* set things up for sproc'ed process */
		Header = u;
		Type = type;
		if ((spid = sproc(slave, PR_SALL)) < 0) {
			if (errno == EAGAIN) {
				perror("hardtest:can't sproc");
				exit(1);
			} else
				errprintf(ERR_ERRNO_EXIT, "sproc");
		}
	}
	usmallopt(M_DEBUG, 1, u);
	if (domem) {
		mi = usmallinfo(u);
		dumpmi(&mi);
	}
	for (i = 0; i < nloops; i++) {
		dolocks(Nlocks, type, i, multi, u);
	}
	if (domem) {
		mi = usmallinfo(u);
		dumpmi(&mi);
	}
	unlink(afile);
	if (multi == RELATED || multi == UNRELATED) {
		kill(spid, SIGKILL);
		waitpid(spid, NULL, 0);
	}
}

void
dolocks(int nl, int type, int pass, int multi, usptr_t *u)
{
	register int i;

	if (verbose)
		fprintf(stderr, "Pass:%d\n", pass);
	for (i = 0; i < nl; i++) {
		if ((thelock[i] = usnewlock(u) ) == NULL) {
			errprintf(ERR_ERRNO_EXIT, "usnewlock failed on lock #%d type %d pass %d",
				i, type, pass);
			/* NOTREACHED */
		}
		if (trace)
			printf("LOCK # = %d, lockaddr = %x \n",i, thelock[i]);

		/* for 1st lock, have one process set, the other unset */
		if (i == 0)
			if (ussetlock(thelock[i]) != 1)
				errprintf(ERR_EXIT, "setlock #%d failed", i);

		if (multi) {
			/* let slave do playing */
			*Index = i;
			unblockproc(spid);
			blockproc(ppid);
		} else {
			/* we do the playing */
			playlock(i, ppid, type, u);
		}

		if (ustestlock(thelock[i]) != 0)
			errprintf(ERR_EXIT, "untestlock #%d failed", i);
	}		
	for (i = 0; i < nl; i++)
		if (thelock[i])
			usfreelock(thelock[i], u);
	return;
}


/*
 * play with locks
 */
void
playlock(
 register int i,	/* index of lock */
 pid_t pid,		/* who should own the lock */
 int type,		/* type of lock */
 usptr_t *u)
{
	lockmeter_t met;
	lockdebug_t deb;
	register int j;
	int ofd, err;

	ofd = _utracefd;
	if (i != 0)
		if (ussetlock(thelock[i]) != 1)
			errprintf(ERR_EXIT, "setlock #%d failed", i);
	if (ustestlock(thelock[i]) != 1)
		errprintf(ERR_EXIT, "testlock #%d failed", i);

	if (usctllock(thelock[i], CL_DEBUGFETCH, &deb) != 0) {
		if (type != _USNODEBUG)
			errprintf(ERR_EXIT, "CL_DEBUGFETCH failed lock #%d", i);
	} else {
		if (deb.ld_owner_pid != (i == 0 ? ppid : pid))
			errprintf(ERR_EXIT,
			"DEBUG pid for lock #%d is %d should be %d",
			i, deb.ld_owner_pid, i == 0 ? ppid : pid);
	}
	_utracefd = 99;
	if (usunsetlock(thelock[i]) < 0)
		errprintf(ERR_EXIT, "unsetlock #%d failed", i);
	_utracefd = ofd;

	if (ustestlock(thelock[i]) != 0)
		errprintf(ERR_EXIT, "testlock #%d failed", i);

	/* conditionally set lock */
	if ((err = uscsetlock(thelock[i], 1)) != 1)
		errprintf(ERR_EXIT, "csetlock #%d failed rv %d",
			i, err);
	if ((err = uscsetlock(thelock[i], 1)) != 0)
		errprintf(ERR_EXIT, "csetlock2 #%d failed rv %d",
			i, err);
	if (usunsetlock(thelock[i]) != 0)
		errprintf(ERR_EXIT, "un[c]setlock #%d failed", i);

	if (i == Nlocks - 1) {
		_utracefd = 99;
		/* test unlocking a unlocked lock only on last lock */
		if (usunsetlock(thelock[i]) < 0)
			errprintf(ERR_EXIT, "2nd unsetlock #%d failed", i);
		_utracefd = ofd;
	}

	if (usctllock(thelock[i], CL_METERFETCH, &met) != 0) {
		if (type != _USNODEBUG)
			errprintf(ERR_EXIT, "CL_METERFETCH failed lock #%d", i);
	} else {
		if (met.lm_tries != 3 || met.lm_hits != 2)
			errprintf(ERR_EXIT, "METER for lock #%d tries %d hits %d",
				i, met.lm_tries, met.lm_hits);
	}
	if (usctllock(thelock[i], CL_DEBUGFETCH, &deb) != 0) {
		if (type != _USNODEBUG)
			errprintf(ERR_EXIT, "CL_DEBUGFETCH failed lock #%d", i);
	} else {
		if (deb.ld_owner_pid != -1)
			errprintf(ERR_EXIT,
				"DEBUG pid for lock #%d is %d should be -1",
				i, deb.ld_owner_pid);
	}

	/* run locks through paces */
	for (j = 0; j < 2 * Nusers ; j++) {
		ussetlock(thelock[i]);
		usunsetlock(thelock[i]);
	}
	usfreelock(thelock[i], u);

	if ((thelock[i] = usnewlock(u)) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "usnewlock realloc failed on lock #%d", i);
		return;
	}
	if (trace)
		printf("REALLOC  # = %d, lockaddr = %x \n",i, thelock[i]);
}

/* ARGSUSED */
static void
slave(void *a)
{
	int mypid = get_pid();

	if (usinit(afile) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "slave usinit failed");
	}
	for (;;) {
		blockproc(mypid);
		playlock(*Index, mypid, Type, Header);
		unblockproc(ppid);
	}
}

void
dumpmi(register struct mallinfo *mi)
{
	size_t size;

	printf("Mallinfo:arena:%d ordblks:%d smblks:%d hblkhd:%d\n",
		mi->arena, mi->ordblks, mi->smblks, mi->hblkhd);
	printf("\thblks:%d usmblks:%d fsmblks:%d uordblks:%d\n",
		mi->hblks, mi->usmblks, mi->fsmblks, mi->uordblks);
	printf("\tfordblks:%d keepcost:%d\n",
		mi->fordblks, mi->keepcost);
	size = mi->hblkhd + mi->usmblks + mi->fsmblks + mi->uordblks +
		mi->fordblks;
	if (mi->arena != size)
		errprintf(0, "\t size %d != arena %d\n", size, mi->arena);
}

void
unrel(void)
{
	ppid = getppid();
	/* set up arena */
	if ((Header = usinit(afile)) == NULL) {
		errprintf(ERR_ERRNO_EXIT, "usinit failed");
	}
	slave(0);
	exit(-1);
}
