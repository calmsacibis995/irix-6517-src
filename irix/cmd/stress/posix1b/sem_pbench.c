/*
 * File: sem_pbench.c
 *
 * Function: sem_pbench is a benchmark for measuring the average worst
 *           case performance (100% contention) Posix.1b unnamed semaphores,
 *           and Posix.1b named semaphores shared between PROCESSES.
 *
 *           Arena semaphores where left-out of this benchmark so that
 *           we could run this test on a variety of systems.
 *
 * Arguments:
 *           -u    Posix.1b Unnamed semaphore test
 *           -n    Posix.1b Named semaphore test
 *           -c #  Specify the number of producer/consumer cycles 
 *
 * Default:
 *           Measure Posix.1b unnamed semaphores using 1000000 cycles.
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <time.h>
#include <sched.h>
#include <ulocks.h>
#include <sys/mman.h>
#include <errno.h>

#define PRIORITY 198

#define TEST_TYPE_ALL     0
#define TEST_TYPE_UNNAMED 1
#define TEST_TYPE_NAMED   2

#define SEM1 0
#define SEM2 1

int test_type = TEST_TYPE_ALL;

sem_t *unnamed_sem_array;
sem_t *named_sem1;
sem_t *named_sem2;

int shm_fd;
int mem_size;

int shared = 0;
int realtime = 1;
int global_count = 10000;

float total_time;
struct timespec start_stamp, end_stamp;

void unnamed_child(void);
void named_child(void);

main(int argc, char *argv[])
{
  int loop;
  int parse;
  int pid;
  struct  sched_param param;
  int iterations = 1;
  int wrap_around = 1;
  int every_test = 0;

  while ((parse = getopt(argc, argv, "c:nuri:")) != EOF)
    switch (parse) {
    case 'c':
      global_count = atoi(optarg);
      break;
    case 'u':
      test_type = TEST_TYPE_UNNAMED;
      break;
    case 'n':
      test_type = TEST_TYPE_NAMED;
      break;
    case 'r':
      realtime = 0;
      break;
    case 'i':
      iterations = atoi(optarg);
      break;
    }

  if (realtime) {
	  /*
	   * set priority above system daemons, but below
	   * interrupt threads.
	   */	  
	  param.sched_priority = PRIORITY;
	  if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
		  if (errno == EPERM)
			  printf("You must be root to run this benchmark\n");
		  else
			  perror("sched_setscheduler");              
		  exit(0);                                           
	  }
  }

  if (test_type == TEST_TYPE_ALL) {
    every_test = 1;
    test_type++;
  }

  shm_fd = shm_open ("/usr/tmp/sem_pbench.memory", O_CREAT | O_RDWR , 0666);
  if (shm_fd == -1) {
    perror("shm_open");
    exit(1);
  }
  mem_size = 2 * sizeof(sem_t);

  if (ftruncate(shm_fd, mem_size) < 0) {
    perror("ftruncate");
    exit(1);
  }

  if (mlockall(MCL_CURRENT)) {
    perror("mlockall");
    exit(1);
  }

  unnamed_sem_array = (sem_t *) mmap(0, mem_size, PROT_READ|PROT_WRITE,
				     MAP_SHARED, shm_fd, 0);
  
repeat:

  switch(test_type) {
  case TEST_TYPE_UNNAMED:

    if (sem_init(&unnamed_sem_array[SEM1], 0, 1)) {
      perror("FAILED: sem_init");
      exit(1);
    }

    if (sem_init(&unnamed_sem_array[SEM2], 0, 0)) {
      perror("FAILED: sem_init 2");
      exit(1);
    }

    if (wrap_around == 1)
      printf ("sem_pbench: posix UNNAMED semaphore PROCESS test...\n");
 
    /* fork child */
    if ((pid = fork()) == -1) {
      perror("FAILED: child fork failure");
      exit(1);
    }

    if (pid == 0) {
      unnamed_child();
      exit(0);
    }
    break;

  case TEST_TYPE_NAMED:
    named_sem1 = sem_open("/usr/tmp/sema1.posix",
			  O_RDWR|O_CREAT|O_TRUNC,
			  S_IRWXU,
			  1);

    if ((int) named_sem1 == -1) {
      perror("FAILED: sem_open1");
      exit(1);
    }

    named_sem2 = sem_open("/usr/tmp/sema2.posix",
			  O_RDWR|O_CREAT|O_TRUNC,
			  S_IRWXU,
			  0);

    if ((int) named_sem2 == -1) {
      perror("FAILED: sem_open2");
      exit(1);
    }

    if (sem_unlink("/usr/tmp/sema1.posix") < 0) {
      perror("FAILED: sem_unlink");
      exit(1);
    }
    if (sem_unlink("/usr/tmp/sema2.posix") < 0) {
      perror("FAILED: sem_unlink 2");
      exit(1);
    }

    if (wrap_around == 1)
      printf ("sem_pbench: posix NAMED semaphore PROCESS test...\n");

    /* fork child */
    if ((pid = fork()) == -1) {
      perror("FAILED: child fork failure");
      exit(1);
    }

    if (pid == 0) {
      named_child();
      exit(0);
    }
    break;
  }

  loop = global_count/2;

  if ((clock_gettime(CLOCK_REALTIME, &start_stamp)) == -1) {
     perror("FAILED: clock_gettime");
     exit(1);
  }

  switch (test_type) {
  case TEST_TYPE_UNNAMED:
    while (loop--) {
      if (sem_wait(&unnamed_sem_array[SEM1]) < 0) {
	perror("FAILED: unnamed sem_wait parent");
	exit(1);
      }
      if (sem_post(&unnamed_sem_array[SEM2]) < 0) {
	perror("FAILED: unnamed sem_post parent");
	exit(1);
      }
    }
    break;
  case TEST_TYPE_NAMED:
    while (loop--) {
      if (sem_wait(named_sem1) < 0) {
	perror("FAILED: named sem_wait parent");
	exit(1);
      }
      if (sem_post(named_sem2) , 0) {
	perror("FAILED: named sem_wait parent");
	exit(1);
      }
    }
    break;
  }

  if ((clock_gettime(CLOCK_REALTIME, &end_stamp)) == -1) {
     perror("FAILED: clock_gettime");
     exit(1);
  }  

  end_stamp.tv_sec -= start_stamp.tv_sec;
  if (end_stamp.tv_nsec >= start_stamp.tv_nsec) {
        end_stamp.tv_nsec -= start_stamp.tv_nsec;
  } else {
    if (end_stamp.tv_sec > 0)
      end_stamp.tv_sec--;
    end_stamp.tv_nsec = (end_stamp.tv_nsec - start_stamp.tv_nsec) + 1000000000;
  }

  total_time = end_stamp.tv_sec * 1000000 + end_stamp.tv_nsec/1000;

  printf("[%d] %d semaphore blocks took %d.%d seconds: %5.1f uS/block\n",
	 wrap_around, global_count, end_stamp.tv_sec, end_stamp.tv_nsec/1000,
	 (float) total_time/global_count);


  wait(NULL);

  switch (test_type) {
  case TEST_TYPE_UNNAMED:
    if (sem_destroy(&unnamed_sem_array[SEM1]) < 0) {
      perror("FAILED: sem_destroy");
      exit(1);
    }
    if (sem_destroy(&unnamed_sem_array[SEM2]) < 0) {
      perror("FAILED: sem_destroy 2");
      exit(1);
    }
    break;
  case TEST_TYPE_NAMED:
    if (sem_close(named_sem1) < 0) {
      perror("FAILED: sem_close");
      exit(1);
    }
    if (sem_close(named_sem2) < 0) {
      perror("FAILED: sem_close 2");
      exit(1);
    }
    
    break;
  }

  if (wrap_around++ < iterations)
    goto repeat;

  if (every_test && ++test_type <= 2) {
    wrap_around = 1;
    goto repeat;
  }
  
  if (shm_unlink("/usr/tmp/sem_pbench.memory") < 0) {
    perror("FAILED: shm_unlink");
    exit(1);
  }

  if (munlockall() < 0)
    perror("FAILED: munlockall");

  exit(0);
}

void
unnamed_child(void)
{
  int loop = global_count/2;

  while (loop--) {
    if (sem_wait(&unnamed_sem_array[SEM2]) < 0) {
      perror("FAILED: unnamed sem_wait child");
      exit(1);
    }
    if (sem_post(&unnamed_sem_array[SEM1]) < 0) {
      perror("FAILED: unnamed sem_post child");
      exit(1);
    }
  }

  exit(0);
}

void
named_child(void)
{
  int loop = global_count/2;

  while (loop--) {
    if (sem_wait(named_sem2) < 0) {
      perror("FAILED: named sem_wait child");
      exit(1);
    }
    if (sem_post(named_sem1) < 0) {
      perror("FAILED: unnamed sem_post child");
      exit(1);
    }
  }

  exit(0);
}
