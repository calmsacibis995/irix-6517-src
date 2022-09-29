#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>

sem_t *named_sema[100];

main(int argc, char *argv[])
{
  int parse;
  int x;
  int unix_unlink = 0;
  int pause_here = 0;
  int ignore_unlink = 0;
  char sem_name[50];
  char c;


  while ((parse = getopt(argc, argv, "upi")) != EOF)
    switch (parse) {
    case 'u':
      unix_unlink = 1;
      break;
    case 'p':
      pause_here = 1;
      break;
    case 'i':
      ignore_unlink = 1;
      break;
    }

  for (x=0; x < 100; x++) {
    sprintf(sem_name, "/usr/tmp/semaphore%d.posix", x);
    named_sema[x] = sem_open(sem_name,
			   O_RDWR|O_CREAT|O_TRUNC,
			   S_IRWXU,
			   1);
    
    if (named_sema[x] == SEM_FAILED) {
      perror("sem_create_destroy: FAILED sem_open");
      exit(1);
    }
  }

  if (pause_here)
    c = getchar();

  for (x=0; x < 100; x++) {
    if (sem_post(named_sema[x])) {
      perror("sem_create_destroy: FAILED sem_post");
      exit(1);
    }
  }

  if (ignore_unlink == 0) {
    for (x=0; x < 100; x++) {
      sprintf(sem_name, "/usr/tmp/semaphore%d.posix", x);
      if (unix_unlink) {
	if (unlink(sem_name)) {
	  perror("sem_create_destroy: FAILED unlink");
	  exit(1);
	}
      } else {
	if (sem_unlink(sem_name)) {
	  perror("sem_create_destroy: FAILED sem_unlink");
	  exit(1);
	}
      }
    }
  }

  for (x=0; x < 100; x++) {
    if (sem_close(named_sema[x])) {
      perror("sem_create_destroy: FAILED sem_close");
      exit(1);
    }
  }

  printf("sem_create_destroy: PASSED\n");
  exit(0);
}
