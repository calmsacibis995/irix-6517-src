#include "par.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysmp.h>
#include <sys.s>


pid_t tpid;
int mustrundone = 0;
int mustruncpu;
int nswitchin;
int initialsleep = 5; /* seconds */
int mintestperiod = 10000; /* milliseconds */

void
test_prologue(char* funs, int* Qflag)
{
	strcat(funs, " -rs");
	if (*Qflag < 1)
		(*Qflag)++;
}

/* ARGSUSED */
void
test_init(int totalms, char *arg)
{
	int ncpus;

	printf("\t\n\nTest to check restriction & mustrun of a spinner process.\n\n");
	if (totalms < (initialsleep * 1000) + mintestperiod) {
		printf("ERROR: Test period too short. Must be atleast 15 seconds.\n");
		exit(-1);
	}
	ncpus = (int) sysmp(MP_NPROCS);
	mustruncpu = (int) random() % ncpus;
	if (sysmp(MP_RESTRICT, mustruncpu) == -1) {
		perror("MP_RESTRICT");
		exit(-1);
	}
	switch (tpid = fork()) {
	case -1:
		perror("Fork");
		exit(-1);
	case 0:
		/* child */
		sleep(initialsleep);
		if (sysmp(MP_MUSTRUN, mustruncpu) == -1) {
			perror("sysmp:MUSTRUN");
			exit(-1);
		}
		printf("Mustrun onto CPU %d\n", mustruncpu);
		for ( ; ; );
		/* NOT REACHED */
	default:
		/* parent */
		printf("Spinner created, pid = %d\n", tpid);
		/* fall through */
	}
	nswitchin = 0;
}

/* ARGSUSED */
void
test_switchin(struct procrec *pp, struct cpurec *crp, ptime_t ms)
{
	if (pp->lastrun != mustruncpu)
		return;
	if (!mustrundone || pp->pid != tpid) {
		printf("ERROR: cpu %d is restricted, but switched in unknown process %d (mustrundone %d tpid %d)\n", mustruncpu, pp->pid, mustrundone, tpid);
		return;
	}
	if (pp->pid == tpid)
		nswitchin++;
}

void
test_syscall(pid_t pid, struct cl *clp)
{
	extern void printcall(struct cl *crecp, int flag);
	if (pid != tpid)
		return;
	printcall(clp, 0);
	if (clp->call ==  (SYS_sysmp - SYSVoffset))
		mustrundone++;
}

void
test_end()
{
	printf("Process %d switched %d times into CPU %d\n", tpid, nswitchin, mustruncpu);
	kill(tpid, SIGKILL);
	printf("Process %d terminated.\n", tpid);
	if (sysmp(MP_EMPOWER, mustruncpu) == -1) {
		perror("MP_RESTRICT");
		exit(-1);
	}
}
