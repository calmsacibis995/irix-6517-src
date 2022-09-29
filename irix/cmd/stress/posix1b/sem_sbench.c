/*
 * File: sem_sbench.c
 *
 * Function: sem_sbench is a benchmark for measuring the average
 *           worst case performance (100% contention) of arena
 *           semaphores, Posix.1b unnamed semaphores, and Posix.1b
 *           named semaphores shared between SPROCS.
 *
 * Arguments:
 *           -u    Posix.1b Unnamed semaphore test
 *           -n    Posix.1b Named semaphore test
 *           -a    Irix arena semaphores (usemas)
 *           -c #  Specify the number of producer/consumer cycles 
 *
 * Default:
 *           Measure Posix.1b unnamed semaphores using 1000000 cycles.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <time.h>
#include <sched.h>
#include <ulocks.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/wait.h>

#define PRIORITY 198

#define TEST_TYPE_ALL     0
#define TEST_TYPE_UNNAMED 1
#define TEST_TYPE_NAMED   2
#define TEST_TYPE_ARENA   3
#define TEST_TYPE_SVR4    4

int test_type = TEST_TYPE_ALL;

sem_t unnamed_sema1;
char  x[1000];
sem_t unnamed_sema2;

sem_t *named_sema1;
sem_t *named_sema2;

usema_t *arena_sema1;
usema_t *arena_sema2;

#define SEMKEY    1003

#define WAIT_SEM1 0
#define WAIT_SEM2 1
#define POST_SEM1 2
#define POST_SEM2 3

int svr4_sema;
static struct sembuf svr4_op[4] = {{0,-1,NULL},
				   {1,-1,NULL},
				   {0,1,NULL},
				   {1,1,NULL}};

int shared;
int verbose = 0;
int realtime = 1;
int global_count = 1000000;
int spins = 0;

unsigned int total_time;
struct timespec start_stamp, end_stamp;

#define VERBOSE_INC()   if (verbose) shared++
#define VERBOSE_PRINT() if (verbose) printf("%d\n", shared)

void child(void *);

main(int argc, char *argv[])
{
	int loop;
	int parse;
	usptr_t *hdr;
	struct  sched_param param;
	int iterations = 1;
	int wrap_around = 1;
	int every_test = 0;
	union semun semarg;

	while ((parse = getopt(argc, argv, "c:i:anusvrp:")) != EOF)
		switch (parse) {
		case 'c':
			global_count = atoi(optarg);
			break;
		case 'a':
			test_type = TEST_TYPE_ARENA;
			break;
		case 'u':
			test_type = TEST_TYPE_UNNAMED;
			break;
		case 'n':
			test_type = TEST_TYPE_NAMED;
			break;
		case 's':
			test_type = TEST_TYPE_SVR4;
			break;
		case 'i':
			iterations = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'r':
			realtime = 0;
			break;
		case 'p':
			spins = atoi(optarg);
			break;
		}

	if (test_type == TEST_TYPE_ALL) {
		every_test = 1;
		test_type++;
	}
	
	if (realtime) {
		param.sched_priority = PRIORITY;
		if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
			if (errno == EPERM)
			   printf("You must be root to run this benchmark\n");
			else
				perror("sched_setscheduler");              
			exit(0);                                           
		}
	}
	
	if (mlockall(MCL_CURRENT|MCL_FUTURE)) {
		perror("mlockall");
		exit(1);
	}

	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	usconfig(CONF_INITUSERS, 2);

repeat:

	/*
	 * Initialize Semaphores
	 */
	switch(test_type) {
	case TEST_TYPE_UNNAMED:
		if (sem_init(&unnamed_sema1, 0, 1)) {
			perror("sem_init1");
			exit(1);
		}

		if (sem_init(&unnamed_sema2, 0, 0)) {
			perror("sem_init2");
			exit(1);
		}

		if (wrap_around == 1)
		  printf ("sem_sbench: posix UNNAMED semaphore SPROC test...\n");
		
		if (spins) {
			if (sem_mode(&unnamed_sema1, SEM_MODE_SPINSET, spins)) {
				perror("sem_sbench: sem_mode1 spinset");
				exit(1);
			}

			if (sem_mode(&unnamed_sema2, SEM_MODE_SPINSET, spins)) {
				perror("sem_sbench: sem_mode2 spinset");
				exit(1);
			}
			
			printf("sem_sbench: spins = %d\n", spins);
		}
		break;

	case TEST_TYPE_NAMED:
		named_sema1 = sem_open("/usr/tmp/sema1.posix",
				       O_RDWR|O_CREAT|O_TRUNC,
				       S_IRWXU,
				       1);

		if (named_sema1 == SEM_FAILED) {
			perror("sem_open1");
			exit(1);
		}

		named_sema2 = sem_open("/usr/tmp/sema2.posix",
				       O_RDWR|O_CREAT|O_TRUNC,
				       S_IRWXU,
				       0);

		if ((int) named_sema2 == -1) {
			perror("sem_open2");
			exit(1);
		}

		if (sem_unlink("/usr/tmp/sema1.posix") < 0) {
			perror("FAILED unlink");
			exit(1);
		}
		if (sem_unlink("/usr/tmp/sema2.posix") < 0) {
			perror("FAILED unlink 2");
			exit(1);
		}      

		if (wrap_around == 1)
		   printf("sem_sbench: posix NAMED semaphore SPROC test...\n");

		break;

	case TEST_TYPE_ARENA:
		
		if ((hdr = usinit("/usr/tmp/sbench_arena.arena")) == NULL) {
			perror("usinit");
			exit (1);
		}

		if ((arena_sema1 = usnewsema(hdr, 1)) == NULL) {
			perror("usnewsema1");
			exit(1);
		}

		if ((arena_sema2 = usnewsema(hdr, 0)) == NULL) {
			perror("usnewsema2");
			exit(1);
		}

		if (wrap_around == 1)
		   printf ("sem_sbench: irix ARENA semaphore SPROC test...\n");

		break;

	case TEST_TYPE_SVR4:
		if ((svr4_sema = semget(SEMKEY, 2, 0777|IPC_CREAT)) == -1) {
			perror("semget");
			exit(-1);
		}

		semarg.val = 1;
		if (semctl(svr4_sema, 0, SETVAL, semarg) == -1) {
			perror("semctl");
			semctl(svr4_sema, 2, IPC_RMID);
			exit(1);
		}

		semarg.val = 0;
		if (semctl(svr4_sema, 1, SETVAL, semarg) == -1) {
			perror("semctl");
			semctl(svr4_sema, 2, IPC_RMID);
			exit(1);
		}

		if (wrap_around == 1)
			printf("sem_sbench: SVR4 semaphore SPROC test...\n");

		break;
	}

	if (sproc(child, PR_SALL) == -1) {
		perror("sproc failed");
		exit(1);
	}

	shared = 0;
	loop = global_count/2;

	if ((clock_gettime(CLOCK_REALTIME, &start_stamp)) == -1) {
		perror("clock_gettime");
		exit(1);
	}

	switch (test_type) {
	case TEST_TYPE_UNNAMED:
		while (loop--) {
			if (sem_wait(&unnamed_sema1) < 0) {
				perror("P: sem_wait - unnamed");
				exit(1);
			}
			VERBOSE_PRINT();
			if (sem_post(&unnamed_sema2) < 0) {
				perror("P: sem_post - unnamed");
				printf ("%d\n", errno);
				exit(1);
			}
		}
		break;

	case TEST_TYPE_NAMED:
		while (loop--) {
			if (sem_wait(named_sema1) < 0) {
				perror("P: sem_wait - named");
				exit(1);
			}
			VERBOSE_PRINT();
			if (sem_post(named_sema2) < 0) {
				perror("P: sem_post - named");
				exit(1);
			}
		}
		break;

	case TEST_TYPE_ARENA:
		while (loop--) {
			if (uspsema(arena_sema1) < 0) {
				perror("P: uspsema");
				exit(1);
			}
			VERBOSE_PRINT();
			if (usvsema(arena_sema2) < 0) {
				perror("P: usvsema");
				exit(1);
			}
		}
		break;

	case TEST_TYPE_SVR4:
		while (loop--) {
			if (semop(svr4_sema, &svr4_op[WAIT_SEM1], 1) < 0) {
				perror("P: semop - wait");
				exit(1);
			}
			VERBOSE_PRINT();
			if (semop(svr4_sema, &svr4_op[POST_SEM2], 1) < 0) {
				perror("P: semop - post");
				exit(1);
			}
		}
		break;
	}
	
	if ((clock_gettime(CLOCK_REALTIME, &end_stamp)) == -1) {
		perror("clock_gettime");
		exit(1);
	}  

	end_stamp.tv_sec -= start_stamp.tv_sec;
	if (end_stamp.tv_nsec >= start_stamp.tv_nsec) {
		end_stamp.tv_nsec -= start_stamp.tv_nsec;
	} else {
		if (end_stamp.tv_sec > 0)
			end_stamp.tv_sec--;
		end_stamp.tv_nsec = (end_stamp.tv_nsec - start_stamp.tv_nsec)
			+ 1000000000;
	}

	total_time = end_stamp.tv_sec * 1000000 + end_stamp.tv_nsec/1000;
	
	printf("[%d] %d semaphore blocks took %d.%d seconds: %5.1f uS/block\n",
	       wrap_around, global_count, end_stamp.tv_sec,
	       end_stamp.tv_nsec/1000,
	       (float) total_time/global_count);

	/* 
	 * Destroy semaphores
	 */

	wait(NULL);

	switch (test_type) {
	case TEST_TYPE_UNNAMED:
		if (sem_destroy(&unnamed_sema1) < 0) {
			perror("sem_destroy 1");
			exit(1);
		}
		if (sem_destroy(&unnamed_sema2) < 0) {
			perror("sem_destroy 2");
			exit(1);
		}
		break;

	case TEST_TYPE_NAMED:
		if (sem_close(named_sema1) < 0) {
			perror("sem_close 1");
			exit(1);
		}
		if (sem_close(named_sema2) < 0) {
			perror("sem_close 2");
			exit(1);
		}
		break;

	case TEST_TYPE_ARENA:
		usfreesema(arena_sema1, hdr);
		usfreesema(arena_sema2, hdr);
		break;
		
	case TEST_TYPE_SVR4:
		if (semctl(svr4_sema, 2, IPC_RMID) < 0) {
			perror("semctl - ipc_rmid");
			exit(1);
		}
		break;
	}

	if (wrap_around++ < iterations)
		goto repeat;
	
	if (every_test && ++test_type <= 4) {
		wrap_around = 1;
		goto repeat;
	}
	
	if (munlockall() < 0)
		perror("munlockall");

	exit(0);
}

/* ARGSUSED */
void
child(void *arg)
{
	int loop = global_count/2;

	switch (test_type) {
	case TEST_TYPE_UNNAMED:
		while (loop--) {
			if (sem_wait(&unnamed_sema2) < 0) {
				perror("C: sem_wait - unnamed");
				exit(1);
			}
			VERBOSE_INC();
			if (sem_post(&unnamed_sema1) < 0) {
				perror("C: sem_post - unnamed");
				exit(1);
			}
		}
		break;

	case TEST_TYPE_NAMED:
		while (loop--) {
			if (sem_wait(named_sema2) < 0) {
				perror("C: sem_wait - named");
				exit(1);
			}
			VERBOSE_INC();
			if (sem_post(named_sema1) < 0) {
				perror("C: sem_post - named");
				exit(1);
			}
		}
		break;

	case TEST_TYPE_ARENA:
		while (loop--) {
			if (uspsema(arena_sema2) < 0) {
				perror("C: uspsema");
				exit(1);
			}
			VERBOSE_INC();
			if (usvsema(arena_sema1) < 0) {
				perror("C: usvsema");
				exit(1);
			}
		}
		break;

	case TEST_TYPE_SVR4:
		while (loop--) {
			if (semop(svr4_sema, &svr4_op[WAIT_SEM2], 1) < 0) {
				perror("C: semop - wait");
				exit(1);
			}
			VERBOSE_INC();
			if (semop(svr4_sema, &svr4_op[POST_SEM1], 1) < 0) {
				perror("C: semop - post");
				exit(1);
			}
		}
		break;
	}

	exit(0);
}
