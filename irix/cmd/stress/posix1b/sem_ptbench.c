/*
 * File: sem_ptbench.c
 *
 * Function: sem_ptbench is a benchmark for measuring the average
 *           worst case performance (100% contention) of
 *           the PTHREAD version of Posix.1b unnamed semaphores.
 *           named semaphores shared between PTHREADS.
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
#include <pthread.h>

sem_t unnamed_sema1;
sem_t unnamed_sema2;

int counter = 0;
int global_count = 10000;
int verbose = 0;

float total_time;
struct timespec start_stamp, end_stamp;

pthread_t thread_id;

#define VERBOSE_INC()   if (verbose) counter++
#define VERBOSE_PRINT() if (verbose) printf("%d\n", counter)

void pchild(void);

main(int argc, char *argv[])
{
  int loop;
  int parse;
  int share_semaphore = 0;

  while ((parse = getopt(argc, argv, "c:nsv")) != EOF)
    switch (parse) {
    case 'c':
      global_count = atoi(optarg);
      break;
    case 'n':
      share_semaphore = 0;
      break;
    case 's':
      share_semaphore = 1;
      break;
    case 'v':
      verbose = 1;
      break;
    }

  if (mlockall(MCL_CURRENT|MCL_FUTURE)) {
    perror("sem_ptbench: ERROR - mlockall");
    exit(1);
  }

  if (sem_init(&unnamed_sema1, share_semaphore, 0)) {
    perror("sem_ptbench: ERROR - sem_init1");
    exit(1);
  }

  if (sem_init(&unnamed_sema2, share_semaphore, 1)) {
    perror("sem_ptbench: ERROR - sem_init2");
    exit(1);
  }


  if (share_semaphore)
    printf ("sem_ptbench: posix SHARE semaphore test...\n");
  else
    printf ("sem_ptbench: posix NOSHARE semaphore test...\n");


  if ((pthread_create(&thread_id, NULL, (void *) pchild, (void *) 0)) != 0)  {
    perror("sem_ptbench: ERROR - pthread_create");
    exit(2);
  }

  loop = global_count/2;

  if ((clock_gettime(CLOCK_REALTIME, &start_stamp)) == -1) {
     perror("sem_ptbench: ERROR - clock_gettime");
     exit(1);
  }

  while (loop--) {
    if (sem_wait(&unnamed_sema1) < 0) {
      perror("sem_ptbench: ERROR - parent - sem_wait");
      exit(1);
    }
    VERBOSE_PRINT();
    if (sem_post(&unnamed_sema2) < 0) {
      perror("sem_ptbench: ERROR - parent - sem_post");
      exit(1);
    }
  }

  if ((clock_gettime(CLOCK_REALTIME, &end_stamp)) == -1) {
     perror("sem_ptbench: ERROR - clock_gettime");
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

  printf("\t%d semaphore blocks took %d.%d seconds\n",
	 global_count, end_stamp.tv_sec, end_stamp.tv_nsec/1000);

  printf("\t%5.1f uS per block\n", (float) total_time/global_count);

  if (sem_destroy(&unnamed_sema1) < 0)
    perror("sem_ptbench: ERROR - sem_destroy 1");

  if (sem_destroy(&unnamed_sema2) < 0)
    perror("sem_ptbench: ERROR - sem_destroy 2");

  exit(0);
}

void
pchild(void)
{
  int loop = global_count/2;

  while (loop--) {
    if (sem_wait(&unnamed_sema2) < 0) {
      perror("sem_ptbench: ERROR - child - sem_wait");
      pthread_exit((void *) NULL);
    }
    VERBOSE_INC();
    if (sem_post(&unnamed_sema1) < 0) {
      perror("sem_ptbench: ERROR - child - sem_post");
      pthread_exit((void *) NULL);
    }
  }

  pthread_exit((void *) NULL);
}
