#include <signal.h>
#include <sys/types.h>
#include <sys/sysmp.h>

void on_ckpt() {;}

main(int argc, char **argv)
{
	pid_t ctest_pid = getppid();
	int cpu;
	int new_cpu = atol(argv[1]);
	signal(SIGCKPT, on_ckpt);
	if ((cpu = sysmp(MP_GETMUSTRUN)) == -1)
		printf("\n\tmustrun: process %d is not bound to any processor\n",
			getpid());
	else
		printf("\n\tmustrun: process %d is bound to cpu %d\n", getpid(), cpu);

	printf("\n\tmustrun: binding process %d to cpu %d...\n", getpid(), new_cpu);

	if ((cpu = sysmp(MP_MUSTRUN, new_cpu)) == -1) {
		perror("MP_MUSTRUN");
	}

	if ((cpu = sysmp(MP_GETMUSTRUN)) == -1)
	{
		printf("\n\tmustrun: process %d is not bound to any processor\n",
			getpid());
		kill(ctest_pid, SIGUSR2);
		exit(1);
	}
	else 
		printf("\n\tmustrun: process %d bound to cpu %d\n", getpid(), cpu);

	pause();

	if ((new_cpu = sysmp(MP_GETMUSTRUN)) == -1) 
        {
                printf("\n\tmustrun: process %d is not bound to any processor\n", 
                	getpid());
		kill(ctest_pid, SIGUSR2);
		exit(1);
	} 
        else if (new_cpu != cpu)
	{
		printf("\n\tmustrun: process %d incorrectly bound to cpu %d after restart\n",
			getpid(), new_cpu);
		kill(ctest_pid, SIGUSR2);
		exit(1);
	}
	else 
	{
		printf("\n\tmustrun: process %d still bound to cpu %d after restart\n",
			getpid(), new_cpu);
		kill(ctest_pid, SIGUSR1);

        }
}

