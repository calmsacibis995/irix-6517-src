#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <sys/schedctl.h>

int prio_reserve[] = {{35},{34},{33},{32},{31}};

int prio_index = 0;
int prio_size = sizeof(prio_reserve)/sizeof(int);

sem_t prio_sema;
sem_t block_sema;
sem_t wake_parent;

int children = 5;

void child(void *, size_t);

main(int argc, char *argv[])
{
  int parse;
  int x;

  while ((parse = getopt(argc, argv, "c:")) != EOF)
    switch (parse) {
    case 'c':
      children = atoi(optarg);
      break;
    }

  sem_init(&block_sema, 0, 0);
  sem_init(&wake_parent, 0, 0);
  sem_init(&prio_sema, 0, 1);

  for (x=0; x < children; x++) {
    if (sprocsp(child, PR_SALL, 0, NULL, 8192) == -1) {
      perror("sem_prio: ERROR - sproc failed");
      printf("sem_prio: child %d\n", x);
      exit(1);
    }
  }

  sleep(2);

  sem_post(&block_sema);

  sem_wait(&wake_parent);

  return(0);
}

/* ARGSUSED */
void
child(void *arg, size_t len)
{
  pid_t pid = getpid();
  int my_prio;

  sem_wait(&prio_sema);
  my_prio = prio_reserve[prio_index];
  prio_index = (prio_index+1) % prio_size;
  sem_post(&prio_sema);

  schedctl(NDPRI, 0, my_prio);

  my_prio = (int)schedctl(GETNDPRI, 0);
  printf ("sem_prio: pid=%d prio=%d\n",pid, my_prio);

  sem_wait(&block_sema);

  printf ("sem_prio: WAKE: pid=%d prio=%d\n",pid, my_prio);
  if (--children == 0)
    sem_post(&wake_parent);
  else
    sem_post(&block_sema);

  exit(0);
}
