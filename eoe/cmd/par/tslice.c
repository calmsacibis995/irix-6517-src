#include "par.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


pid_t tpid;
double nswitchin;
double totalms;

void
test_prologue(char* funs, int* Qflag)
{
	strcat(funs, " -r");
	if (*Qflag < 1)
		(*Qflag)++;
}

/* ARGSUSED */
void
test_init(int period, char *arg)
{
	printf("\t\n\nTest to check time-slice and number of context switches of a spinner process.\n\n");
	switch (tpid = fork()) {
	case -1:
		perror("Fork");
		exit(-1);
	case 0:
		/* child */
		for ( ; ; ) ;  /* spinner */
		/* NOT REACHED */
	default:
		/* parent */
		printf("Spinner created, pid = %d\n", tpid);
		/* fall through */
	}
	nswitchin = (double) 0.0;
	totalms = (double) period;
	printf("Total Duration of Test = %3.2f milliseconds.\n\n", totalms);
}

/* ARGSUSED */
void
test_switchin(struct procrec *pp, struct cpurec *crp, ptime_t ms)
{
	if (pp->pid == tpid)
		nswitchin++;
}

void
test_end()
{
	struct procrec *prec;
	double perslice;
	double eperslice = (double) 100.0; /* expected milliseconds per slice */
	double eswitches; /* expected # of switches */
	double switchdiff, slicediff;

	prec = procfind(tpid);
	if (prec == NULL) {
		printf("ERROR: couldn't find procrec for pid %d\n", tpid);
		kill(tpid, SIGKILL);
		exit(-1);
	}
	perslice = (double) TOMS(prec->runtime)/ nswitchin;
	eswitches =  totalms / eperslice;
	switchdiff = nswitchin - eswitches;
	slicediff = perslice - eperslice;
	switchdiff = switchdiff * (double) 100.0 / eswitches;
	slicediff = slicediff * (double) 100.0 / eperslice;

	printf("Switches: Actual = %3.2f, Expected = %3.2f, Diff = %3.2f%% %s\n",
		nswitchin, eswitches, switchdiff,
		(switchdiff > 10.0 || switchdiff < -10.0) ? "<< BAD >>" : "");
	printf("Time-Slice: Actual = %3.2f, Expected = %3.2f, Diff = %3.2f%% %s\n",
		perslice, eperslice, slicediff,
		(slicediff > 10.0 || slicediff < -10.0) ? "<< BAD >>" : "");
	printf("Process %d terminated.\n", tpid);
}
