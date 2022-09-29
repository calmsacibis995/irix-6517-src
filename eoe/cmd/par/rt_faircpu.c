#include "par.h"
#include <signal.h>
#include <stdio.h>
#include <sys/sysmp.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <ulocks.h>
#include <sched.h>

#define MAXPROCS 32

int totalms = 0;
int nprocs, ncpus;

pid_t pid[MAXPROCS];

void
test_prologue(char* funs, int* Qflag)
{
	struct sched_param prio;

	ncpus = (int) sysmp(MP_NPROCS);
	if (ncpus  == 1) { 
		printf("ERROR: Test will only work on multiprocessors.\n");
		exit(-1);
	}
	/* restrict last processor to run only par/padc */ 
	if (sysmp(MP_RESTRICT, ncpus-1) == -1) {
		perror("MP_RESTRICT");
		exit(-1);
	}
	if (sysmp(MP_MUSTRUN, ncpus-1) == -1) {
		perror("sysmp:MUSTRUN");
		exit(-1);
	}
	printf("Restricted & Mustrun onto CPU %d\n", ncpus-1);
	/* Increase priority, so test processes will
	 * inherit priority automatically
	 */
	 /* Choose priority such that it is low enough to avoid 
	  * clashing with system activities like timeouts
	  */
	prio.sched_priority = 2;
	if (sched_setscheduler(0, SCHED_RR, &prio) == -1) {
		perror("sched_setscheduler");
		exit(-1);
	}

	strcat(funs, " -r");
	if (*Qflag < 1)
		(*Qflag)++;
}

void
finish(int i)
{
	printf("Killing Spinners, pids = {");
	for (--i; i >= 0; i--) {
		kill(pid[i], SIGKILL);
		printf(" %d", pid[i]);
	}
	printf(" }\n");
	if (sysmp(MP_EMPOWER, ncpus-1) == -1) {
		perror("MP_RESTRICT");
		exit(-1);
	}
}

void
spinner(void)
{
	if (sysmp(MP_RUNANYWHERE) == -1) {
		perror("sysmp:MUSTRUN");
		exit(-1);
	}
	for ( ; ; );
}

void
sproc_and_spin(int i)
{
	pid[i] = sproc((void (*) (void *)) spinner, PR_SALL);
	if (pid[i] == -1) {
		perror("sproc");
		finish(i);
		exit(-1);
	}
}

void
fork_and_spin(int i)
{
	switch (pid[i] = fork()) {
	case -1:
		perror("Fork");
		finish(i);
		exit(-1);
	case 0:
		/* child */
		spinner();
		/* NOT REACHED */
	}
}

#define USAGE "Usage: -T <mode,#processes>, mode = sproc/fork"

/* ARGSUSED */
void
test_init(int period, char *s)
{
	int i;
	char *mode, *np;

	if (s == NULL) {
		printf("ERROR: %s\n", USAGE);
		exit(-1);
	}
	mode = s;
	np = strchr(s, ',');
	if (np == NULL) {
		printf("ERROR: %s\n", USAGE);
		exit(-1);
	}
	*np++ = NULL; /* terminate mode, skip comma */
	if (strcmp(mode, "fork") != 0 && strcmp(mode, "sproc") != 0) {
		printf("ERROR: %s\n", USAGE);
		exit(-1);
	}
	nprocs = atoi(np);
	if (nprocs <= 0 || nprocs > 32) {
		printf("ERROR: #Processes not in range 1..%d (given %d)\n",
			MAXPROCS, nprocs);
		exit(-1);
	}
	printf("\t\n\nTest to check fair cpu allocation among %sed processes.\n\n", mode);
	if (strcmp(mode, "sproc") == 0 && nprocs > 7) {
		if (usconfig(CONF_INITUSERS, nprocs+1) == -1) {
			perror("usconfig");
			exit(-1);
		}
	}
	printf("Creating Spinners, pids = {");
	for (i = 0; i < nprocs; i++) {
		if (strcmp(mode, "fork") == 0)
			fork_and_spin(i);
		else
			sproc_and_spin(i);
		printf(" %d", pid[i]);
	}
	printf(" }\n");
	totalms = period;
}

void
test_end()
{
	struct procrec *prec[MAXPROCS];
	int i, avgentitlement;
	double diff;
	
	for (i = 0; i < nprocs; i++) {
		prec[i] = procfind(pid[i]);
		if (prec[i] == NULL) {
			printf("ERROR: couldn't find procrec for pid %d\n",
				pid[i]);
			finish(nprocs);
			exit(-1);
		}
	}
	if (nprocs > (ncpus-1))
		avgentitlement = ((ncpus-1) * totalms) / nprocs;
	else
		avgentitlement = totalms;

	printf("NCPUS = %d, Total time = %d mS, nprocs = %d, average entitlement = %d mS\n", (ncpus-1), totalms, nprocs, avgentitlement);
	printf("Allowed difference = 10%%.\n");
	for (i = 0; i < nprocs; i++) {
		diff = (double) (TOMS(prec[i]->runtime) - avgentitlement);
		diff = diff * (double) 100.0 / (double) avgentitlement;
		printf("pid[%d] = %d, runtime = %d mS, diff = %3.2f%% \t%s\n",
			i, pid[i], TOMS(prec[i]->runtime), diff,
			(diff > 10.0 || diff < -10.0) ? "<< BAD >>" : "");
	}
	finish(nprocs);
}
