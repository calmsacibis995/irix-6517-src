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
	printf("\tmp: PARENT: Catching SIGCKPT (PID %d)\n", getpid());
	fprintf(temp, "i");
}

void ckpt1()
{
	printf("\tmp: 1: Catching SIGCKPT (PID %d)\n", getpid());
	fprintf(temp, "i");
}

void ckpt2()
{
	printf("\tmp: 2: Catching SIGCKPT (PID %d)\n", getpid());
	fprintf(temp, "i");
}

void ckpt3()
{
	printf("\tmp: 3: Catching SIGCKPT (PID %d)\n", getpid());
	fprintf(temp, "i");
}

void ckpt4()
{
	printf("\tmp: 4: Catching SIGCKPT (PID %d)\n", getpid());
	fprintf(temp, "i");
}

void rest1()
{
	printf("\tmp: catching SIGRESTART (PID %d)\n", getpid());
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

	sleep(id);

	printf("\n\tmp: in the sproc'd PID %d: my stk addr: %lx\n",
		getpid(), stkaddr());
	sleep(id+SLEEP1);
	printf("\tmp: child:%d exiting\n", id);
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
		/* sproc(sproc_entry, PR_SADDR|PR_SID|PR_NOLIBC, (void *)&args[i]); */
		sproc(sproc_entry, PR_SALL, (void *)&args[i]);
	}

	printf("\tmp: in the parent, my stkaddr: %lx\n", stkaddr());
	pause();
	sleep(nprocs+SLEEP1+5);
	printf("\tmp: parent done\n");
	rewind(temp);
	while (getc(temp) != EOF)
		succeeded++;
	
	if (succeeded == (nprocs*5 + nprocs*NUMREPS + 1))
		kill(ctest_pid, SIGUSR1);
	else
		kill(ctest_pid, SIGUSR2);
	printf("\n\tmp: Done\n");
	exit(0);
}
