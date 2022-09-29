#include <stdio.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <signal.h>
#define SLEEP
#define SLEEP1 20
#define NUMREPS 9

extern int atcheckpoint(void (*)()), atrestart(void (*)());
int nprocs;
FILE * temp;

void *
stkaddr(void)
{
	int stkparam;
	return ((void *)&stkparam);
}

void ckpt0()
{
	printf("\tsp_simple: PARENT: Catching SIGCKPT (PID %d)\n", getpid());
	fprintf(temp, "i");
}

void ckpt1()
{
	printf("\tsp_simple: 1: Catching SIGCKPT (PID %d)\n", getpid());
	fprintf(temp, "i");
}

void ckpt2()
{
	printf("\tsp_simple: 2: Catching SIGCKPT (PID %d)\n", getpid());
	fprintf(temp, "i");
}

void ckpt3()
{
	printf("\tsp_simple: 3: Catching SIGCKPT (PID %d)\n", getpid());
	fprintf(temp, "i");
}

void ckpt4()
{
	printf("\tsp_simple: 4: Catching SIGCKPT (PID %d)\n", getpid());
	fprintf(temp, "i");
}

void rest1()
{
	printf("\tsp_simple: catching SIGRESTART (PID %d)\n", getpid());
	fprintf(temp, "i");
}

void
sproc_entry(void *arg)
{
	int id = *(int *)arg;

#ifdef SLEEP
	int i;
	for (i = 0; i < NUMREPS; i++)
		atrestart(rest1);
	atcheckpoint(ckpt1);
	atcheckpoint(ckpt2);
	atcheckpoint(ckpt3);
	atcheckpoint(ckpt4);
#endif

	sleep(id+1);

	printf("\n\tsp_simple: in the sproc'd PID %d: my stk addr: %lx\n",
		getpid(), stkaddr());
	sleep(id+1);
	printf("\tsp_simple: child:%d exiting\n", id);
	fprintf(temp, "i");
	exit(0);
}

main(int argc, char **argv)
{
	int args[6];
	int i;
	int succeeded = 0;
	pid_t ctest_pid = getppid();
	nprocs = atoi(argv[1]);
	temp = tmpfile();
	atcheckpoint(ckpt0);
	if (!nprocs)
		nprocs = 4;


	for (i = 0; i < nprocs; i++) {
		args[i] = i+1;
		sproc(sproc_entry, PR_SADDR|PR_SID|PR_NOLIBC, (void *)&args[i]);
	}

	sleep(1);
	printf("\tsp_simple: in the parent, my stkaddr: %lx\n", stkaddr());
	pause();
	sleep(nprocs+SLEEP1+5);
	printf("\tsp_simple: parent done\n");
	rewind(temp);
	while (getc(temp) != EOF)
		succeeded++;
	
	if (succeeded == (nprocs*5 + nprocs*NUMREPS + 1))
		kill(ctest_pid, SIGUSR1);
	else
		kill(ctest_pid, SIGUSR2);
	printf("\n\tsp_simple: Done\n");
	exit(0);
}
