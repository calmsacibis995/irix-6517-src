#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>

sem_t unnamed_sema1;
sem_t unnamed_sema2;

sem_t *named_sema1;
sem_t *named_sema2;

int shared = 0;
int global_count = 100;
int unnamed_sema_test = 1;

void child();

main(int argc, char *argv[])
{
  int loop;
  int parse;
  
  while ((parse = getopt(argc, argv, "c:n")) != EOF)
    switch (parse) {
    case 'c':
      global_count = atoi(optarg);
      break;
    case 'n':
      unnamed_sema_test = 0;
      break;
    }


  if (unnamed_sema_test) {
    if (sem_init(&unnamed_sema1, 0, 1)) {
      perror("sem_init1");
      exit(1);
    }

    if (sem_init(&unnamed_sema2, 0, 0)) {
      perror("sem_init2");
      exit(1);
    }
  } else {

    named_sema1 = sem_open("/usr/tmp/sema1.posix",
			   O_RDWR|O_CREAT|O_TRUNC,
			   S_IRWXU,
			   1);

    if ((int) named_sema1 == -1) {
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

    sem_unlink("/usr/tmp/sema1.posix");
    sem_unlink("/usr/tmp/sema2.posix");
  }

  if (sproc(child, PR_SALL) == -1) {
    perror("sproc failed");
    exit(1);
  }

  loop = global_count;

  if (unnamed_sema_test) {
    while (loop--) {
      if (sem_wait(&unnamed_sema1)) {
	perror("sem_wait P failed");
	exit(1);
      }
      shared++;
      if (sem_post(&unnamed_sema2)) {
	perror("sem_post P failed");
	exit(1);
      }
    }
  } else {
    while (loop--) {
      if (sem_wait(named_sema1)) {
	perror("sem_wait P failed");
	exit(1);
      }
      shared++;
      if (sem_post(named_sema2)) {
	perror("sem_post P failed");
	exit(1);
      }
    }
  }

  exit(0);
}

void
child()
{
  int loop = global_count;

  if (unnamed_sema_test) {
    while (loop--) {
      if (sem_wait(&unnamed_sema2)) {
	perror("sem_wait C failed");
	exit(1);
      }
      printf("%d\n", shared);
      if (sem_post(&unnamed_sema1)) {
	perror("sem_post C failed");
	exit(1);
      }
    }
  } else {
    while (loop--) {
      if (sem_wait(named_sema2)) {
	perror("sem_wait C failed");
	exit(1);
      }
      printf("%d\n", shared);
      if (sem_post(named_sema1)) {
	perror("sem_post C failed");
	exit(1);
      }
    }
  }

  exit(0);
}
