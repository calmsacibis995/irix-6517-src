/*
 * sem_speed.c - program to measure non-blocking semaphore overhead
 *
 * DISCLAIMER:
 * This software and its documentation are provided "AS IS" and without 
 * any expressed or implied warranties whatsoever.  No warranties as to 
 * performance, merchantability, or fitness for a particular purpose exist.
 * The user of this software is advised to test it thoroughly before relying 
 * on it.  The user must assume the entire risk and liability of using this
 * software.
 *
 * In no event shall Silicon Graphics or any Silicon Graphics employee be
 * held responsible for any direct, indirect, consequential or inconsequential 
 * damages or lost profits related to this software.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <time.h>
#include <sys/schedctl.h>
#include <ulocks.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/mman.h>

#define SEMKEY    1003

#define TEST_TYPE_ALL     0
#define TEST_TYPE_UNNAMED 1
#define TEST_TYPE_NAMED   2
#define TEST_TYPE_SVR4    3
#define TEST_TYPE_ARENA   4

#define LAST_TEST_TYPE TEST_TYPE_ARENA

int test_type = TEST_TYPE_ALL;

sem_t unnamed_sema1;
sem_t *named_sema1;
usema_t *arena_sema1;

static struct sembuf psema1[1] = { 0, -1, NULL };
static struct sembuf vsema1[1] = { 0,  1, NULL };

int  sema;
int  out_val[2];
int  i,val;

int global_count = 100000;

float total_time;
struct timespec start_stamp, end_stamp;
float block_time;

main(int argc, char *argv[])
{
	int loop;
	int parse;
	usptr_t *hdr;
	int iterations = 1;
	int wrap_around = 1;
	int every_test = 0;
	union semun semarg;

	while ((parse = getopt(argc, argv, "c:anusi:")) != EOF)
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
		}
	
	if (test_type == TEST_TYPE_ALL) {
		every_test = 1;
		test_type++;
	}

	if (mlockall(MCL_CURRENT|MCL_FUTURE)) {
		perror("sem_speed: FAILED - mlockall");
		exit(1);
	}

	/*
	 * Configure arena for all tests so that the memory footprint
	 * is the same for all tests.
	 */
	usconfig(CONF_ARENATYPE, US_SHAREDONLY);
	usconfig(CONF_INITUSERS, 2);

	if ((hdr = usinit("/usr/tmp/sbench_arena.arena")) == NULL) {
		perror("sem_speed: FAILED - usinit");
		exit (1);
	}

repeat:

	switch(test_type) {
	case TEST_TYPE_UNNAMED:
		if (sem_init(&unnamed_sema1, 0, 1)) {
			perror("sem_speed: FAILED - sem_init");
			exit(1);
		}

		if (wrap_around == 1)
			printf("sem_speed: posix UNNAMED semaphore test...\n");
		break;

	case TEST_TYPE_NAMED:
		named_sema1 = sem_open("/usr/tmp/sema1.posix",
				       O_RDWR|O_CREAT|O_TRUNC,
				       S_IRWXU,
				       1);

		if (named_sema1 == SEM_FAILED) {
			perror("sem_speed: FAILED - sem_open");
			exit(1);
		}

		if (sem_unlink("/usr/tmp/sema1.posix") < 0) {
			perror("sem_speed: FAILED - sem_unlink");
			exit(1);
		}

		if (wrap_around == 1)
			printf("sem_speed: posix NAMED semaphore test...\n");
		break;

	case TEST_TYPE_ARENA:
		if ((arena_sema1 = usnewsema(hdr, 1)) == NULL) {
			perror("sem_speed: FAILED - usnewsema1");
			exit(1);
		}

		if (wrap_around == 1)
			printf("sem_speed: irix ARENA semaphore test...\n");
		break;
    
	case TEST_TYPE_SVR4:
		if ((sema = semget(SEMKEY,1,0777|IPC_CREAT)) == -1) {
			perror("sem_speed: FAILED - semget");
			exit(1);
		}

		semarg.val = 1;
		if (semctl(sema,0,SETVAL,semarg) == -1) {
			perror("sem_speed: FAILED - semctl");
			semctl(sema, 2 , IPC_RMID,0 );
			exit(1);
		}

		if (wrap_around == 1)
			printf("sem_speed: UNIX SVR4 semaphore test...\n");
		break;
	}

	loop = global_count/2;
	
	if (clock_gettime(CLOCK_SGI_CYCLE, &start_stamp) < 0) {
		perror("sem_speed: FAILED - clock_gettime");
		exit(1);
	}

	switch (test_type) {
	case TEST_TYPE_UNNAMED:
		while (loop--) {
			if (sem_wait(&unnamed_sema1) < 0) {
				perror("sem_speed: FAILED - unnamed sem_wait");
				exit(1);
			}
			if (sem_post(&unnamed_sema1) < 0) {
				perror("sem_speed: FAILED - unnamed sem_post");
				exit(1);
			}
		}
		break;

	case TEST_TYPE_NAMED:
		while (loop--) {
			if (sem_wait(named_sema1) < 0) {
				perror("sem_speed: FAILED - named sem_wait");
				exit(1);
			}
			if (sem_post(named_sema1) < 0) {
				perror("sem_speed: FAILED - named sem_post");
				exit(1);
			}
		}
		break;

	case TEST_TYPE_ARENA:
		while (loop--) {
			if (uspsema(arena_sema1) < 0) {
				perror("sem_speed: FAILED - uspsema");
				exit(1);
			}
			if (usvsema(arena_sema1) < 0) {
				perror("sem_speed: FAILED - usvsema");
				exit(1);
			}
		}
		break;

	case TEST_TYPE_SVR4:
		while (loop--) {
			if (semop(sema, &psema1[0], 1) == -1 ) {
				perror("sem_speed: FAILED - semop psema1");
				exit(-1) ;
			}
			if (semop(sema, &vsema1[0], 1) == -1 ) {
				perror("sem_speed: FAILED - semop vsema1");
				exit(-1);
			}
		}
		semctl(sema, 1 , IPC_RMID);
	}

	if ((clock_gettime(CLOCK_SGI_CYCLE, &end_stamp)) == -1) {
		perror("sem_speed: FAILED - clock_gettime 2");
		exit(1);
	}
	end_stamp.tv_sec -= start_stamp.tv_sec;
	if (end_stamp.tv_nsec >= start_stamp.tv_nsec) {
		end_stamp.tv_nsec -= start_stamp.tv_nsec;
	} else {
		if (end_stamp.tv_sec > 0)
			end_stamp.tv_sec--;
		end_stamp.tv_nsec =
			(end_stamp.tv_nsec - start_stamp.tv_nsec) + 1000000000;
	}

	total_time = end_stamp.tv_sec * 1000000 + end_stamp.tv_nsec/1000;

	printf("[%d] %d non-block semaphore ops took %d.%d seconds: %5.1f uS/block\n",
	       wrap_around, global_count, end_stamp.tv_sec,
	       end_stamp.tv_nsec/1000,
	       (float) total_time/global_count);

	if (wrap_around++ < iterations)
		goto repeat;

	if (every_test && ++test_type <= 4) {
		wrap_around = 1;
		goto repeat;
	}

	if (munlockall() < 0) {
		perror("sem_speed: FAILED - munlockall");
		exit(1);
	}

	exit(0);
}

