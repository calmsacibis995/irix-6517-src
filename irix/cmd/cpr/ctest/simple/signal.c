
/*
 * Usage: signal child_sig parent_sig delay
 */
/* I'm not certain how this test is supposed to be carried
out, exactly. The test spawns a child process and plays signal
tag with it. Are both the child and parent supposed to be
checkpointed? If so, are they supposed to be checkpointed
while they are "paused" or "asleep"? If the test is fed
into the scaffolding as it stands (with a minor adjustment
I made to terminate the looping), the child will send a signal
to a parent who is most likely checkpointed. Thus, the signal will
be sent to a nonexistent PID. After restart, though, the activity
should resume.*/



#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <ckpt.h>

void child_act(int sig, int code, struct sigcontext *sc);
void parent_act(int sig, int code, struct sigcontext *sc);

int sig1, sig2;
int switchsig = SIGCKPT;

void
on_checkpoint()
{;}

void
on_restart()
{
  switchsig = sig2;
  sigpause(sig2);
}

int count = 0;

main(int argc, char **argv)
{
  pid_t ctest_pid = getppid();
  int delay = 1;
  pid_t child;
  sig1 = atol(argv[1]);
  sig2 = atol(argv[2]);
  delay = atol(argv[3]);
  
  if (sig1 == 0 || sig2 == 0) 
    {
      sig1 = SIGUSR1;
      sig2 = SIGUSR2;
      delay = 10;
    }
  if ((child = fork()) == 0) 
    {
    loop1:
      sleep(delay);
      signal(sig1, child_act);
      kill(getppid(), sig2);
      sigpause(sig1);
      if (count == 5)
	goto end2;
      goto loop1;
    }
  if (child == -1) 
    {
      perror("signal: fork");
      exit(-1);
    }
loop2:
  signal(SIGCKPT, on_checkpoint);
  signal(SIGRESTART, on_restart);
  signal(sig2, parent_act);
  if (count == 5) 
    goto end1;
  sigpause(switchsig);
  sleep(delay);
  kill(child, sig1);
  goto loop2;
end1:
  waitpid(child, &count, 0);
  printf("\n\t***signal: Done ***\n");
  kill(ctest_pid, SIGUSR1);
end2:;
}

void
child_act(int sig, int code, struct sigcontext *sc)
{
  int tval = time(0) & 0xfff;
  printf("\t*** %d: child  catching signal %d at %d ***\n", ++count, sig, tval);
}

void
parent_act(int sig, int code, struct sigcontext *sc)
{
  int tval = time(0) & 0xfff;
  printf("\t*** %d: parent catching signal %d at %d ***\n", ++count, sig, tval);
}





















