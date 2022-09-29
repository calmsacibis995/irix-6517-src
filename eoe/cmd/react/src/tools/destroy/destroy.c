#include <sys/types.h>
#include <sys/frs.h>
#include <math.h>
#include <signal.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/frs.h>


main(int argc, char** argv)
{
	int i;
	int ncpus;
	int victimcpu;

	signal(SIGHUP, SIG_IGN);

	if (argc == 2) {
		/*
		 * just one cpu has been specified
		 * we destroy the frs on that cpu
		 */
		victimcpu = atoi(argv[1]);
		printf("Trying to destroy frs on cpu [%d] ...", victimcpu);
		fflush(stdout);
		if (schedctl(MPTS_FRS_DESTROY, victimcpu) < 0) {
			perror("Could not destroy frs:");
		}
	} else {
		ncpus = sysmp(MP_NPROCS);
		if (ncpus < 0) {
			perror("sysmp(MP_NPROCS)");
			exit(1);
		}
	
		for (i = 0; i < ncpus; i++) {
			printf("Trying to destroy frs on cpu [%d] ...", i);
			fflush(stdout);
			if (schedctl(MPTS_FRS_DESTROY, i) < 0) {
				perror("Could not destroy frs:");
			} else {
				printf("Destroyed.\n");
			}
		}
	}
}






