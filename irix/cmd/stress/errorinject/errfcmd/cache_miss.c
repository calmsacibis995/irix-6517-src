#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <fs/procfs/procfs.h>
#include <syslog.h>
#include <sys/sysmp.h>
#include <sys/resource.h>

int gen, fd, thresh;

#define ALLOC_SIZE	0x10000
#define ALLOC_TABLE_SIZE 256
char *alloc_table[ALLOC_TABLE_SIZE];


void
cache_err_die(char *x)
{
	syslog(LOG_ALERT, x);
	exit(-1);
}



do_accesses(int cpu)
{
	char *tmp;
	int i, j;
	int old_cnt, bad_cnt;
	
	if (sysmp(MP_MUSTRUN, cpu) == -1)  {
		printf("could not run on cpu %d\n", cpu);
		return -1;
	}

	for (i = 0; i < ALLOC_TABLE_SIZE; i++) {
		if (alloc_table[i] == NULL) break;
		for (j = 0; j < ALLOC_SIZE; j += 512)  {
			*(volatile char *)(alloc_table[i] + j);
			*(volatile char *)(alloc_table[i] + j + 128);
			*(volatile char *)(alloc_table[i] + j + 256);
			*(volatile char *)(alloc_table[i] + j + 384);
		}
	}
	return 0;
}

main(int argc, char *argv[])
{
	char file[32];
	int i;
	int error;
	int cpu = 0;

	int size = 0x200000;
	int numprocs;

	if (argc > 1) 
	    cpu = strtol(argv[1], NULL, 0);

	for (i = 0; i <= size / ALLOC_SIZE; i++) {
		if (i >= ALLOC_TABLE_SIZE) break;
		if ((alloc_table[i] = (char *)malloc(ALLOC_SIZE)) == NULL)
		    cache_err_die("Unable to allocate memory for test");
	}

	openlog(argv[0], LOG_PID|LOG_CONS|LOG_PERROR|LOG_NDELAY, LOG_USER);
	syslog(LOG_INFO, "started. Cache size %x. Threshold %d", 
	       size, thresh);
	
	numprocs = sysmp(MP_NPROCS);
	if (numprocs == -1) 
		cache_err_die("sysmp(MP_NPROCS) = -1!");

	if (setpriority(PRIO_PROCESS, getpid(), 20) < 0)
	    perror("setprio");

	printf("%s Running scache tests on size %d (%x)\n", argv[0], size, size);

	sprintf(file, "/proc/%d", getpid());
	fd = open(file, O_RDWR);


	error = 1;
	while (1) {
		int i;
		int num_err;

		num_err = 0;
		for (i =0; i <2000; i++)
		    do_accesses(cpu);
		exit(0);
#if 0
		for (cpu = 0; cpu < numprocs; cpu++) {
			do_accesses(cpu);
		}
		sleep (300);
#endif
	}

}


