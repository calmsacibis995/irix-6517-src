#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define SEM_MAX 100
#define SEM_WAITER 10
#define SEM_SIGNAL 80

#define POST_ME 47

int shm_fd;
int mem_size;
sem_t *sem_array;

void child(void);

main()
{
  int x, y, pid;

  shm_fd = shm_open ("/usr/tmp/memory.shm", O_CREAT | O_RDWR , 0666);
  mem_size = SEM_MAX * sizeof(sem_t);

  if (ftruncate(shm_fd, mem_size) < 0) {
    perror("FAILED: cannot grow shared memory file via ftruncate");
    exit(1);
  }

  sem_array = (sem_t *) mmap(0, mem_size, PROT_READ|PROT_WRITE,
			     MAP_SHARED, shm_fd, 0);

  shm_unlink("/usr/tmp/memory.shm");

  for (x=0; x<SEM_MAX; x++) {
    if (x == SEM_WAITER)
      sem_init(&sem_array[x], 0, 0);
    else
      sem_init(&sem_array[x], 0, 1);
  }

  /* fork child */
  if ((pid = fork()) == -1) {
    perror("FAILED: child fork failure");
    exit(1);
  }

  if (pid == 0) {
    child();
    exit(0);
  }

  if (sem_wait(&sem_array[SEM_WAITER]) < 0)
    perror("FAILED: sem_wait");

  if (sem_getvalue(&sem_array[SEM_SIGNAL], &y) < 0) {
    perror("FAILED: parent cannot perform sem_getvalue");
    exit(1);
  }

  if (y != (POST_ME+1))
    printf("FAILED: interprocess semaphore usage failed (y=%d)\n", y);
  else
    printf("PASSED: interprocess unnamed semaphore sharing succeeded\n");

  exit(0);
}

void
child(void)
{
  int x;
  int y;
  int signal_sema = POST_ME;

  for (x=0; x<SEM_MAX; x++) {
    if (sem_getvalue(&sem_array[x], &y) < 0) {
      perror("FAILED: child sem_getvalue");
      exit(1);
    }
    if (x != SEM_WAITER && y != 1)
      printf("FAILED: child retrieved incorrect semaphore values\n");
  }

  while(signal_sema--)
    if (sem_post(&sem_array[SEM_SIGNAL]) < 0)
      perror("FAILED: child cannot post unnamed semaphore");

  if (sem_post(&sem_array[SEM_WAITER]) < 0)
    perror("FAILED: child cannot post unnamed semaphore");

  exit(0);
}
  
  
