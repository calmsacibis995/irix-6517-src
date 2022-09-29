
/* Tests whether a signal held in a process's
signal mask will propagate through a checkpoint. It does
so by generating a SIGALRM, holding the SIGALRM in the mask,
checkpoint/restarting, and the releasing the signal from the
mask. Test succeeds if SIGALRM is received.*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>


int succeeded = 0;

void
catch()
{
  printf("\n\t*** alarm2: SIGALRM received ***\n\n");
  succeeded++;
}

void
on_checkpoint()
{
  printf("\n\t*** alarm2: SIGCKPT received ***\n\n");
}

main(int argc, char **argv)
{
  pid_t ctest_pid = getppid();
  signal(SIGCKPT, on_checkpoint);
  signal(SIGALRM, catch);
  sighold(SIGALRM);
  kill(getpid(), SIGALRM);
  printf("\n\t*** alarm2: SIGALARM sent ***\n\n");
  pause();
  sigrelse(SIGALRM);
  printf("\n\t*** alarm2: Done ***\n");
  if (succeeded)
    kill(ctest_pid, SIGUSR1);
  else
    kill(ctest_pid, SIGUSR2);
}








