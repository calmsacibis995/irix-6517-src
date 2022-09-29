#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <ulocks.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>



/*
 * Unit/stress test for tfault.  Have multiple processes touching
 * the same region of memory to test the multi-reader region locks.
 * This program is specially written to generate lots of tfaults.
 *
 */

#define PASS	1
#define FAIL	0

#define Message if (Verbose) printf
#define Debug	if (Dbg) printf

int Verbose = 0;
int Dbg = 0;
int Iterations = 20000;
int Nprocs = 20;

#define NBPP		4096
#define NUM_SEGS	8	/* max wired entries */

char *segs[] = {
	(char *) 0x30000000,	/* segment boundary addresses for mappings */
	(char *) 0x30400000,
	(char *) 0x30800000,
	(char *) 0x30c00000,
	(char *) 0x31000000,
	(char *) 0x31400000,
	(char *) 0x31800000,
	(char *) 0x31c00000
};


void child();

main(int argc, char *argv[])
{
	int c;
	int errflg = 0;
	usptr_t *arena;

	while ((c = getopt(argc, argv, "vdi:n:")) != -1)
		switch (c) {
		case 'v':
			Verbose++;
			break;
		case 'd':
			Dbg++;
			break;
		case 'i':
			Iterations = atoi(optarg);
			break;
		case 'n':
			Nprocs = atoi(optarg);
			break;
		case '?':
			errflg++;
			break;
		}

	if (errflg) {
		fprintf(stderr, 
"Usage: tfault [-v] [-d] [-i iterations] [-n nprocs] [-a arena_size]\n");
		exit(1);
	}


	/*
	 * General initialization.
	 */

	if (usconfig(CONF_INITUSERS, Nprocs + 1) == -1) {
		perror("usconfig");
		exit(1);
	}


	/*
	 * Do the tests
	 */

	if (stress_tests() == FAIL) {
		printf("\n*** STRESS TESTS FAILED ***\n\n");
		exit(1);
	}

	printf("tfault: All tests passed\n");
	exit(0);
}


stress_tests()
{
	int errors = 0;
	int status, i, fd, *pid, shmid;
	void *old_brk;

	Message("Beginning Stress Tests...\n");
	Message("Using sproc and shared /dev/zero mappings...\n");

	if ((fd = open("/dev/zero", O_RDWR)) == -1) {
		perror("/dev/zero");
		return FAIL;
	}

	Debug("Setting up mappings...\n");

	for (i = 0; i < NUM_SEGS; i++)
		if (mmap(segs[i], NBPP, PROT_READ|PROT_WRITE, MAP_SHARED,
				fd, 0) == (void *) -1) {
			fprintf(stderr, "Couldn't map 0x%x\n", segs[i]);
			perror("mmap");
			exit(1);
		}


	Debug("Starting child processes...\n");

	for (i = 0; i < Nprocs; i++) {
		if (sprocsp(child, PR_SALL, NULL, NULL, 32*1024) == -1) {
			perror("sproc");
			errors++;
			break;
		}
	}

	Debug("Starting children...\n");

	unblockprocall(getpid());

	Debug("Waiting for children to finish...\n");

	while (wait(&status) != -1)
		if (status)
			errors++;

	Debug("All child processes done...\n");
	close(fd);

	for (i = 0; i < NUM_SEGS; i++)
		if (munmap(segs[i], NBPP) == -1) {
			fprintf(stderr, "Couldn't munmap 0x%x\n", segs[i]);
			perror("munmap");
			exit(1);
		}

	/*
	 * Now try it again with a shared memory segment just to see if
	 * having separate pmaps and aspacelocks makes any difference.
	 */

	Message("Using fork and shmem...\n");

	pid = malloc(Nprocs * sizeof(int));

	Debug("Setting up mappings...\n");

	for (i = 0; i < NUM_SEGS; i++) {
		if ((shmid = shmget(IPC_PRIVATE, NBPP, 0)) == -1) {
			perror("shmget");
			exit(1);
		}

		if (shmat(shmid, segs[i], 0) == (void *) -1) {
			fprintf(stderr, "Couldn't shmat 0x%x\n", segs[i]);
			perror("shmat");
			exit(1);
		}

		if (shmctl(shmid, IPC_RMID) == -1)
			perror("shmctl(IPC_RMID)");
	}

	Debug("Forking child processes...\n");

	for (i = 0; i < Nprocs; i++) {
		if ((pid[i] = fork()) == -1) {
			perror("fork");
			errors++;
			break;
		}

		if (pid[i] == 0)
			child();
	}

	Debug("Starting children...\n");

	for (i = 0; i < Nprocs; i++)
		unblockproc(pid[i]);

	Debug("Waiting for children to finish...\n");

	while (wait(&status) != -1)
		if (status)
			errors++;

	Debug("All child processes done...\n");

	for (i = 0; i < NUM_SEGS; i++)
		if (shmdt(segs[i]) == -1) {
			fprintf(stderr, "Couldn't shmdt 0x%x\n", segs[i]);
			perror("shmdt");
			exit(1);
		}

	if (errors)
		return FAIL;

	return PASS;
}


void
child()
{
	int i, x;
	int seg, fd;
	void *addr;

	if ((fd = open("/dev/zero", O_RDWR)) == -1) {
		perror("child open of /dev/zero");
		exit(1);
	}

	if ((addr = mmap(NULL, NBPP, PROT_READ|PROT_WRITE, 
			 MAP_PRIVATE|MAP_LOCAL, fd, 0)) == (void *) -1) {
		perror("child mmap");
		exit(1);
	}

	blockproc(getpid());

	Debug("child %d starting...\n", getpid());

	for (i = 0, seg = 0; i < Iterations; i++) {
		x = *(volatile int *)segs[seg];

		seg++;

		/*
		 * Once we've touched enough segments to use up all the
		 * wired slots, invalidate them all so that we continue
		 * to take tfaults.  In we allow the system to go into
		 * segment table mode (fast kmiss path), then we won't
		 * be testing the tfault code.  One way to invalidate
		 * the wired entries is to change the address space.  The
		 * easiest way to do this is with an mprotect.
		 */

		if (seg >= NUM_SEGS) {
			if (mprotect(addr, NBPP, PROT_READ) == -1) {
				perror("mprotect read-only");
				exit(1);
			}

			if (mprotect(addr, NBPP, PROT_READ|PROT_WRITE) == -1) {
				perror("mprotect read-write");
				exit(1);
			}

			seg = 0;
		}
	}

	munmap(addr, NBPP);
	close(fd);
	exit(0);
}
