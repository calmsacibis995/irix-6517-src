/*
 * File: psched_pthread.c
 *
 * Function: psched_pthread tests posix.1c and posix.1b scheduling semantics
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sched.h>
#include <errno.h>
#include <sched.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

int children = 5;
sem_t sema;

void pchild(void);

main(int argc, char *argv[])
{
  struct sched_param sparams;
  int parse;
  int x;
  pthread_t thread_id;
  char c;

  while ((parse = getopt(argc, argv, "c:")) != EOF)
    switch (parse) {
    case 'c':
      children = atoi(optarg);
      break;
    }

  sem_init(&sema, 1, 0);

  for (x=0; x < children; x++) {

    if ((pthread_create(&thread_id, NULL, (void *) pchild, (void *) 0)) != 0)  {
      perror("psched_pthread: FAILED - pthread_create");
      exit(2);
    }

  }

  printf("sched_setscheduler: FIFO test - %d\n",
	 sched_get_priority_max(SCHED_FIFO));

  sparams.sched_priority = sched_get_priority_max(SCHED_FIFO);
  if (sched_setscheduler(0, SCHED_FIFO, &sparams) < 0) {
    perror("FAILED: sched_setscheduler");
    exit(1);
  }

  printf("check...\n");
  c = getchar();

  printf("sched_setparam: FIFO test - %d\n",
	 sched_get_priority_max(SCHED_FIFO)-10);

  sparams.sched_priority = sched_get_priority_max(SCHED_FIFO)-10;
  if (sched_setparam(0, &sparams) < 0) {
    perror("FAILED: sched_setparam");
    exit(1);
  }

  printf("check...\n");
  c = getchar();

  printf("sched_setscheduler: FIFO test - %d\n",
	 sched_get_priority_min(SCHED_FIFO));

  sparams.sched_priority = sched_get_priority_min(SCHED_FIFO);
  if (sched_setscheduler(0, SCHED_FIFO, &sparams) < 0) {
    perror("FAILED: sched_setscheduler");
    exit(1);
  }

  printf("check...\n");
  c = getchar();

  printf("sched_setscheduler: RR test - %d\n",
	 sched_get_priority_max(SCHED_RR));

  sparams.sched_priority = sched_get_priority_max(SCHED_RR);
  if (sched_setscheduler(0, SCHED_RR, &sparams) < 0) {
    perror("FAILED: sched_setscheduler");
    exit(1);
  }

  printf("check...\n");
  c = getchar();

  printf("sched_setscheduler: RR test - %d\n",
	 sched_get_priority_min(SCHED_RR));

  sparams.sched_priority = sched_get_priority_min(SCHED_RR);
  if (sched_setscheduler(0, SCHED_RR, &sparams) < 0) {
    perror("FAILED: sched_setscheduler");
    exit(1);
  }

  printf("check...\n");
  c = getchar();

  printf("sched_setscheduler: TS test - %d\n",
	 sched_get_priority_max(SCHED_TS));

  sparams.sched_priority = sched_get_priority_max(SCHED_TS);
  if (sched_setscheduler(0, SCHED_TS, &sparams) < 0) {
    perror("FAILED: sched_setscheduler");
    exit(1);
  }

  printf("check...\n");
  c = getchar();

  printf("sched_setscheduler: TS test - %d\n",
	 sched_get_priority_min(SCHED_TS));

  sparams.sched_priority = sched_get_priority_min(SCHED_TS);
  if (sched_setscheduler(0, SCHED_TS, &sparams) < 0) {
    perror("FAILED: sched_setscheduler");
    exit(1);
  }

  printf("check...\n");
  c = getchar();

}


void
pchild(void)
{
  sem_wait(&sema);

  pthread_exit((void *) NULL);
}


