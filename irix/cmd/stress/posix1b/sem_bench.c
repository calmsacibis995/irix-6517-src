#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <time.h>
#include <sched.h>
#include <ulocks.h>
#include <sys/mman.h>

#define PRIORITY 0

#define TEST_TYPE_UNNAMED 1
#define TEST_TYPE_NAMED   2
#define TEST_TYPE_ARENA   4

int test_type = TEST_TYPE_UNNAMED;

sem_t unnamed_sema1;
sem_t unnamed_sema2;

sem_t *named_sema1;
sem_t *named_sema2;

usema_t *arena_sema1;
usema_t *arena_sema2;

int shared = 0;
int global_count = 10000;

float total_time;
struct timespec start_stamp, end_stamp;
float block_time;

void unnamed_child(void *);
void named_child(void *);
void arena_child(void *);

main(int argc, char *argv[])
{
  int loop;
  int parse;
  usptr_t *hdr;

  while ((parse = getopt(argc, argv, "c:anu")) != EOF)
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
    }

#if PRIORITY
  {
  struct  sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
    perror("sched_setscheduler");              
    exit(0);                                           
  }
  }
#endif

  if (mlockall(MCL_CURRENT|MCL_FUTURE)) {
    perror("mlockall");
    exit(1);
  }

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

    printf ("sem_bench: posix UNNAMED semaphore test...\n");

    if (sproc(unnamed_child, PR_SALL) == -1) {
      perror("sproc");
      exit(1);
    }
    break;

  case TEST_TYPE_NAMED:
    named_sema1 = sem_open("/usr/tmp/sema1.posix",
			   O_RDWR|O_CREAT|O_TRUNC,
			   S_IRWXU,
			   1);

    if (named_sema1 == (sem_t *)-1L) {
      perror("sem_open1");
      exit(1);
    }

    named_sema2 = sem_open("/usr/tmp/sema2.posix",
			   O_RDWR|O_CREAT|O_TRUNC,
			   S_IRWXU,
			   0);

    if (named_sema2 == (sem_t *)-1L) {
      perror("sem_open2");
      exit(1);
    }

    sem_unlink("/usr/tmp/sema1.posix");
    sem_unlink("/usr/tmp/sema2.posix");

    printf ("sem_bench: posix NAMED semaphore test...\n");

    if (sproc(named_child, PR_SALL) == -1) {
      perror("sproc failed");
      exit(1);
    }
    break;

  case TEST_TYPE_ARENA:
    usconfig(CONF_ARENATYPE, US_SHAREDONLY);
    usconfig(CONF_INITUSERS, 2);

    if ((hdr = usinit("/usr/tmp/sem_bench_arena.arena")) == NULL) {
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

    printf ("sem_bench: irix ARENA semaphore test...\n");

    if (sproc(arena_child, PR_SALL) == -1) {
      perror("sproc failed");
      exit(1);
    }
  }

  loop = global_count/2;

  if ((clock_gettime(CLOCK_REALTIME, &start_stamp)) == -1) {
     perror("clock_gettime");
     exit(1);
  }

  switch (test_type) {
  case TEST_TYPE_UNNAMED:
    while (loop--) {
      sem_wait(&unnamed_sema1);
      sem_post(&unnamed_sema2);
    }
    break;
  case TEST_TYPE_NAMED:
    while (loop--) {
      sem_wait(named_sema1);
      sem_post(named_sema2);
    }
    break;
  case TEST_TYPE_ARENA:
    while (loop--) {
      uspsema(arena_sema1);
      usvsema(arena_sema2);
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
    end_stamp.tv_nsec = (end_stamp.tv_nsec - start_stamp.tv_nsec) + 1000000000;
  }

  printf("\t%d semaphore blocks took %u secs %u usecs\n",
	 global_count, end_stamp.tv_sec, end_stamp.tv_nsec/1000);

  total_time = (float)(end_stamp.tv_sec * 1000000 + end_stamp.tv_nsec/1000);
  block_time = (total_time / ((float) global_count));
  printf("\t%5.1f uS per block\n", block_time);

  return(0);
}

/* ARGSUSED */
void
unnamed_child(void *arg)
{
  int loop = global_count/2;

#if PRIORITY
  {
  struct sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
    perror("sched_setscheduler");              
    exit(0);                                           
  }
  }
#endif

  while (loop--) {
    sem_wait(&unnamed_sema2);
    sem_post(&unnamed_sema1);
  }

  exit(0);
}

/* ARGSUSED */
void
named_child(void *arg)
{
  int loop = global_count/2;

#if PRIORITY
  {
  struct sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
    perror("sched_setscheduler");              
    exit(0);                                           
  }
  }
#endif

  while (loop--) {
    sem_wait(named_sema2);
    sem_post(named_sema1);
  }

  exit(0);
}

/* ARGSUSED */
void
arena_child(void *arg)
{
  int loop = global_count/2;

#if PRIORITY
  {
  struct sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
    perror("sched_setscheduler");              
    exit(0);                                           
  }
  }
#endif

  while (loop--) {
    uspsema(arena_sema2);
    usvsema(arena_sema1);
  }

  exit(0);
}
