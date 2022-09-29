#include <signal.h>

extern int atcheckpoint(void (*)());
extern int atrestart(void (*)());

int succeeded = 0;

void
ckpt()
{
  succeeded++;
  printf("\n\t*** slp: catching SIGCKPT ***\n\n");
}

void
rest()
{
  succeeded++;
  printf("\n\t*** slp: catching SIGRESTART ***\n\n");
}

main()
{
  pid_t ctest_pid = getppid();
  atcheckpoint(ckpt);
  atrestart(rest);

  printf("\n\t*** slp: PID %d: Before sleeping... ***\n\n", getpid());
  pause();
  printf("\n\t*** slp: Done ***\n\n");
  if (succeeded == 2)
    kill(ctest_pid, SIGUSR1);
  else
    kill(ctest_pid, SIGUSR2);
}
