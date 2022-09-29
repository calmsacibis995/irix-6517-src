#include "par.h"
#include <signal.h>
#include <stdio.h>
#include <sys/sysmp.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <ulocks.h>

#define MAXPROCS 32

int totalms = 0;
int nprocs;
int mustruncpu;
int initialsleep = 5; /* seconds */
int mintestperiod = 10000; /* milliseconds */
int ncpus;
int mustrundone = 0;

pid_t pid[MAXPROCS];

void
test_prologue(char* funs, int* Qflag)
{
	strcat(funs, " -rs");
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
}

void
spinner(void)
{
	sleep(initialsleep);
	if (sysmp(MP_MUSTRUN, mustruncpu) == -1) {
		perror("sysmp:MUSTRUN");
		exit(-1);
	}
	/* printf("Mustrun onto CPU %d\n", mustruncpu); */
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
	printf("\t\n\nTest to check fair cpu allocation among %sed mustrun processes.\n\n", mode);
	if (strcmp(mode, "sproc") == 0 && nprocs > 7) {
		if (usconfig(CONF_INITUSERS, nprocs+1) == -1) {
			perror("usconfig");
			exit(-1);
		}
	}
	totalms = period;
	if (totalms < (initialsleep * 1000) + mintestperiod) {
		fprintf(stderr,
		    "ERROR: Test period too short. Must be atleast %d seconds.\n",
		    initialsleep + (mintestperiod / 1000));
		exit(-1);
	}
	ncpus = (int) sysmp(MP_NPROCS);
	mustruncpu = (int) random() % ncpus;
	printf("Creating Spinners, mustrun = %d, pids = {", mustruncpu);
	for (i = 0; i < nprocs; i++) {
		if (strcmp(mode, "fork") == 0)
			fork_and_spin(i);
		else
			sproc_and_spin(i);
		printf(" %d", pid[i]);
	}
	printf(" }\n");
}

void
test_syscall(int tpid, struct cl *clp)
{
	int i;
	struct procrec *prec[MAXPROCS];

	for (i = 0; i < nprocs; i++)
		if (pid[i] == tpid)
			break;
	if (i == nprocs) /* some process not belonging to the test */
		return;
	/* printf("Syscall, pid = %d, call = %d\n", tpid, clp->call); */
	if (clp->call ==  (SYS_sysmp - SYSVoffset))
		mustrundone++;
	if (mustrundone == nprocs) {
		for (i = 0; i < nprocs; i++) {
			prec[i] = procfind(pid[i]);
			if (prec[i] == NULL) {
				printf("ERROR: couldn't find procrec for pid %d\n",
					pid[i]);
				finish(nprocs);
				exit(-1);
			}
			prec[i]->runtime = 0; /* time starts now */
		}
		totalms -= initialsleep * 1000;
		printf("Started timing after mustrun of all processes\n");
	}
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
	avgentitlement = totalms / nprocs;
	printf("NCPUS = %d, Total time = %d mS, nprocs = %d, average entitlement = %d mS\n", ncpus, totalms, nprocs, avgentitlement);
	printf("Allowed difference = 10%%.\n");
	for (i = 0; i < nprocs; i++) {
		diff = (double) (TOMS(prec[i]->runtime) - avgentitlement);
		diff = diff * (double) 100.0 / (double) avgentitlement;
		printf("pid[%d] = %d, runtime = %llu mS, diff = %3.2f%% \t%s\n",
			i, pid[i], TOMS(prec[i]->runtime), diff,
			(diff > 10.0 || diff < -10.0) ? "<< BAD >>" : "");
	}
	finish(nprocs);
}
