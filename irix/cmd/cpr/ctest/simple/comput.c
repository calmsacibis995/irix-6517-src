/*
 * Usage: give a big enough loop # and pipe the result to a file.
 * while it's running, checkpoint and checkpoint (check? -AM) the output file.
 * restart the process and make sure the output file has the right data
 * compared to checkpoint time.
 * Tests whether CPR can successfully checkpoint/restart a running
 * process with open files.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>


#define DUMP_PATH "/usr/people/guest/aravind/ctest/comput.dump"
#define CHECK_PATH "/usr/people/guest/aravind/ctest/comput.tmp"

FILE * dumpfile;
FILE * checkfile;

int succeeded = 1;


void
on_checkpoint()
{
  int c;
  FILE * temp = dumpfile;
  rewind(temp);
  while ((c = getc(temp)) != EOF)
    putc(c, checkfile);
}

void
on_restart()
{
  int c;
  
  FILE * temp = dumpfile;
  rewind(temp);
  rewind(checkfile);
  while ((c = getc(checkfile)) != EOF)
    {
      if (c != getc(temp))
	{
	  succeeded = 0;
	  return;
	}
    }
}


main(int argc, char **argv)
{
  pid_t ctest_pid = getppid();
  double a = 1, b, c, x, y;
  int i, loops;
  if ((dumpfile = fopen(DUMP_PATH, "w+")) == NULL)
    {
      perror("comput: fopen");
      exit(1);
    }
  if ((checkfile = fopen(CHECK_PATH, "w+")) == NULL)
    {
      perror("comput: fopen");
      exit(1);
    }
  signal(SIGCKPT, on_checkpoint);
  signal(SIGRESTART, on_restart);
  loops = atol(argv[1]);
  if (loops == 0)
    loops = 500000;
  printf("\n\t*** comput: %d loops... ***\n", loops);
  
  for (i = 0; (i < loops) & succeeded; i++) 
    {
      x = (double)(i/a);
      y = sqrt(x);
      fprintf(dumpfile, "%d: x=%f y=%f\n", i, x, y);
    }
  fclose (dumpfile);
  fclose (checkfile);
  unlink (DUMP_PATH);
  unlink (CHECK_PATH);
  printf("\n\t*** comput: Done ***\n");
  if (succeeded)
    kill(ctest_pid, SIGUSR1);
  else
    kill(ctest_pid, SIGUSR2);
}











































































































































































