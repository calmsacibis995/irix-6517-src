/* NOTES: Tests whether a delayed event is preserved across
a checkpoint. SIGCKPT should precede the delayed SIGALRM,
eheckpointing the process. After restart, the delayed SIGALRM
should still be remembered, sent, and received. Test succeeds
if SIGCKPT and SIGALRM are both received, with SIGCKPT
preceeding SIGALRM.*/

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
  printf("\n\t*** alarm: SIGALRM sent and received ***\n\n");
  succeeded++;
}

void
on_checkpoint()                            /* NEW */
{
  printf("\n\t*** alarm: SIGCKPT received ***\n\n");
  if (!succeeded)
    succeeded++;
}

main(int argc, char **argv)
{
  pid_t ctest_pid = getppid();
  signal(SIGCKPT, on_checkpoint);
  signal(SIGALRM, catch);
  alarm(4);
  pause();    /* To be broken by SIGCKPT */ 
  printf("parent: %d", getppid());
  pause();    /* To be broken by SIGALRM after checkpoint */
  printf("\n\t*** alarm: Done ***\n");
  if (succeeded == 2)
    kill(ctest_pid, SIGUSR1);
  else
    kill(ctest_pid, SIGUSR2);
}


